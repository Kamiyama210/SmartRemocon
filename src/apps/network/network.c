#include "network.h"
#include "command_parser.h"
#include "payload.h"
#include "wifi.h"
#include "mqtt.h"
#include "json.h"
#include "logger.h"
#include "rtos.h"
#include "sys.h"
#include "wdt_monitor.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>


#define NET_WIFI_CONNECT_TIMEOUT_MS     (30000u)    ///< WiFiの接続を待つ時間の上限。実測:約10s
#define NET_MQTT_CONNECT_TIMEOUT_MS     (2000u)     ///< MQTTの接続を待つ時間の上限。実測:約50ms
#define NET_LINK_CHECK_INTERVAL_MS      (1000u)     ///< 接続中に接続状態を確認する間隔。
#define NET_BACKOFF_FIRST_DELAY_MS      (1000u)     ///< 再接続の待ち時間の初期値。
#define NET_BACKOFF_LIMIT_DELAY_MS      (60000u)    ///< 再接続の待ち時間の上限。
#define NET_BACKOFF_SHIFT_LIMIT         (6u)        ///< 待ち時間を倍にする回数の上限。
#define NET_WDT_RUNNING_LIMIT_MS        (200u)      ///< WDTの処理中の上限。ループ実測:約1ms
#define NET_WDT_WAITING_LIMIT_MS        (5000u)     ///< WDTの待ち中の上限。
#define NET_WAIT_SLICE_MS               (1000u)     ///< 1回の待ちの最大時間。
#define MAX_PUBLISH_PER_LOOP        (5u)            ///< 1回のループで送信する測定値の最大数。
#define MAX_PUBLISH_FAILURES        (3u)            ///< 送信失敗がこの回数続いたら接続を作り直す。
#define STATE_PAYLOAD_MAX_LEN       (128u)          ///< ac/stateのペイロードの最大長。最も長いペイロードは約90文字
#define SENSOR_PAYLOAD_MAX_LEN      (64u)           ///< ac/sensorのペイロードの最大長。最も長いペイロードは約30文字
#define AC_STATE_BOX_LEN            (1u)            ///< エアコンの状態のキューの長さ。最新の状態だけ送ればよい
#define COMMAND_BOX_LEN             (1u)            ///< 受信した運転操作のキューの長さ。毎回全項目を含むので最新の1件だけでよい
#define SENSOR_QUEUE_LEN            (8u)            ///< 測定値のキューの長さ。5秒周期で40秒分

#define NET_EVENT_WIFI_CONNECTED    (1u << 0)       ///< Wi-Fiが接続した
#define NET_EVENT_WIFI_DISCONNECTED (1u << 1)       ///< Wi-Fiが切断された
#define NET_EVENT_MQTT_CONNECTED    (1u << 2)       ///< MQTTが接続した
#define NET_EVENT_MQTT_DISCONNECTED (1u << 3)       ///< MQTTが切断された
#define NET_EVENT_AC_STATE_CHANGED  (1u << 4)       ///< エアコンの状態が変わった
#define NET_EVENT_COMMAND_RECEIVED  (1u << 5)       ///< 運転操作を受信した
#define NET_EVENT_SENSOR_QUEUED     (1u << 6)       ///< 測定値が届いた
#define NET_EVENT_ALL               (0xFFFFFFFFu)   ///< すべての通知ビット
#define NET_EVENT_NONE              (0u)            ///< どの通知ビットも指定しない

/** 受信した運転操作のペイロード（解析前） */
typedef struct {
    uint16_t len;                               ///< 受信したペイロードの長さ
    uint8_t  data[CMD_PAYLOAD_MAX_LEN];         ///< ペイロード
} net_raw_command_t;

/** 空調制御タスクから受け取った測定値。 */
typedef struct {
    float temperature;   ///< 温度（℃）
    float humidity;      ///< 湿度（%）
} sensor_data_t;

/** ネットワークタスクの状態 */
typedef enum {
    NET_STATE_WIFI_INIT = 0,   ///< Wi-Fiの接続を開始する
    NET_STATE_WIFI_WAIT,       ///< Wi-Fiの接続を待つ
    NET_STATE_MQTT_INIT,       ///< MQTTの接続を開始する
    NET_STATE_MQTT_WAIT,       ///< MQTTの接続を待つ
    NET_STATE_MQTT_PROCESS,    ///< 接続中。送信と接続の確認を行う
    NET_STATE_TEARDOWN,        ///< 接続を片付ける
    NET_STATE_RETRY_DELAY      ///< 再接続まで待つ
} net_state_t;

/** ネットワークタスクの作業用の情報 */
typedef struct {
    net_state_t     state;                ///< 現在の状態
    uint32_t        deadline_ms;          ///< 現在の状態のタイムアウト時間
    uint32_t        wdt_id;               ///< WDTの登録ID
    uint8_t         retry_count;          ///< 再接続を試みた回数
    uint8_t         publish_fail_count;   ///< 送信に連続で失敗した回数
    bool            backlog;              ///< 送り切れていない測定値が残っているか
    bool            online_pending;       ///< ac/statusへonlineを送る必要があるか
    bool            state_pending;        ///< ac/stateを送る必要があるか
} net_ctx_t;

static rtos_task_handle_t volatile s_network_task_handle = NULL;   ///< ネットワークタスクのハンドル

static queue_t s_ac_state_box = NULL;   ///< エアコンの最新の状態を持つキュー
static queue_t s_command_box  = NULL;   ///< 受信した最新の運転操作を持つキュー
static queue_t s_sensor_queue = NULL;   ///< 送信を待つ測定値のキュー

/** 状態の名前を返す（ログ用） */
static const char *state_name(net_state_t s) {
    switch (s) {
    case NET_STATE_WIFI_INIT:     return "WIFI_INIT";
    case NET_STATE_WIFI_WAIT:     return "WIFI_WAIT";
    case NET_STATE_MQTT_INIT:     return "MQTT_INIT";
    case NET_STATE_MQTT_WAIT:     return "MQTT_WAIT";
    case NET_STATE_MQTT_PROCESS:  return "MQTT_PROCESS";
    case NET_STATE_TEARDOWN:      return "TEARDOWN";
    case NET_STATE_RETRY_DELAY:   return "RETRY_DELAY";
    default:                      return "?";
    }
}

/** ネットワークタスクへ通知を送る。タスクがまだ動いていなければ何もしない。 */
static void notify_task(uint32_t events) {
    rtos_task_handle_t h = s_network_task_handle;

    if (h != NULL) {
        rtos_task_notify(h, events);
    }
}

bool network_init(void) {
    json_init();
    if (s_ac_state_box == NULL) {
        s_ac_state_box = rtos_queue_create(AC_STATE_BOX_LEN, sizeof(ac_state_t));
    }
    if (s_command_box == NULL) {
        s_command_box = rtos_queue_create(COMMAND_BOX_LEN, sizeof(net_raw_command_t));
    }
    if (s_sensor_queue == NULL) {
        s_sensor_queue = rtos_queue_create(SENSOR_QUEUE_LEN, sizeof(sensor_data_t));
    }
    return (s_ac_state_box != NULL) && (s_command_box != NULL) && (s_sensor_queue != NULL);
}

bool network_report_ac_state(const ac_state_t *state) {
    if (!rtos_queue_overwrite(s_ac_state_box, state)) {
        return false;
    }
    notify_task(NET_EVENT_AC_STATE_CHANGED);
    return true;
}

bool network_report_sensor(float temperature, float humidity) {
    const sensor_data_t sample = {
        .temperature = temperature,
        .humidity    = humidity,
    };

    if (!rtos_queue_send(s_sensor_queue, &sample, 0)) {
        return false;
    }
    notify_task(NET_EVENT_SENSOR_QUEUED);
    return true;
}

/** 現在の状態に応じて、期限を設定する。 */
static void set_deadline(net_ctx_t *ctx) {
    uint32_t duration_ms = 0u;

    switch (ctx->state) {
    case NET_STATE_WIFI_WAIT:    duration_ms = NET_WIFI_CONNECT_TIMEOUT_MS; break;
    case NET_STATE_MQTT_WAIT:    duration_ms = NET_MQTT_CONNECT_TIMEOUT_MS; break;
    case NET_STATE_MQTT_PROCESS: duration_ms = NET_LINK_CHECK_INTERVAL_MS; break;
    case NET_STATE_RETRY_DELAY:
        duration_ms = sys_backoff_delay_ms(NET_BACKOFF_FIRST_DELAY_MS, NET_BACKOFF_LIMIT_DELAY_MS,
                                           ctx->retry_count, NET_BACKOFF_SHIFT_LIMIT);
        break;
    case NET_STATE_TEARDOWN:
    default:                     duration_ms = 0u; break;
    }
    ctx->deadline_ms = sys_deadline_from_now(duration_ms);
}

/** WiFiの接続状態の変化を通知する。 */
static void on_wifi_status_changed(bool connected) {
    notify_task(connected ? NET_EVENT_WIFI_CONNECTED : NET_EVENT_WIFI_DISCONNECTED);
}

/** MQTTの接続状態の変化を通知する。 */
static void on_mqtt_status_changed(bool connected) {
    notify_task(connected ? NET_EVENT_MQTT_CONNECTED : NET_EVENT_MQTT_DISCONNECTED);
}

/** 受信したメッセージが運転操作なら、ペイロードを写して通知する。 */
static void on_mqtt_message(const char *topic, const uint8_t *payload, uint16_t len) {
    static net_raw_command_t raw;
    uint16_t copy_len;

    if ((topic == NULL) || (payload == NULL) || (strcmp(topic, MQTT_TOPIC_CMD) != 0)) {
        return;
    }
    copy_len = (len < sizeof(raw.data)) ? len : (uint16_t)sizeof(raw.data);
    raw.len  = len;
    (void)memcpy(raw.data, payload, copy_len);

    if (rtos_queue_overwrite(s_command_box, &raw)) {
        notify_task(NET_EVENT_COMMAND_RECEIVED);
    }
}

/** 受信した運転操作があれば、解析してcontrolタスクへ渡す。 */
static void handle_command(void) {
    net_raw_command_t raw;

    if (!rtos_queue_receive(s_command_box, &raw, 0)) {
        return;
    }
    (void)command_handle_payload(raw.data, raw.len);
}

/** 測定値のキューから、送信できた分だけ取り出してac/sensorへ送る。 */
static void publish_pending(net_ctx_t *ctx) {
    sensor_data_t sample;
    char payload[SENSOR_PAYLOAD_MAX_LEN];
    size_t len;
    uint8_t sent = 0u;
    ctx->backlog = false;
    while (sent < MAX_PUBLISH_PER_LOOP) {
        if (!rtos_queue_peek(s_sensor_queue, &sample, 0)) {
            return;
        }
        len = payload_format_sensor(sample.temperature, sample.humidity, payload, sizeof(payload));
        if (len == 0u) {
            LOG_ERROR("Sensor sample could not be formatted. Dropped.");
            (void)rtos_queue_receive(s_sensor_queue, &sample, 0);
            continue;
        }
        if (!mqtt_publish_sensor(payload, (uint16_t)len)) {
            ctx->publish_fail_count++;
            LOG_WARN("Publish failed (%u consecutive)", (unsigned)ctx->publish_fail_count);
            return;
        }
        (void)rtos_queue_receive(s_sensor_queue, &sample, 0);
        ctx->publish_fail_count = 0u;
        sent++;
    }
    ctx->backlog = rtos_queue_peek(s_sensor_queue, &sample, 0);
}

/** 必要に応じてac/statusのonlineとac/stateを送る。 */
static void publish_status(net_ctx_t *ctx) {
    ac_state_t state;
    char payload[STATE_PAYLOAD_MAX_LEN];
    size_t len;

    if (ctx->online_pending) {
        if (mqtt_publish_online()) {
            ctx->online_pending = false;
        } else {
            ctx->publish_fail_count++;
            LOG_WARN("Online status publish failed (%u consecutive)", (unsigned)ctx->publish_fail_count);
            return;
        }
    }

    if (!ctx->state_pending) {
        return;
    }
    if (!rtos_queue_peek(s_ac_state_box, &state, 0)) {
        ctx->state_pending = false;
        return;
    }
    len = payload_format_state(&state, payload, sizeof(payload));
    if (len == 0u) {
        LOG_ERROR("AC state could not be formatted. Not sent.");
        ctx->state_pending = false;
        return;
    }
    if (!mqtt_publish_state(payload, (uint16_t)len)) {
        ctx->publish_fail_count++;
        LOG_WARN("AC state publish failed (%u consecutive)", (unsigned)ctx->publish_fail_count);
        return;
    }
    ctx->state_pending = false;
    LOG_INFO("Published AC state: %s", payload);
}

/** Wi-Fiの接続を開始する。 */
static net_state_t net_state_wifi_init(net_ctx_t *ctx) {
    LOG_INFO("Starting Wi-Fi connection (retry=%u)", (unsigned)ctx->retry_count);
    if (wifi_is_connected()) {
        LOG_INFO("Wi-Fi already connected.");
        return NET_STATE_MQTT_INIT;
    }
    if (wifi_start_connect(on_wifi_status_changed)) {
        return NET_STATE_WIFI_WAIT;
    }
    LOG_WARN("wifi_start_connect() failed.");
    return NET_STATE_TEARDOWN;
}

/** Wi-Fiの接続に失敗した理由を返す。 */
static const char *wifi_fail_reason_name(wifi_fail_reason_t reason) {
    switch (reason) {
    case WIFI_FAIL_NONE:            return "none";
    case WIFI_FAIL_BAD_CREDENTIALS: return "bad credentials";
    case WIFI_FAIL_AP_NOT_FOUND:    return "AP not found";
    case WIFI_FAIL_GENERIC:
    default:                        return "unknown";
    }
}

/** Wi-Fiの接続の完了・失敗を判定する。 */
static net_state_t net_state_wifi_wait(net_ctx_t *ctx, uint32_t events, bool timed_out) {
    (void)ctx;
    if (events & NET_EVENT_WIFI_DISCONNECTED) {
        LOG_WARN("Wi-Fi connection failed during wait.");
        return NET_STATE_TEARDOWN;
    }
    if (events & NET_EVENT_WIFI_CONNECTED) {
        LOG_INFO("Wi-Fi connected.");
        return NET_STATE_MQTT_INIT;
    }
    if (timed_out) {
        LOG_WARN("Wi-Fi connection timed out (%u ms, reason: %s).",
                 (unsigned)NET_WIFI_CONNECT_TIMEOUT_MS, wifi_fail_reason_name(wifi_get_fail_reason()));
        return NET_STATE_TEARDOWN;
    }
    return NET_STATE_WIFI_WAIT;
}

/** MQTTの接続を開始する。 */
static net_state_t net_state_mqtt_init(net_ctx_t *ctx) {
    (void)ctx;
    LOG_INFO("Starting MQTT connection...");
    if (!wifi_is_connected()) {
        LOG_WARN("Wi-Fi down before MQTT init.");
        return NET_STATE_TEARDOWN;
    }
    if (mqtt_start_connect(on_mqtt_status_changed, on_mqtt_message)) {
        return NET_STATE_MQTT_WAIT;
    }
    LOG_WARN("mqtt_start_connect() failed.");
    return NET_STATE_TEARDOWN;
}

/** MQTTの接続の完了・失敗を判定する。 */
static net_state_t net_state_mqtt_wait(net_ctx_t *ctx, uint32_t events, bool timed_out) {
    if (events & NET_EVENT_WIFI_DISCONNECTED) {
        LOG_WARN("Wi-Fi lost while waiting for MQTT.");
        return NET_STATE_TEARDOWN;
    }
    if (events & NET_EVENT_MQTT_DISCONNECTED) {
        LOG_WARN("MQTT connection failed during wait.");
        return NET_STATE_TEARDOWN;
    }
    if (events & NET_EVENT_MQTT_CONNECTED) {
        LOG_INFO("MQTT connected.");
        ctx->retry_count = 0u;
        ctx->publish_fail_count = 0u;
        ctx->online_pending = true;
        ctx->state_pending  = true;
        return NET_STATE_MQTT_PROCESS;
    }
    if (timed_out) {
        LOG_WARN("MQTT connection timed out (%u ms).",
                 (unsigned)NET_MQTT_CONNECT_TIMEOUT_MS);
        return NET_STATE_TEARDOWN;
    }
    return NET_STATE_MQTT_WAIT;
}

/** 状態と測定値を送信し、接続が保たれているかを確認する。 */
static net_state_t net_state_mqtt_process(net_ctx_t *ctx, uint32_t events) {
    if (events & NET_EVENT_WIFI_DISCONNECTED) {
        LOG_WARN("Wi-Fi link lost.");
        return NET_STATE_TEARDOWN;
    }
    if (events & NET_EVENT_MQTT_DISCONNECTED) {
        LOG_WARN("MQTT link lost.");
        return NET_STATE_TEARDOWN;
    }
    if (events & NET_EVENT_AC_STATE_CHANGED) {
        ctx->state_pending = true;
    }
    publish_status(ctx);
    publish_pending(ctx);
    if (ctx->publish_fail_count >= MAX_PUBLISH_FAILURES) {
        LOG_WARN("Too many publish failures. Recycling link.");
        return NET_STATE_TEARDOWN;
    }
    if (!wifi_is_connected() || !mqtt_is_connected()) {
        LOG_WARN("Link health check failed.");
        return NET_STATE_TEARDOWN;
    }
    return NET_STATE_MQTT_PROCESS;
}

/** offlineを送ってからMQTTを切断し、WiFiが切れていればWiFiも止める。 */
static net_state_t net_state_teardown(net_ctx_t *ctx) {
    LOG_INFO("Tearing down network resources...");
    if (mqtt_is_connected()) {
        (void)mqtt_publish_offline();
    }
    mqtt_stop();
    if (!wifi_is_connected()) {
        wifi_stop();
    }
    ctx->publish_fail_count = 0u;
    ctx->backlog = false;

    return NET_STATE_RETRY_DELAY;
}

/** 待ち時間が過ぎたら、WiFiの状態に応じて再接続を始める。 */
static net_state_t net_state_retry_delay(net_ctx_t *ctx, uint32_t events, bool timed_out) {
    (void)events;
    if (!timed_out) {
        return NET_STATE_RETRY_DELAY;
    }
    if (ctx->retry_count < UINT8_MAX) {
        ctx->retry_count++;
    }
    return wifi_is_connected() ? NET_STATE_MQTT_INIT : NET_STATE_WIFI_INIT;
}

void network_task(void *pvParameters) {
    (void)pvParameters;
    net_ctx_t ctx = {
        .state              = NET_STATE_WIFI_INIT,
        .deadline_ms        = 0u,
        .wdt_id             = 0u,
        .retry_count        = 0u,
        .publish_fail_count = 0u,
        .backlog            = false,
        .online_pending     = false,
        .state_pending      = false,
    };
    uint32_t notified_events = 0u;

    s_network_task_handle = rtos_task_get_current_handle();
    ctx.wdt_id = wdt_register_task("Network", NET_WDT_RUNNING_LIMIT_MS, NET_WDT_WAITING_LIMIT_MS);
    if (ctx.wdt_id == WDT_INVALID_ID) {
        LOG_ERROR("wdt_register_task() failed. Network task aborting.");
        s_network_task_handle = NULL;
        rtos_task_delete_current();
        return;
    }
    set_deadline(&ctx);

    while (true) {
        wdt_task_set_state(ctx.wdt_id, WDT_TASK_RUNNING);
        const uint32_t now = sys_get_time_ms();
        const bool timed_out = sys_deadline_expired(now, ctx.deadline_ms);
        const net_state_t prev_state = ctx.state;
        net_state_t next_state;

        handle_command();

        switch (ctx.state) {
        case NET_STATE_WIFI_INIT:
            next_state = net_state_wifi_init(&ctx);
            break;
        case NET_STATE_WIFI_WAIT:
            next_state = net_state_wifi_wait(&ctx, notified_events, timed_out);
            break;
        case NET_STATE_MQTT_INIT:
            next_state = net_state_mqtt_init(&ctx);
            break;
        case NET_STATE_MQTT_WAIT:
            next_state = net_state_mqtt_wait(&ctx, notified_events, timed_out);
            break;
        case NET_STATE_MQTT_PROCESS:
            next_state = net_state_mqtt_process(&ctx, notified_events);
            break;
        case NET_STATE_TEARDOWN:
            next_state = net_state_teardown(&ctx);
            break;
        case NET_STATE_RETRY_DELAY:
            next_state = net_state_retry_delay(&ctx, notified_events, timed_out);
            break;
        default:
            LOG_ERROR("Invalid state %d. Recovering.", (int)ctx.state);
            next_state = NET_STATE_TEARDOWN;
            break;
        }

        ctx.state = next_state;
        LOG_DEBUG("loop took %lu ms", (unsigned long)(sys_get_time_ms() - now));

        if (next_state != prev_state) {
            LOG_INFO("NET: %s -> %s", state_name(prev_state), state_name(next_state));
            set_deadline(&ctx);
        } else if (timed_out) {
            set_deadline(&ctx);
        }

        uint32_t wait_ms = ctx.backlog ? 0u : sys_deadline_remaining(sys_get_time_ms(), ctx.deadline_ms);

        if (wait_ms > NET_WAIT_SLICE_MS) {
            wait_ms = NET_WAIT_SLICE_MS;
        }
        if (wait_ms > 0u) {
            wdt_task_set_state(ctx.wdt_id, WDT_TASK_WAITING);
        }
        if (!rtos_task_notify_wait(NET_EVENT_NONE, NET_EVENT_ALL, &notified_events, wait_ms)) {
            notified_events = 0u;
        }
    }
}
