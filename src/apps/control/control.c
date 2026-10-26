#include "control.h"
#include "temp_filter.h"
#include "control_fsm.h"
#include "logger.h"
#include "rtos.h"
#include "ac_ir.h"
#include "network.h"
#include "bme280.h"
#include "wdt_monitor.h"
#include "sys.h"

#include <string.h>

#define SENSOR_INTERVAL_MS             (TEMP_SAMPLE_PERIOD_MS)   ///< 測定の間隔
#define SENSOR_FAIL_THRESHOLD          (3u)                      ///< 読み取りの失敗がこの回数続いたらセンサー異常とする。
#define SENSOR_RETRY_INTERVAL_MS       (30000u)                  ///< センサー異常のとき、復帰を試みる間隔。
#define SENSOR_INIT_RETRIES            (3u)                      ///< 起動時にセンサーの初期化を試みる回数。
#define SENSOR_INIT_RETRY_MS           (500u)                    ///< 起動時の初期化を再試行する間隔。3回試しても合計約1.5秒で、WDTの登録待ち5秒に収まる
#define CONTROL_WAIT_SLICE_MS          (1000u)                   ///< 1回の待ちの最大時間。
#define IR_IDLE_WAIT_MAX_MS            (500u)                    ///< 運転操作の前に、前回のIR送信の完了を待つ時間の上限。1回の信号は約290ms。
#define IR_IDLE_POLL_MS                (10u)                     ///< IR送信の完了を確認する間隔。
#define CONTROL_WDT_RUNNING_LIMIT_MS   (200u)                    ///< WDTの処理中の上限。
#define CONTROL_WDT_WAITING_LIMIT_MS   (5000u)                   ///< WDTの待ち中の上限。
#define COMMAND_QUEUE_LEN              (1u)                      ///< 運転操作のキューの長さ。毎回全項目を含むので最新の1件だけでよい

/** controlタスクの作業用の情報。 */
typedef struct {
    ac_state_t      ir_state;             ///< 最後に送信に成功したIRの内容
    temp_filter_t   filter;               ///< 温度のフィルタ
    control_fsm_t   fsm;                  ///< 状態遷移の現在の状態と設定
    uint32_t        wdt_id;               ///< WDTの登録ID
    uint32_t        next_measure_ms;      ///< 次に測定する時刻
    uint8_t         sensor_fail_streak;   ///< 読み取りに連続で失敗した回数
    uint32_t        last_retry_ms;        ///< 最後にセンサーの復帰を試みた時刻
} control_ctx_t;

static queue_t s_command_queue = NULL;   ///< 受け取った運転操作のキュー

bool control_init(void) {
    if (s_command_queue == NULL) {
        s_command_queue = rtos_queue_create(COMMAND_QUEUE_LEN, sizeof(remote_command_t));
    }
    return s_command_queue != NULL;
}

bool control_post_command(const remote_command_t *cmd) {
    if ((s_command_queue == NULL) || (cmd == NULL)) {
        return false;
    }
    return rtos_queue_overwrite(s_command_queue, cmd);
}

/** 状態遷移が決めた運転モードを、IR信号で送る運転モードの値に変換する。 */
static ac_mode_t to_ir_mode(control_mode_t m) {
    switch (m) {
    case CONTROL_MODE_HEAT: return AC_MODE_HEAT;
    case CONTROL_MODE_COOL:
    default:                return AC_MODE_COOL;
    }
}

/** PCから受けた運転モード（IR信号の値）を、状態遷移で使う運転モードに変換する。 */
static control_mode_t to_fsm_mode(ac_mode_t m) {
    return (m == AC_MODE_HEAT) ? CONTROL_MODE_HEAT : CONTROL_MODE_COOL;
}

/** IRを送信し、成功したら状態をネットワークタスクへ報告する。 */
static bool send_ir(const ac_state_t *st) {
    if (!ac_ir_send(st)) {
        return false;
    }
    if (!network_report_ac_state(st)) {
        LOG_WARN("AC state report failed");
    }
    return true;
}

/** IRを送信し、成功したときだけ最後に送ったIRの内容を更新する。 */
static bool apply_ir(control_ctx_t *ctx, const ac_state_t *next) {
    if (!send_ir(next)) {
        return false;
    }
    ctx->ir_state = *next;
    return true;
}

/** 判断の結果に従ってIRを送信する。true:送信成功、または何もしない、false:送信失敗 */
static bool apply_decision(control_ctx_t *ctx, const control_decision_t *d) {
    ac_state_t next = ctx->ir_state;

    switch (d->action) {
    case CONTROL_ACTION_NONE:
        return true;

    case CONTROL_ACTION_SET:
        next.power_on    = true;
        next.mode        = to_ir_mode(d->mode);
        next.temperature = d->setpoint_c;
        LOG_INFO("AC SET %s %d C (%s)", control_fsm_mode_name(d->mode), (int)d->setpoint_c, d->reason);
        return apply_ir(ctx, &next);

    case CONTROL_ACTION_POWER_OFF:
        next.power_on = false;
        LOG_INFO("AC OFF (%s)", d->reason);
        return apply_ir(ctx, &next);

    default:
        LOG_WARN("Unknown AC action %d", (int)d->action);
        return true;
    }
}

/** 前回のIR送信が終わるまで、上限の時間まで待つ。 */
static void wait_ir_idle(const control_ctx_t *ctx) {
    uint32_t deadline;

    if (!ac_ir_is_busy()) {
        return;
    }
    deadline = sys_deadline_from_now(IR_IDLE_WAIT_MAX_MS);
    wdt_task_set_state(ctx->wdt_id, WDT_TASK_WAITING);
    while (ac_ir_is_busy() && !sys_deadline_expired(sys_get_time_ms(), deadline)) {
        rtos_delay_ms(IR_IDLE_POLL_MS);
    }
    wdt_task_set_state(ctx->wdt_id, WDT_TASK_RUNNING);
}

/** PCからの運転操作を状態遷移に伝えてから、IRで送信する。送信に失敗したら状態遷移を操作の前に戻す。 */
static void process_remote_command(control_ctx_t *ctx, const remote_command_t *cmd, uint32_t now_ms) {
    control_fsm_t saved;
    ac_state_t next;
    bool sent = false;

    wait_ir_idle(ctx);
    saved = ctx->fsm;
    next  = ctx->ir_state;

    switch (cmd->type) {
    case CMD_STOP_AC:
        LOG_INFO("Remote: STOP");
        control_fsm_notify_manual_off(&ctx->fsm, now_ms);
        next.power_on = false;
        sent = apply_ir(ctx, &next);
        break;

    case CMD_APPLY:
        LOG_INFO("Remote: APPLY %s %d C", control_fsm_mode_name(to_fsm_mode(cmd->mode)), (int)cmd->target_temperature);
        if (!control_fsm_notify_manual_apply(&ctx->fsm, to_fsm_mode(cmd->mode), cmd->target_temperature, now_ms)) {
            LOG_WARN("Remote: APPLY rejected (sensor fault)");
            return;
        }
        next.power_on    = true;
        next.mode        = to_ir_mode(ctx->fsm.mode);
        next.temperature = ctx->fsm.setpoint_c;
        if (cmd->has_fan) {
            next.fan = cmd->fan;
        }
        if (cmd->has_airflow) {
            next.dir = cmd->airflow;
        }
        sent = apply_ir(ctx, &next);
        break;

    default:
        LOG_WARN("Remote: unknown command type %d", (int)cmd->type);
        return;
    }

    if (!sent) {
        LOG_WARN("Remote command not applied");
        ctx->fsm = saved;
    }
}

/** センサーの初期化を、決められた回数まで試みる。 */
static bool sensor_init_with_retry(void) {
    for (uint8_t i = 0u; i < SENSOR_INIT_RETRIES; i++) {
        if (bme280_init()) {
            return true;
        }
        LOG_WARN("BME280 init failed (%u/%u), retrying...",
                 (unsigned)(i + 1u), (unsigned)SENSOR_INIT_RETRIES);
        rtos_delay_ms(SENSOR_INIT_RETRY_MS);
    }
    return false;
}

/** センサー異常の状態に移り、必要ならエアコンを止める。 */
static void enter_sensor_fault(control_ctx_t *ctx, uint32_t now_ms) {
    control_decision_t d;

    if (ctx->fsm.state == CONTROL_STATE_FAULT) {
        return;
    }
    LOG_ERROR("Sensor fault.");
    ctx->last_retry_ms = now_ms;

    d = control_fsm_notify_fault(&ctx->fsm, now_ms);
    if (!apply_decision(ctx, &d)) {
        LOG_WARN("Safe-state OFF not sent, will retry");
    }
}

/** センサー異常なのにエアコンが運転中のままなら、停止を送り直す。 */
static void retry_safe_off(control_ctx_t *ctx) {
    ac_state_t next = ctx->ir_state;

    if ((ctx->fsm.state != CONTROL_STATE_FAULT) || !ctx->ir_state.power_on) {
        return;
    }
    LOG_INFO("AC OFF (retry safe state)");
    next.power_on = false;
    (void)apply_ir(ctx, &next);
}

/** 一定間隔でセンサーの初期化を試み、成功したら状態遷移に復帰を伝える。 */
static void try_sensor_recovery(control_ctx_t *ctx, uint32_t now_ms) {
    if (!sys_elapsed_since(now_ms, ctx->last_retry_ms, SENSOR_RETRY_INTERVAL_MS)) {
        return;
    }
    ctx->last_retry_ms = now_ms;

    LOG_INFO("Attempting sensor recovery...");
    if (!bme280_init()) {
        LOG_WARN("Sensor still unavailable");
        return;
    }

    ctx->sensor_fail_streak = 0u;
    temp_filter_init(&ctx->filter);
    control_fsm_notify_recovered(&ctx->fsm);
}

/** 温湿度を測定し、送信用に渡したうえで、フィルタを通した室温から制御を判断する。 */
static void do_measurement(control_ctx_t *ctx, uint32_t now_ms) {
    float temp = 0.0f;
    float hum  = 0.0f;
    temp_result_t r;
    float avg_temp = 0.0f;
    temp_stability_t stability = {0};
    control_decision_t d = {0};
    control_fsm_t saved;

    if (!bme280_read(&temp, &hum)) {
        ctx->sensor_fail_streak++;
        LOG_WARN("Sensor read failed (%u/%u)", (unsigned)ctx->sensor_fail_streak, (unsigned)SENSOR_FAIL_THRESHOLD);
        if (ctx->sensor_fail_streak >= SENSOR_FAIL_THRESHOLD) {
            enter_sensor_fault(ctx, now_ms);
        }
        return;
    }
    ctx->sensor_fail_streak = 0u;

    if (!network_report_sensor(temp, hum)) {
        LOG_WARN("Sensor queue full, sample dropped");
    }

    r = temp_filter_push(&ctx->filter, temp);

    if (r == TEMP_RESULT_FAULT) {
        enter_sensor_fault(ctx, now_ms);
        return;
    }
    if ((r == TEMP_RESULT_REJECTED_RANGE) || (r == TEMP_RESULT_REJECTED_SPIKE)) {
        return;
    }

    if (!temp_filter_get_average(&ctx->filter, &avg_temp)) {
        return;
    }

    temp_filter_get_stability(&ctx->filter, &stability);

    saved = ctx->fsm;
    d = control_fsm_evaluate(&ctx->fsm, now_ms, avg_temp, &stability);
    if (!apply_decision(ctx, &d)) {
        LOG_WARN("AC decision not applied, will retry");
        ctx->fsm = saved;
    }
}

void control_task(void *pvParameters) {
    (void)pvParameters;
    static control_ctx_t ctx;
    (void)memset(&ctx, 0, sizeof(ctx));

    ctx.ir_state.power_on    = false;
    ctx.ir_state.mode        = AC_MODE_COOL;
    ctx.ir_state.temperature = (uint8_t)AC_TEMP_MAX_C;
    ctx.ir_state.fan         = AC_FAN_AUTO;
    ctx.ir_state.dir         = AC_DIR_AUTO;

    temp_filter_init(&ctx.filter);
    control_fsm_init(&ctx.fsm);
    if (!ac_ir_init()) {
        LOG_ERROR("IR TX unavailable. Measurements continue, but AC commands cannot be sent.");
    }

    if (!sensor_init_with_retry()) {
        LOG_ERROR("BME280 unavailable at startup.");
        enter_sensor_fault(&ctx, sys_get_time_ms());
    }

    ctx.wdt_id = wdt_register_task("Control", CONTROL_WDT_RUNNING_LIMIT_MS, CONTROL_WDT_WAITING_LIMIT_MS);
    if (ctx.wdt_id == WDT_INVALID_ID) {
        LOG_ERROR("wdt_register_task() failed. Control task aborting.");
        rtos_task_delete_current();
        return;
    }
    ctx.next_measure_ms = sys_deadline_from_now(SENSOR_INTERVAL_MS);

    while (true) {
        uint32_t now_ms = 0u;
        uint32_t wait_ms = 0u;
        remote_command_t cmd = {0};
        bool got_cmd = false;

        wdt_task_set_state(ctx.wdt_id, WDT_TASK_RUNNING);
        now_ms  = sys_get_time_ms();
        wait_ms = sys_deadline_remaining(now_ms, ctx.next_measure_ms);

        if (wait_ms > CONTROL_WAIT_SLICE_MS) {
            wait_ms = CONTROL_WAIT_SLICE_MS;
        }

        wdt_task_set_state(ctx.wdt_id, WDT_TASK_WAITING);
        got_cmd = rtos_queue_receive(s_command_queue, &cmd, wait_ms);
        wdt_task_set_state(ctx.wdt_id, WDT_TASK_RUNNING);

        now_ms = sys_get_time_ms();
        if (got_cmd) {
            process_remote_command(&ctx, &cmd, now_ms);
        }

        if (sys_deadline_expired(now_ms, ctx.next_measure_ms)) {
            ctx.next_measure_ms += SENSOR_INTERVAL_MS;
            if (sys_deadline_expired(now_ms, ctx.next_measure_ms)) {
                ctx.next_measure_ms = sys_deadline_from_now(SENSOR_INTERVAL_MS);
            }

            if (ctx.fsm.state != CONTROL_STATE_FAULT) {
                do_measurement(&ctx, now_ms);
            } else {
                retry_safe_off(&ctx);
                try_sensor_recovery(&ctx, now_ms);
            }
        }
    }
}
