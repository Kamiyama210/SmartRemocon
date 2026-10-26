#include "control_fsm.h"
#include "ac_ir.h"
#include "logger.h"
#include "sys.h"

#include <string.h>

#define SETTLE_TIME_MS          (10u * MS_PER_MIN)    ///< 操作を送ってから安定を判定し始めるまでの時間。
#define MIN_COMMAND_INTERVAL_MS (10u * MS_PER_MIN)    ///< 自動で操作を送る最小の間隔。
#define IDLE_RESTART_MARGIN_C   (1.0f)                ///< 運転していない状態（IDLE）から運転を始める判定で、不感帯に足す幅
#define COOL_TARGET_C           (22.0f)               ///< 冷房の目標室温
#define COOL_DEADBAND_C         (1.0f)                ///< 冷房の不感帯の幅
#define COOL_POWER_DIR          (-1)                  ///< 冷房は設定温度を下げると出力が上がる
#define HEAT_TARGET_C           (22.0f)               ///< 暖房の目標室温
#define HEAT_DEADBAND_C         (1.0f)                ///< 暖房の不感帯の幅
#define HEAT_POWER_DIR          (+1)                  ///< 暖房は設定温度を上げると出力が上がる

/** 運転モードごとの制御の設定 */
typedef struct {
    const char *name;           ///< ログ用の名前
    float       target_c;       ///< 目標室温
    float       deadband_c;     ///< 不感帯の幅。この範囲では何もしない
    int8_t      power_dir;      ///< 出力を上げるときの設定温度の増減の向き
} mode_profile_t;

/** 運転モードごとの制御の設定の一覧*/
static const mode_profile_t MODE_PROFILE[] = {
    { "COOL", COOL_TARGET_C, COOL_DEADBAND_C, COOL_POWER_DIR },
    { "HEAT", HEAT_TARGET_C, HEAT_DEADBAND_C, HEAT_POWER_DIR },
};

/** 運転モードの制御の設定を返す */
static const mode_profile_t *profile_of(control_mode_t m) {
    if ((uint32_t)m >= ARRAY_SIZE(MODE_PROFILE)) {
        return &MODE_PROFILE[CONTROL_MODE_COOL];
    }
    return &MODE_PROFILE[m];
}

/** 判断の結果を作る。 */
static control_decision_t make_decision(const control_fsm_t *t, control_action_t action,
                                        uint8_t setpoint, const char *reason) {
    control_decision_t d;
    d.action     = action;
    d.mode       = (t != NULL) ? t->mode : CONTROL_MODE_COOL;
    d.setpoint_c = setpoint;
    d.reason     = reason;
    return d;
}

/** 何もしない場合の判断の結果を作る。 */
static control_decision_t no_action(const control_fsm_t *t, const char *reason) {
    return make_decision(t, CONTROL_ACTION_NONE, t->setpoint_c, reason);
}

/** 状態の名前を返す（ログ用） */
static const char *state_name(control_state_t s) {
    switch (s) {
    case CONTROL_STATE_INIT:      return "INIT";
    case CONTROL_STATE_SETTLING:  return "SETTLING";
    case CONTROL_STATE_STEADY:    return "STEADY";
    case CONTROL_STATE_IDLE:      return "IDLE";
    case CONTROL_STATE_FAULT:     return "FAULT";
    case CONTROL_STATE_OFF:       return "OFF";
    default:                      return "?";
    }
}

const char *control_fsm_mode_name(control_mode_t m) {
    return profile_of(m)->name;
}

/** 運転中かを返す。 */
static bool is_running(const control_fsm_t *t) {
    return (t->state != CONTROL_STATE_IDLE) && (t->state != CONTROL_STATE_FAULT) && (t->state != CONTROL_STATE_OFF);
}

/** 設定温度を1℃動かす。出力を上げる向きか下げる向きかを指定する。 */
static bool step_setpoint(control_fsm_t *t, bool increase_power) {
    const mode_profile_t *p = profile_of(t->mode);
    int8_t  dir  = increase_power ? p->power_dir : (int8_t)(-p->power_dir);
    int32_t next = (int32_t)t->setpoint_c + dir;

    if ((next < (int32_t)AC_TEMP_MIN_C) || (next > (int32_t)AC_TEMP_MAX_C)) {
        return false;
    }
    t->setpoint_c = (uint8_t)next;
    return true;
}

/** 操作を送った時刻を記録し、安定待ちに移る。 */
static void commit_command(control_fsm_t *t, uint32_t now_ms) {
    t->last_command_ms = now_ms;
    t->state           = CONTROL_STATE_SETTLING;
}

/** 目標室温に対して、どれだけ出力が足りないかを返す。 */
static float calc_demand(const mode_profile_t *p, control_mode_t mode, float room_temp_c) {
    if (mode == CONTROL_MODE_HEAT) {
        return p->target_c - room_temp_c;
    }
    return room_temp_c - p->target_c;
}

void control_fsm_init(control_fsm_t *t) {
    if (t == NULL) {
        return;
    }
    (void)memset(t, 0, sizeof(*t));
    t->state      = CONTROL_STATE_INIT;
    t->mode       = CONTROL_MODE_COOL;
    t->setpoint_c = (uint8_t)MODE_PROFILE[CONTROL_MODE_COOL].target_c;
}

bool control_fsm_notify_manual_apply(control_fsm_t *t, control_mode_t mode, uint8_t setpoint_c, uint32_t now_ms) {
    if ((t == NULL) || (t->state == CONTROL_STATE_FAULT)) {
        return false;
    }
    t->mode       = mode;
    t->setpoint_c = setpoint_c;
    commit_command(t, now_ms);
    return true;
}

void control_fsm_notify_manual_off(control_fsm_t *t, uint32_t now_ms) {
    if (t == NULL) {
        return;
    }
    t->last_command_ms = now_ms;
    if (t->state == CONTROL_STATE_FAULT) {
        t->off_before_fault = true;
        return;
    }
    t->state = CONTROL_STATE_OFF;
}

control_decision_t control_fsm_notify_fault(control_fsm_t *t, uint32_t now_ms) {
    bool was_running;

    if (t == NULL) {
        return make_decision(t, CONTROL_ACTION_NONE, 0u, "null");
    }
    if (t->state == CONTROL_STATE_FAULT) {
        return no_action(t, "sensor fault (already in fault)");
    }
    was_running         = is_running(t);
    t->off_before_fault = (t->state == CONTROL_STATE_OFF);
    t->state            = CONTROL_STATE_FAULT;

    if (was_running) {
        t->last_command_ms = now_ms;
        return make_decision(t, CONTROL_ACTION_POWER_OFF, t->setpoint_c,
                             "sensor fault: forcing safe state");
    }
    return no_action(t, "sensor fault (already off)");
}

void control_fsm_notify_recovered(control_fsm_t *t) {
    if ((t == NULL) || (t->state != CONTROL_STATE_FAULT)) {
        return;
    }
    if (t->off_before_fault) {
        LOG_INFO("Sensor recovered. Staying off until a remote command.");
        t->state = CONTROL_STATE_OFF;
    } else {
        LOG_INFO("Sensor recovered. Resuming control.");
        t->state = CONTROL_STATE_INIT;
    }
    t->off_before_fault = false;
}

/** 室温が快適な範囲なら停止のまま、そうでなければ運転を始める。 */
static control_decision_t control_state_init(control_fsm_t *t, uint32_t now_ms, float room_temp_c) {
    const mode_profile_t *p = profile_of(t->mode);
    float demand = calc_demand(p, t->mode, room_temp_c);

    if (demand <= p->deadband_c) {
        LOG_INFO("Room %.1f C, target %.1f +-%.1f (demand %+.1f). Staying idle.",
                 (double)room_temp_c, (double)p->target_c,
                 (double)p->deadband_c, (double)demand);
        t->state = CONTROL_STATE_IDLE;
        return no_action(t, "already comfortable at startup");
    }

    t->setpoint_c = (uint8_t)p->target_c;
    commit_command(t, now_ms);
    return make_decision(t, CONTROL_ACTION_SET, t->setpoint_c, "startup: begin operation");
}

/** 操作から一定時間が過ぎ、室温が落ち着いたら安定に移る。 */
static control_decision_t control_state_settling(control_fsm_t *t, uint32_t now_ms, const temp_stability_t *st) {
    if (!sys_elapsed_since(now_ms, t->last_command_ms, SETTLE_TIME_MS)) {
        return no_action(t, "settling: waiting for AC response");
    }
    if (!st->is_valid || !st->is_stable) {
        return no_action(t, "settling: temperature still moving");
    }

    LOG_INFO("Reached steady state (range=%.2f C, slope=%.2f C/min)",
             (double)st->range_c, (double)st->slope_c_per_min);
    t->state = CONTROL_STATE_STEADY;
    return no_action(t, "entered steady state");
}

/** 室温が不感帯の外なら、設定温度を1℃動かす。安定が崩れたら安定待ちに戻る。 */
static control_decision_t control_state_steady(control_fsm_t *t, uint32_t now_ms, float room_temp_c, const temp_stability_t *st) {
    const mode_profile_t *p = profile_of(t->mode);
    float demand;

    if (!st->is_valid || !st->is_stable) {
        t->state = CONTROL_STATE_SETTLING;
        return no_action(t, "stability lost: back to settling");
    }

    if (!sys_elapsed_since(now_ms, t->last_command_ms, MIN_COMMAND_INTERVAL_MS)) {
        return no_action(t, "steady but within min command interval");
    }

    demand = calc_demand(p, t->mode, room_temp_c);

    if (demand > p->deadband_c) {
        if (step_setpoint(t, true)) {
            commit_command(t, now_ms);
            return make_decision(t, CONTROL_ACTION_SET, t->setpoint_c, "steady but under target: increase output");
        }
        return no_action(t, "under target but setpoint at limit");
    }

    if (demand < -p->deadband_c) {
        if (step_setpoint(t, false)) {
            commit_command(t, now_ms);
            return make_decision(t, CONTROL_ACTION_SET, t->setpoint_c, "steady but over target: ease output");
        }
        return no_action(t, "over target but setpoint at limit");
    }

    return no_action(t, "comfortable and steady: nothing to do");
}

/** 監視だけ行い、室温が快適な範囲から大きく外れたら運転を始める。 */
static control_decision_t control_state_idle(control_fsm_t *t, uint32_t now_ms, float room_temp_c) {
    const mode_profile_t *p = profile_of(t->mode);
    float demand = calc_demand(p, t->mode, room_temp_c);

    if (demand > (p->deadband_c + IDLE_RESTART_MARGIN_C)) {
        if (sys_elapsed_since(now_ms, t->last_command_ms, MIN_COMMAND_INTERVAL_MS)) {
            t->setpoint_c = (uint8_t)p->target_c;
            commit_command(t, now_ms);
            return make_decision(t, CONTROL_ACTION_SET, t->setpoint_c,
                                 "idle: drifted out of comfort, restarting");
        }
    }
    return no_action(t, "idle: monitoring only");
}

control_decision_t control_fsm_evaluate(control_fsm_t *t, uint32_t now_ms, float room_temp_c, const temp_stability_t *stability) {
    control_decision_t decision;
    control_state_t prev;

    if ((t == NULL) || (stability == NULL)) {
        return make_decision(t, CONTROL_ACTION_NONE, 0u, "null argument");
    }
    prev = t->state;

    switch (t->state) {
    case CONTROL_STATE_INIT:
        decision = control_state_init(t, now_ms, room_temp_c);
        break;
    case CONTROL_STATE_SETTLING:
        decision = control_state_settling(t, now_ms, stability);
        break;
    case CONTROL_STATE_STEADY:
        decision = control_state_steady(t, now_ms, room_temp_c, stability);
        break;
    case CONTROL_STATE_IDLE:
        decision = control_state_idle(t, now_ms, room_temp_c);
        break;
    case CONTROL_STATE_FAULT:
        decision = no_action(t, "fault: awaiting sensor recovery");
        break;
    case CONTROL_STATE_OFF:
        decision = no_action(t, "off: awaiting remote command");
        break;
    default:
        t->state = CONTROL_STATE_INIT;
        decision = no_action(t, "invalid state, recovering");
        break;
    }

    if (t->state != prev) {
        LOG_INFO("AC: %s/%s -> %s/%s (%.1f C)",
                 state_name(prev), control_fsm_mode_name(t->mode),
                 state_name(t->state), control_fsm_mode_name(t->mode),
                 (double)room_temp_c);
    }
    return decision;
}
