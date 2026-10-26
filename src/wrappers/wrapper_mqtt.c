#include "wrapper_mqtt.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"

#include <string.h>

static mqtt_client_t              *s_client  = NULL;   ///< lwIPのMQTTクライアント
static wrapper_mqtt_connect_cb_t   s_conn_cb = NULL;   ///< 接続の結果の通知先
static wrapper_mqtt_message_cb_t   s_msg_cb  = NULL;   ///< 受信したメッセージの通知先
static wrapper_mqtt_subscribe_cb_t s_sub_cb  = NULL;   ///< サブスクライブの結果の通知先

static struct mqtt_connect_client_info_t s_client_info;   ///< 接続の情報（lwIPが接続中に参照する）

static char     s_rx_topic[WRAPPER_MQTT_RX_TOPIC_MAX];        ///< 受信中のメッセージのトピック名
static uint8_t  s_rx_payload[WRAPPER_MQTT_RX_PAYLOAD_MAX];    ///< 受信中のメッセージのペイロード
static uint16_t s_rx_len      = 0u;                           ///< 受信済みのペイロードの長さ
static bool     s_rx_overflow = false;                        ///< ペイロードがバッファに収まらないか

/** 受信の始まりにトピック名を受け取り、ペイロードがバッファに収まるかを判定する。 */
static void on_incoming_publish(void *arg, const char *topic, u32_t tot_len) {
    (void)arg;

    (void)strncpy(s_rx_topic, topic, sizeof(s_rx_topic) - 1u);
    s_rx_topic[sizeof(s_rx_topic) - 1u] = '\0';

    s_rx_len = 0u;

    s_rx_overflow = (tot_len > WRAPPER_MQTT_RX_PAYLOAD_MAX);
}

/** 分割して届くペイロードをつなぎ合わせ、最後の断片を受け取ったら上位へ渡す。大きすぎるものは捨てる。 */
static void on_incoming_data(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    (void)arg;

    if (!s_rx_overflow) {
        if (((uint32_t)s_rx_len + len) <= WRAPPER_MQTT_RX_PAYLOAD_MAX) {
            (void)memcpy(&s_rx_payload[s_rx_len], data, len);
            s_rx_len = (uint16_t)(s_rx_len + len);
        } else {
            s_rx_overflow = true;
        }
    }

    if ((flags & MQTT_DATA_FLAG_LAST) == 0u) {
        return;
    }

    if (s_rx_overflow) {
        s_rx_overflow = false;
        s_rx_len      = 0u;
        return;
    }

    if (s_msg_cb != NULL) {
        s_msg_cb(s_rx_topic, s_rx_payload, s_rx_len);
    }
    s_rx_len = 0u;
}

/** サブスクライブの結果（SUBACK）を受け取り、上位へ通知する。 */
static void on_subscribe_result(void *arg, err_t result) {
    (void)arg;
    if (s_sub_cb != NULL) {
        s_sub_cb(result == ERR_OK);
    }
}

/** 接続状態の変化を受け取り、接続したら受信のコールバックを登録して、上位へ通知する。 */
static void on_connect_result(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    bool success = (status == MQTT_CONNECT_ACCEPTED);

    if (success) {
        mqtt_set_inpub_callback(client, on_incoming_publish, on_incoming_data, arg);
    }
    s_rx_len      = 0u;
    s_rx_overflow = false;

    if (s_conn_cb != NULL) {
        s_conn_cb(success);
    }
}

bool wrapper_mqtt_init(void) {
    if (s_client != NULL) {
        return true;
    }
    cyw43_arch_lwip_begin();
    s_client = mqtt_client_new();
    cyw43_arch_lwip_end();

    return (s_client != NULL);
}

bool wrapper_mqtt_connect(const wrapper_mqtt_config_t *config, wrapper_mqtt_connect_cb_t conn_cb, wrapper_mqtt_message_cb_t msg_cb) {
    ip_addr_t broker_ip_addr;
    err_t err;

    if ((s_client == NULL) || (config == NULL) ||
        (config->broker_ip == NULL) || (config->client_id == NULL)) {
        return false;
    }
    if (!ipaddr_aton(config->broker_ip, &broker_ip_addr)) {
        return false;
    }

    s_conn_cb = conn_cb;
    s_msg_cb  = msg_cb;

    (void)memset(&s_client_info, 0, sizeof(s_client_info));
    s_client_info.client_id  = config->client_id;

    s_client_info.keep_alive = (u16_t)config->keep_alive_sec;

    if (config->will_topic != NULL) {
        s_client_info.will_topic  = config->will_topic;
        s_client_info.will_msg    = config->will_msg;
        s_client_info.will_qos    = config->will_qos;
        s_client_info.will_retain = config->will_retain ? 1u : 0u;
    }

    cyw43_arch_lwip_begin();
    err = mqtt_client_connect(s_client, &broker_ip_addr, config->port, on_connect_result, NULL, &s_client_info);
    cyw43_arch_lwip_end();

    return (err == ERR_OK);
}

void wrapper_mqtt_disconnect(void) {
    if (s_client == NULL) {
        return;
    }
    cyw43_arch_lwip_begin();
    mqtt_disconnect(s_client);
    cyw43_arch_lwip_end();

    s_rx_len      = 0u;
    s_rx_overflow = false;
}

bool wrapper_mqtt_publish(const char *topic, const void *payload, uint16_t len, uint8_t qos, bool retain) {
    err_t err;

    if ((s_client == NULL) || (topic == NULL) || (payload == NULL)) {
        return false;
    }

    cyw43_arch_lwip_begin();
    if (!mqtt_client_is_connected(s_client)) {
        cyw43_arch_lwip_end();
        return false;
    }
    err = mqtt_publish(s_client, topic, payload, len, qos, retain ? 1u : 0u, NULL, NULL);
    cyw43_arch_lwip_end();

    return (err == ERR_OK);
}

bool wrapper_mqtt_subscribe(const char *topic, uint8_t qos, wrapper_mqtt_subscribe_cb_t sub_cb) {
    err_t err;

    if ((s_client == NULL) || (topic == NULL)) {
        return false;
    }
    s_sub_cb = sub_cb;

    cyw43_arch_lwip_begin();
    if (!mqtt_client_is_connected(s_client)) {
        cyw43_arch_lwip_end();
        return false;
    }
    err = mqtt_sub_unsub(s_client, topic, qos, on_subscribe_result, NULL, 1);
    cyw43_arch_lwip_end();

    return (err == ERR_OK);
}

bool wrapper_mqtt_is_connected(void) {
    bool connected;

    if (s_client == NULL) {
        return false;
    }
    cyw43_arch_lwip_begin();
    connected = (mqtt_client_is_connected(s_client) != 0);
    cyw43_arch_lwip_end();

    return connected;
}
