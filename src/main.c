#include "rtos.h"
#include "logger.h"
#include "sys.h"
#include "control.h"
#include "network.h"
#include "wdt_monitor.h"
#include "wdt.h"

#define TASK_PRIO_STARTUP    (RTOS_PRIORITY_HIGH)     ///< 起動タスクの優先度
#define TASK_PRIO_NETWORK    (RTOS_PRIORITY_NORMAL)   ///< ネットワークタスクの優先度
#define TASK_PRIO_CONTROL    (RTOS_PRIORITY_NORMAL)   ///< 空調制御タスクの優先度
#define TASK_STACK_STARTUP   (1024u)                  ///< 起動タスクのスタックサイズ（ワード）
#define TASK_STACK_NETWORK   (4096u)                  ///< ネットワークタスクのスタックサイズ（ワード）。cJSONと書式付き出力を使うため最大
#define TASK_STACK_CONTROL   (2048u)                  ///< 空調制御タスクのスタックサイズ（ワード）
#define EXPECTED_WDT_TASKS   (2u)                     ///< WDTに登録されるタスクの数（NetworkとControl）
#define WDT_START_POLL_MS    (100u)                   ///< タスクのWDT登録を確認する間隔
#define WDT_START_POLL_COUNT (50u)                    ///< タスクのWDT登録を確認する回数。間隔と合わせて5s待つ
#define BOOT_WAIT_MS         (5000u)                  ///< 起動直後のログを取りこぼさないよう、シリアルモニタの接続を待つ時間
#define FATAL_FLUSH_MS       (1000u)                  ///< リセット前に、ログを送り切るまで待つ時間

/** 致命的なエラーをログに出し、システムをリセットする。 */
static void fatal(const char *msg) {
    LOG_ERROR("FATAL: %s", msg);
    sys_busy_wait_ms(FATAL_FLUSH_MS);
    wdt_reboot();
}

/** アプリのタスクを生成し、全タスクがWDTに登録されたら監視を開始して、自身を削除する。 */
static void startup_task(void *pv) {
    (void)pv;
    LOG_INFO("Creating application tasks...");
    if (!rtos_task_create(network_task, "Network", TASK_STACK_NETWORK, NULL, TASK_PRIO_NETWORK, NULL)) {
        fatal("network task creation failed");
    }
    if (!rtos_task_create(control_task, "Control", TASK_STACK_CONTROL, NULL, TASK_PRIO_CONTROL, NULL)) {
        fatal("control task creation failed");
    }
    for (uint8_t i = 0u; i < WDT_START_POLL_COUNT; i++) {
        rtos_delay_ms(WDT_START_POLL_MS);
        if (wdt_monitor_start(EXPECTED_WDT_TASKS)) {
            LOG_INFO("WDT monitoring started. All %u tasks registered.",
                     (unsigned)EXPECTED_WDT_TASKS);
            rtos_task_delete_current();
            return;
        }
    }
    fatal("tasks did not register with WDT in time");
}

/** キューと監視の準備をして起動タスクを作り、スケジューラを開始する。 */
int main(void) {
    sys_stdio_init();
    sys_busy_wait_ms(BOOT_WAIT_MS);
    LOG_INFO("=========================================");
    LOG_INFO(" SmartRemocon");
    LOG_INFO("=========================================");
    if (wdt_caused_reboot()) {
        LOG_WARN("Previous boot ended in a watchdog reset.");
    }
    if (!control_init()) {
        fatal("control init failed");
    }
    if (!network_init()) {
        fatal("network init failed");
    }
    if (!wdt_monitor_init()) {
        fatal("WDT monitor init failed");
    }
    if (!rtos_task_create(startup_task, "Startup", TASK_STACK_STARTUP, NULL, TASK_PRIO_STARTUP, NULL)) {
        fatal("startup task creation failed");
    }
    LOG_INFO("Starting scheduler...");
    rtos_start_scheduler();
    fatal("scheduler returned (out of heap?)");
}
