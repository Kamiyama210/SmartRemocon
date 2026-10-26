#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "ac_ir.h"

/** 運転操作の種類。0は未設定と区別するため使わない。 */
typedef enum {
    CMD_STOP_AC = 1,   ///< 停止
    CMD_APPLY          ///< 運転
} remote_command_type_t;

/** PCから受けた、解析済みの運転操作。 */
typedef struct {
    remote_command_type_t type;                 ///< 操作の種類
    ac_mode_t             mode;                 ///< 運転モード
    uint8_t               target_temperature;   ///< 目標温度
    ac_fan_t              fan;                  ///< 風量
    ac_dir_t              airflow;              ///< 風向き
    bool                  has_fan;              ///< 風量の指定があるか。false:現在の風量を維持する
    bool                  has_airflow;          ///< 風向きの指定があるか。false:現在の風向きを維持する
} remote_command_t;

/** 運転操作を受け取るキューを作る。スケジューラの開始前に1回呼ぶ。失敗したらfalse。 */
bool control_init(void);

/** 運転操作を空調制御タスクへ渡す。未処理の操作があれば上書きする。失敗したらfalse。 */
bool control_post_command(const remote_command_t *cmd);

/** 空調制御タスク。 */
void control_task(void *pvParameters);

#endif /* CONTROL_H */
