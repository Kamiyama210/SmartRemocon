#include "mqtt.h"
#include "wrapper_mqtt.h"
#include "logger.h"

#include <string.h>

#ifndef MQTT_BROKER_IP
#error "MQTT_BROKER_IP is not defined. Set it with -DMQTT_BROKER_IP=x.x.x.x when configuring CMake."
#endif
#define MQTT_BROKER_PORT         (1883u)                  ///< MQTTブローカのポート番号
#define MQTT_CLIENT_ID           "Pico2W_AC_Controller"   ///< MQTTのクライアントID
#define MQTT_KEEPALIVE_SEC       (60u)                    ///< MQTTのキープアライブの間隔（秒）。offlineは最大1.5倍の90sで届き、PCの途絶判定120sより早い

#define MQTT_ONLINE_MESSAGE      "online"                 ///< 接続時にac/statusへ送るメッセージ
#define MQTT_LWT_MESSAGE         "offline"                ///< 切断時にac/statusへ送るメッセージ

#define MQTT_CMD_QOS             (1u)                     ///< 運転操作のQoS
#define MQTT_SENSOR_QOS          (0u)                     ///< 測定値のQoS。5sごとに次の値が届くので0
#define MQTT_STATE_QOS           (1u)                     ///< 状態のQoS
#define MQTT_STATUS_QOS          (1u)                     ///< 生死のQoS

#define RX_PAYLOAD_MIN_LEN       (128u)                   ///< 受信バッファに必要な長さ。運転操作の上限127文字＋終端

_Static_assert(WRAPPER_MQTT_RX_PAYLOAD_MAX >= RX_PAYLOAD_MIN_LEN, "MQTT RX buffer too small for command payloads");

static mqtt_status_cb_t  s_status_cb   = NULL;    ///< 接続状態の変化の通知先
static mqtt_message_cb_t s_msg_cb      = NULL;    ///< 受信したメッセージの通知先

/** ブローカへの接続の設定。 */
static const wrapper_mqtt_config_t s_mqtt_config = {
    .broker_ip      = MQTT_BROKER_IP,
    .port           = MQTT_BROKER_PORT,
    .client_id      = MQTT_CLIENT_ID,
    .keep_alive_sec = MQTT_KEEPALIVE_SEC,
    .will_topic     = MQTT_TOPIC_STATUS,
    .will_msg       = MQTT_LWT_MESSAGE,
    .will_qos       = MQTT_STATUS_QOS,
    .will_retain    = true,
};

/** 接続状態を上位へ通知する。 */
static void notify_status(bool connected) {
    if (s_status_cb != NULL) {
        s_status_cb(connected);
    }
}

/** サブスクライブの結果を受け取る。サブスクライブまで済んで、初めて接続したと上位へ通知する。 */
static void on_subscribe_result(bool success) {
    notify_status(success);
}

/** 接続の結果を受け取り、成功なら運転操作のトピックをサブスクライブする。失敗なら切断として上位へ通知する。 */
static void on_connect_result(bool success) {
    if (!success || !wrapper_mqtt_subscribe(MQTT_TOPIC_CMD, MQTT_CMD_QOS, on_subscribe_result)) {
        notify_status(false);
    }
}

/** 受信したメッセージを上位へ渡す。 */
static void on_message(const char *topic, const uint8_t *data, uint16_t len) {
    if (s_msg_cb != NULL) {
        s_msg_cb(topic, data, len);
    }
}

bool mqtt_start_connect(mqtt_status_cb_t status_cb, mqtt_message_cb_t msg_cb) {
    s_status_cb = status_cb;
    s_msg_cb    = msg_cb;

    if (!wrapper_mqtt_init()) {
        LOG_ERROR("MQTT client allocation failed");
        return false;
    }
    return wrapper_mqtt_connect(&s_mqtt_config, on_connect_result, on_message);
}

void mqtt_stop(void) {
    wrapper_mqtt_disconnect();
}

bool mqtt_is_connected(void) {
    return wrapper_mqtt_is_connected();
}

bool mqtt_publish_sensor(const char *sensor_json, uint16_t len) {
    if (sensor_json == NULL) {
        return false;
    }
    return wrapper_mqtt_publish(MQTT_TOPIC_SENSOR, sensor_json, len, MQTT_SENSOR_QOS, false);
}

bool mqtt_publish_state(const char *state_json, uint16_t len) {
    if (state_json == NULL) {
        return false;
    }
    return wrapper_mqtt_publish(MQTT_TOPIC_STATE, state_json, len, MQTT_STATE_QOS, true);
}

/** ac/statusへメッセージを送る（retain）。 */
static bool publish_status(const char *msg) {
    return wrapper_mqtt_publish(MQTT_TOPIC_STATUS, msg, (uint16_t)strlen(msg), MQTT_STATUS_QOS, true);
}

bool mqtt_publish_online(void) {
    return publish_status(MQTT_ONLINE_MESSAGE);
}

bool mqtt_publish_offline(void) {
    return publish_status(MQTT_LWT_MESSAGE);
}
