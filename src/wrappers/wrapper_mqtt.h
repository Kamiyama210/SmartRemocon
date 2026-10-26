#ifndef WRAPPER_MQTT_H
#define WRAPPER_MQTT_H

#include <stdint.h>
#include <stdbool.h>

#define WRAPPER_MQTT_RX_PAYLOAD_MAX   (256u)   ///< 受信するペイロードの最大長。運転操作の上限127文字より大きくする
#define WRAPPER_MQTT_RX_TOPIC_MAX     (128u)   ///< 受信するトピック名の最大長

/** 接続の結果を受け取るコールバック。lwIPの処理の中から呼ばれる。 */
typedef void (*wrapper_mqtt_connect_cb_t)(bool success);

/** 受信したメッセージを受け取るコールバック。lwIPの処理の中から呼ばれる。 */
typedef void (*wrapper_mqtt_message_cb_t)(const char *topic, const uint8_t *payload, uint16_t len);

/** サブスクライブの結果（SUBACK）を受け取るコールバック。lwIPの処理の中から呼ばれる。 */
typedef void (*wrapper_mqtt_subscribe_cb_t)(bool success);

/** ブローカへの接続の設定。 */
typedef struct {
    const char *broker_ip;        ///< ブローカのIPアドレス
    uint16_t    port;             ///< ブローカのポート番号
    const char *client_id;        ///< クライアントID
    uint16_t    keep_alive_sec;   ///< キープアライブの間隔（秒）
    const char *will_topic;       ///< LWTのトピック。NULLならLWTを使わない
    const char *will_msg;         ///< LWTのメッセージ
    uint8_t     will_qos;         ///< LWTのQoS
    bool        will_retain;      ///< LWTをretainするか
} wrapper_mqtt_config_t;

/** MQTTクライアントを確保する。確保済みなら何もしない。 */
bool wrapper_mqtt_init(void);

/** ブローカへの接続を開始する。結果はconn_cbで通知される。 */
bool wrapper_mqtt_connect(const wrapper_mqtt_config_t *config, wrapper_mqtt_connect_cb_t conn_cb, wrapper_mqtt_message_cb_t msg_cb);

/** ブローカから切断する。 */
void wrapper_mqtt_disconnect(void);

/** メッセージを送信する。接続していなければfalse。 */
bool wrapper_mqtt_publish(const char *topic, const void *payload, uint16_t len, uint8_t qos, bool retain);

/** トピックのサブスクライブを依頼する。依頼を出せなければfalse。結果はsub_cbで通知される。 */
bool wrapper_mqtt_subscribe(const char *topic, uint8_t qos, wrapper_mqtt_subscribe_cb_t sub_cb);

/** ブローカと接続しているか。 */
bool wrapper_mqtt_is_connected(void);

#endif /* WRAPPER_MQTT_H */
