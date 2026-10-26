#include "wifi.h"
#include "wrapper_wifi.h"
#include "wifi_credentials.h"
#include "logger.h"

static bool             s_hw_initialized = false;   ///< Wi-Fiのハードウェアを初期化したか
static wifi_status_cb_t s_status_cb      = NULL;    ///< 接続状態の変化の通知先

/** 接続状態の変化を上位へ通知する。 */
static void on_status_changed(bool connected) {
    if (s_status_cb != NULL) {
        s_status_cb(connected);
    }
}

bool wifi_start_connect(wifi_status_cb_t status_cb) {
    s_status_cb = status_cb;

    if (!s_hw_initialized) {
        LOG_INFO("Initializing Wi-Fi hardware...");
        if (!wrapper_wifi_init(on_status_changed)) {
            LOG_ERROR("Wi-Fi hardware init failed");
            return false;
        }
        s_hw_initialized = true;
    }

    LOG_INFO("Starting async connection to SSID: %s", WIFI_SSID);
    return wrapper_wifi_connect(WIFI_SSID, WIFI_PASSWORD);
}

void wifi_stop(void) {
    if (!s_hw_initialized) {
        return;
    }
    LOG_INFO("Disconnecting Wi-Fi...");
    wrapper_wifi_disconnect();
}

bool wifi_is_connected(void) {
    return wrapper_wifi_is_link_up();
}

wifi_fail_reason_t wifi_get_fail_reason(void) {
    switch (wrapper_wifi_get_link_status()) {
    case WRAPPER_WIFI_LINK_BAD_AUTH: return WIFI_FAIL_BAD_CREDENTIALS;
    case WRAPPER_WIFI_LINK_NO_NET:   return WIFI_FAIL_AP_NOT_FOUND;
    case WRAPPER_WIFI_LINK_UP:       return WIFI_FAIL_NONE;
    case WRAPPER_WIFI_LINK_FAIL:     return WIFI_FAIL_GENERIC;
    default:                         return WIFI_FAIL_GENERIC;
    }
}
