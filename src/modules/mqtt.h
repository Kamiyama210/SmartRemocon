#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>

#define MQTT_TOPIC_CMD          "ac/state/set"           ///< 運転操作を受け取るトピック
#define MQTT_TOPIC_SENSOR       "ac/sensor"              ///< 測定値を送るトピック
#define MQTT_TOPIC_STATE        "ac/state"               ///< 最後に送ったIRの内容を送るトピック
#define MQTT_TOPIC_STATUS       "ac/status"              ///< デバイスの生死を送るトピック

/** 接続状態の変化を受け取るコールバック。trueは運転操作のトピックのサブスクライブまで済んだとき。falseは接続の失敗・切断・サブスクライブの失敗のいずれか。 */
typedef void (*mqtt_status_cb_t)(bool connected);

/** 受信したメッセージを受け取るコールバック。 */
typedef void (*mqtt_message_cb_t)(const char *topic, const uint8_t *payload, uint16_t len);

/** ブローカへの接続を開始する。結果はstatus_cbで通知される。 */
bool mqtt_start_connect(mqtt_status_cb_t status_cb, mqtt_message_cb_t msg_cb);

/** ブローカから切断する。 */
void mqtt_stop(void);

/** ブローカと接続しているか。 */
bool mqtt_is_connected(void);

/** 測定値のペイロードをac/sensorへ送る。 */
bool mqtt_publish_sensor(const char *sensor_json, uint16_t len);

/** 最後に送ったIRの内容をac/stateへ送る（retain）。 */
bool mqtt_publish_state(const char *state_json, uint16_t len);

/** ac/statusへonlineを送る（retain）。 */
bool mqtt_publish_online(void);

/** ac/statusへofflineを送る（retain）。 */
bool mqtt_publish_offline(void);

#endif /* MQTT_H */
