#include "wdt_monitor.h"
#include "rtos.h"
#include "logger.h"
#include "wdt.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_WDT_TASKS               (10u)                 ///< 監視できるタスクの最大数。登録するのは2タスク
#define WDT_HARDWARE_TIMEOUT_MS     (10000u)              ///< ハードウェアWDTのタイムアウト。タスクの上限5s+確認間隔1sより長く、RP2350の上限約16.7s以内
#define WDT_CHECK_INTERVAL_MS       (1000u)               ///< タスクを確認する間隔
#define WDT_MONITOR_PRIORITY        (RTOS_PRIORITY_MAX)   ///< 監視タスクの優先度。監視が止められないように最高にする
#define WDT_MONITOR_STACK_WORDS     (1536u)               ///< 監視タスクのスタックサイズ（ワード）
#define WDT_MUTEX_TIMEOUT_MS        (50u)                 ///< ミューテックスを待つ時間の上限。タスクの処理中の上限200msより短くする
#define WDT_MUTEX_FAIL_THRESHOLD    (3u)                  ///< ミューテックスの取得失敗がこの回数続いたら異常とみなす
#define WDT_START_TIMEOUT_MS        (30000u)              ///< 起動からこの時間内に監視が開始されなければ異常とみなす。起動タスクの待ち5sより長くする

#define WDT_PANIC_MAGIC             (0x57445450u)         ///< 異常の記録が有効であることを表す値。ASCIIの"WDTP"
#define WDT_PANIC_NAME_LEN          (16u)                 ///< 異常の記録に残すタスク名の長さ（終端を含む）
#define WDT_PANIC_STATE_NONE        (0xFFFFFFFFu)         ///< 異常の記録で、タスクの状態が無いことを表す値

/** 監視対象のタスク1つ分の情報。 */
typedef struct {
    rtos_task_handle_t task_handle;           ///< 登録したタスク
    const char        *name;                  ///< タスク名（ログ用）
    wdt_task_state_t   state;                 ///< 現在の状態
    uint32_t           last_feed_tick;        ///< 最後に状態を更新した時刻（tick）
    uint32_t           limit_running_ticks;   ///< 処理中の上限（tick）
    uint32_t           limit_waiting_ticks;   ///< 待ち中の上限（tick）
    bool               is_registered;         ///< このスロットが使用中か
} wdt_task_context_t;

/** 異常でリセットする前に残す記録。再起動後に読み出してログに出す。 */
typedef struct {
    uint32_t magic;                 ///< 記録が有効ならWDT_PANIC_MAGIC
    uint32_t fault_count;           ///< 異常の回数
    uint32_t tick;                  ///< 異常を検知した時刻（tick）
    uint32_t slots_bitmap;          ///< 異常だったスロットのビット列
    char     first_name[WDT_PANIC_NAME_LEN];      ///< 最初に見つかった異常タスクの名前
    uint32_t first_state;           ///< 最初に見つかった異常タスクの状態
    uint32_t first_elapsed_ticks;   ///< 最初に見つかった異常タスクの経過時間（tick）
} wdt_panic_record_t;

static wdt_task_context_t s_tasks[MAX_WDT_TASKS];   ///< 監視対象のタスクの一覧
static rtos_mutex_t       s_mutex          = NULL;    ///< s_tasksを守るミューテックス
static volatile bool      s_started        = false;   ///< 監視を開始したか
static uint32_t           s_mutex_fail_cnt = 0u;      ///< ミューテックスの取得に連続で失敗した回数

static __attribute__((section(".noinit"))) wdt_panic_record_t s_panic;   ///< リセットしても消えない異常の記録

/** 異常を検知したときの既定の処理。システムをリセットする。 */
__attribute__((weak)) void wdt_on_fault(void) {
    wdt_reboot();
}

/** ミューテックスを取得する。失敗した回数を数える。 */
static bool take_lock(uint32_t timeout_ms) {
    if (rtos_mutex_take(s_mutex, timeout_ms)) {
        s_mutex_fail_cnt = 0u;
        return true;
    }
    s_mutex_fail_cnt++;
    return false;
}

/** タスクの現在の状態に応じた上限（tick）を返す。 */
static uint32_t limit_for_state(const wdt_task_context_t *slot) {
    return (slot->state == WDT_TASK_RUNNING) ? slot->limit_running_ticks
                                             : slot->limit_waiting_ticks;
}

/** リセットしても消えない領域に、異常の記録を残す。長すぎる名前は切り詰める。 */
static void record_panic(uint32_t tick, uint32_t slots_bitmap, const char *first_name,
                         uint32_t first_state, uint32_t first_elapsed_ticks) {
    s_panic.magic               = WDT_PANIC_MAGIC;
    s_panic.fault_count++;
    s_panic.tick                = tick;
    s_panic.slots_bitmap        = slots_bitmap;
    s_panic.first_state         = first_state;
    s_panic.first_elapsed_ticks = first_elapsed_ticks;
    (void)strncpy(s_panic.first_name, first_name, sizeof(s_panic.first_name) - 1u);
    s_panic.first_name[sizeof(s_panic.first_name) - 1u] = '\0';
}

static void monitor_task(void *pvParameters);

bool wdt_monitor_init(void) {
    (void)memset(s_tasks, 0, sizeof(s_tasks));

    s_mutex = rtos_mutex_create();
    if (s_mutex == NULL) {
        LOG_ERROR("Failed to create WDT mutex!");
        return false;
    }

    if (!rtos_task_create(monitor_task, "WDT_Mon", WDT_MONITOR_STACK_WORDS, NULL, WDT_MONITOR_PRIORITY, NULL)) {
        LOG_ERROR("Failed to create WDT monitor task!");
        return false;
    }

    if (s_panic.magic == WDT_PANIC_MAGIC) {
        LOG_ERROR("Previous boot ended in WDT fault: task=%s state=%lu "
                  "elapsed=%lu slots=0x%08lx (count=%lu)",
                  s_panic.first_name,
                  (unsigned long)s_panic.first_state,
                  (unsigned long)s_panic.first_elapsed_ticks,
                  (unsigned long)s_panic.slots_bitmap,
                  (unsigned long)s_panic.fault_count);
        s_panic.magic = 0u;
    } else {
        (void)memset(&s_panic, 0, sizeof(s_panic));
    }

    LOG_INFO("WDT Monitor initialized (not started yet).");
    return true;
}

bool wdt_monitor_start(uint32_t expected_task_count) {
    uint32_t registered = 0u;

    if (!take_lock(WDT_MUTEX_TIMEOUT_MS)) {
        LOG_ERROR("wdt_monitor_start: mutex timeout");
        return false;
    }
    for (uint32_t i = 0u; i < MAX_WDT_TASKS; i++) {
        if (s_tasks[i].is_registered) {
            registered++;
        }
    }
    rtos_mutex_give(s_mutex);

    if (registered != expected_task_count) {
        LOG_DEBUG("wdt_monitor_start: expected %lu tasks but %lu registered",
                  (unsigned long)expected_task_count, (unsigned long)registered);
        return false;
    }

    if (!wdt_init(WDT_HARDWARE_TIMEOUT_MS)) {
        LOG_ERROR("wdt_monitor_start: hardware WDT init failed (%u ms)", (unsigned)WDT_HARDWARE_TIMEOUT_MS);
        return false;
    }
    s_started = true;
    LOG_INFO("WDT started with %lu tasks.", (unsigned long)registered);
    return true;
}

uint32_t wdt_register_task(const char *name, uint32_t running_timeout_ms, uint32_t waiting_timeout_ms) {
    uint32_t assigned_id = WDT_INVALID_ID;

    if (!take_lock(WDT_MUTEX_TIMEOUT_MS)) {
        LOG_ERROR("wdt_register_task: mutex timeout");
        return WDT_INVALID_ID;
    }

    for (uint32_t i = 0u; i < MAX_WDT_TASKS; i++) {
        if (!s_tasks[i].is_registered) {
            s_tasks[i].task_handle         = rtos_task_get_current_handle();
            s_tasks[i].name                = name;
            s_tasks[i].state               = WDT_TASK_RUNNING;
            s_tasks[i].last_feed_tick      = rtos_task_get_tick_count();
            s_tasks[i].limit_running_ticks = rtos_ms_to_ticks(running_timeout_ms);
            s_tasks[i].limit_waiting_ticks = rtos_ms_to_ticks(waiting_timeout_ms);
            s_tasks[i].is_registered       = true;
            assigned_id = i;
            break;
        }
    }
    rtos_mutex_give(s_mutex);

    if (assigned_id == WDT_INVALID_ID) {
        LOG_ERROR("WDT registration failed for '%s': no empty slots (MAX_WDT_TASKS=%u)", (name != NULL) ? name : "?", (unsigned)MAX_WDT_TASKS);
    } else {
        LOG_INFO("Task '%s' registered to WDT slot %lu", (name != NULL) ? name : "?", (unsigned long)assigned_id);
    }
    return assigned_id;
}

void wdt_task_set_state(uint32_t wdt_id, wdt_task_state_t state) {
    bool owner_mismatch = false;

    if (wdt_id >= MAX_WDT_TASKS) {
        return;
    }
    if (!take_lock(WDT_MUTEX_TIMEOUT_MS)) {
        if (s_mutex_fail_cnt >= WDT_MUTEX_FAIL_THRESHOLD) {
            LOG_WARN("wdt_task_set_state: mutex contention (%lu consecutive)", (unsigned long)s_mutex_fail_cnt);
        }
        return;
    }

    if (s_tasks[wdt_id].is_registered) {
        if (s_tasks[wdt_id].task_handle == rtos_task_get_current_handle()) {
            s_tasks[wdt_id].state          = state;
            s_tasks[wdt_id].last_feed_tick = rtos_task_get_tick_count();
        } else {
            owner_mismatch = true;
        }
    }
    rtos_mutex_give(s_mutex);
    if (owner_mismatch) {
        LOG_ERROR("WDT slot %lu fed by wrong task! ID mix-up in caller.", (unsigned long)wdt_id);
    }
}

/** 登録されたタスクを一定間隔で確認し、全タスクが正常ならハードウェアWDTを更新する監視タスク。 */
static void monitor_task(void *pvParameters) {
    (void)pvParameters;

    uint32_t last_wake_time = rtos_task_get_tick_count();
    const uint32_t start_deadline =
        last_wake_time + rtos_ms_to_ticks(WDT_START_TIMEOUT_MS);

    while (true) {
        uint32_t fault_bitmap   = 0u;
        uint32_t fault_count    = 0u;
        uint32_t first_slot     = 0u;
        uint32_t first_state    = 0u;
        uint32_t first_elapsed  = 0u;
        const char *first_name  = "?";
        bool lock_failed        = false;

        const uint32_t current_tick = rtos_task_get_tick_count();

        if (!s_started) {
            if ((int32_t)(current_tick - start_deadline) >= 0) {
                LOG_ERROR("WDT was never started within %u ms. Init failure.", (unsigned)WDT_START_TIMEOUT_MS);
                wdt_on_fault();
            }
            rtos_task_delay_until(&last_wake_time, WDT_CHECK_INTERVAL_MS);
            continue;
        }

        if (take_lock(WDT_MUTEX_TIMEOUT_MS)) {
            for (uint32_t i = 0u; i < MAX_WDT_TASKS; i++) {
                if (!s_tasks[i].is_registered) {
                    continue;
                }
                const uint32_t limit = limit_for_state(&s_tasks[i]);
                const uint32_t elapsed = current_tick - s_tasks[i].last_feed_tick;
                if (elapsed > limit) {
                    if (fault_count == 0u) {
                        first_slot    = i;
                        first_state   = (uint32_t)s_tasks[i].state;
                        first_elapsed = elapsed;
                        first_name    = (s_tasks[i].name != NULL) ? s_tasks[i].name : "?";
                    }
                    fault_bitmap |= (1u << i);
                    fault_count++;
                }
            }
            rtos_mutex_give(s_mutex);
        } else {
            lock_failed = true;
        }

        if (lock_failed) {
            LOG_WARN("WDT monitor: mutex timeout (%lu consecutive)", (unsigned long)s_mutex_fail_cnt);
            if (s_mutex_fail_cnt >= WDT_MUTEX_FAIL_THRESHOLD) {
                LOG_ERROR("WDT mutex held too long. Treating as fault.");
                record_panic(current_tick, 0u, "MUTEX", WDT_PANIC_STATE_NONE, 0u);
                wdt_on_fault();
            }
        } else if (fault_count == 0u) {
            wdt_feed();
        } else {
            record_panic(current_tick, fault_bitmap, first_name, first_state, first_elapsed);

            LOG_ERROR("WDT TIMEOUT: %lu task(s) frozen, bitmap=0x%08lx", (unsigned long)fault_count, (unsigned long)fault_bitmap);
            LOG_ERROR("  first: slot=%lu name=%s state=%lu elapsed=%lu ticks",
                      (unsigned long)first_slot, first_name,
                      (unsigned long)first_state, (unsigned long)first_elapsed);

            wdt_on_fault();
        }

        rtos_task_delay_until(&last_wake_time, WDT_CHECK_INTERVAL_MS);
    }
}
