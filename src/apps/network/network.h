#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include "ac_ir.h"

/** 受け渡し用のキューを作り、JSONの解析を使えるようにする。スケジューラの開始前に1回呼ぶ。失敗したらfalse。 */
bool network_init(void);

/** ネットワークタスク。 */
void network_task(void *pvParameters);

/** エアコンの最新の状態を渡す。MQTT接続中ならac/stateとして送信される。 */
bool network_report_ac_state(const ac_state_t *state);

/** 測定値を渡す。MQTT接続中ならac/sensorとして送信される。キューが満杯ならfalse。 */
bool network_report_sensor(float temperature, float humidity);

#endif /* NETWORK_H */
