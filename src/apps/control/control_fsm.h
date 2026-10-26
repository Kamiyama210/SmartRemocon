#ifndef CONTROL_FSM_H
#define CONTROL_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include "temp_filter.h"

/** 状態遷移の状態 */
typedef enum {
    CONTROL_STATE_INIT = 0,   ///< 起動直後
    CONTROL_STATE_SETTLING,   ///< 安定待ち。操作の後、室温が落ち着くのを待つ。暑い・寒いは判定しない
    CONTROL_STATE_STEADY,     ///< 安定。室温が落ち着いており、暑い・寒いを判定する
    CONTROL_STATE_IDLE,       ///< 運転していない。起動時に室温が快適だったときに入る。監視だけ行い、室温が大きく外れたら運転を始める
    CONTROL_STATE_FAULT,      ///< センサー異常。測定を止めて復帰を待つ。手動の運転操作は受け付けず、停止だけ受け付ける
    CONTROL_STATE_OFF         ///< 手動停止中。運転操作を受けるまで自動では運転しない
} control_state_t;

/** 状態遷移で使う運転モード */
typedef enum {
    CONTROL_MODE_COOL = 0, ///< 冷房
    CONTROL_MODE_HEAT      ///< 暖房
} control_mode_t;

/** 判断の結果として行う操作 */
typedef enum {
    CONTROL_ACTION_NONE = 0,     ///< 何もしない
    CONTROL_ACTION_SET,          ///< 運転し、モードと設定温度を送る
    CONTROL_ACTION_POWER_OFF     ///< 停止する
} control_action_t;

/** 判断の結果 */
typedef struct {
    control_action_t  action;        ///< 行う操作
    control_mode_t    mode;          ///< 運転モード
    uint8_t           setpoint_c;    ///< 設定温度
    const char       *reason;        ///< 判断理由
} control_decision_t;

/** 状態遷移の現在の状態と設定 */
typedef struct {
    control_state_t   state;              ///< 現在の状態
    control_mode_t    mode;               ///< 運転モード
    uint8_t           setpoint_c;         ///< 現在の設定温度
    uint32_t          last_command_ms;    ///< 最後に操作を送った時刻
    bool              off_before_fault;   ///< センサー異常の前、またはその間に手動で停止されたか。復帰したら手動停止中に戻る
} control_fsm_t;

/** 状態遷移を初期化する。*/
void control_fsm_init(control_fsm_t *t);

/**
 * 手動で運転モードと設定温度を受けたことを伝える。操作を送った時刻も更新し、安定待ちに移る。
 * センサー異常中は室温がわからないため受け付けず、falseを返す。
 */
bool control_fsm_notify_manual_apply(control_fsm_t *t, control_mode_t mode, uint8_t setpoint_c, uint32_t now_ms);

/** 手動で停止したことを伝える。センサー異常中なら異常のまま、復帰後に手動停止中に戻るよう覚えておく。 */
void control_fsm_notify_manual_off(control_fsm_t *t, uint32_t now_ms);

/** センサー異常を伝える。どの段階からでも異常に移り、運転中なら停止を返す。 */
control_decision_t control_fsm_notify_fault(control_fsm_t *t, uint32_t now_ms);

/** センサーが復帰したことを伝える。異常の前が手動停止中なら手動停止中に、それ以外は起動直後に戻る。 */
void control_fsm_notify_recovered(control_fsm_t *t);

/** 室温と安定の判定結果から、行う操作を判断する。 */
control_decision_t control_fsm_evaluate(control_fsm_t *t, uint32_t now_ms,
                                        float room_temp_c,
                                        const temp_stability_t *stability);

/** 運転モードの名前を返す */
const char *control_fsm_mode_name(control_mode_t m);

#endif /* CONTROL_FSM_H */
