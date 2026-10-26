#include "wrapper_wifi.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"

#include <string.h>

static wrapper_wifi_status_cb_t s_status_cb     = NULL;    ///< 接続状態の変化の通知先
static bool                     s_initialized   = false;   ///< 初期化が済んでいるか
static volatile bool            s_last_reported = false;   ///< 最後に通知した接続状態

/** 接続状態が前回の通知から変わったときだけ、上位へ通知する。 */
static void report_status(bool connected) {
    if (s_status_cb == NULL) {
        return;
    }
    if (connected == s_last_reported) {
        return;
    }
    s_last_reported = connected;
    s_status_cb(connected);
}

/** ネットワークインターフェースが使える状態（リンクが上がり、IPアドレスを取得済み）か。 */
static bool netif_is_usable(struct netif *netif) {
    return (netif != NULL) &&
           netif_is_link_up(netif) &&
           netif_is_up(netif) &&
           !ip4_addr_isany_val(*netif_ip4_addr(netif));
}

/** lwIPからのインターフェースの状態変化を受け取る。 */
static void on_netif_status(struct netif *netif) {
    report_status(netif_is_usable(netif));
}

/** lwIPからのリンクの状態変化を受け取る。 */
static void on_netif_link(struct netif *netif) {
    report_status(netif_is_usable(netif));
}

/** ステーションモードのネットワークインターフェースを返す。 */
static struct netif *sta_netif(void) {
    return &cyw43_state.netif[CYW43_ITF_STA];
}

bool wrapper_wifi_init(wrapper_wifi_status_cb_t status_cb) {
    struct netif *netif;

    if (s_initialized) {
        s_status_cb = status_cb;
        return true;
    }

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_JAPAN) != 0) {
        return false;
    }
    cyw43_arch_enable_sta_mode();

    s_status_cb     = status_cb;
    s_last_reported = false;

    netif = sta_netif();

    cyw43_arch_lwip_begin();
    netif_set_status_callback(netif, on_netif_status);
    netif_set_link_callback(netif, on_netif_link);
    cyw43_arch_lwip_end();

    s_initialized = true;
    return true;
}

bool wrapper_wifi_connect(const char *ssid, const char *password) {
    int err;

    if (!s_initialized) {
        return false;
    }
    if ((ssid == NULL) || (password == NULL)) {
        return false;
    }

    err = cyw43_arch_wifi_connect_async(ssid, password, CYW43_AUTH_WPA2_AES_PSK);
    return (err == 0);
}

void wrapper_wifi_disconnect(void) {
    if (!s_initialized) {
        return;
    }
    cyw43_arch_lwip_begin();
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_arch_lwip_end();

    s_last_reported = false;
}

bool wrapper_wifi_is_link_up(void) {
    int status;

    if (!s_initialized) {
        return false;
    }
    status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    return (status == CYW43_LINK_UP);
}

wrapper_wifi_link_t wrapper_wifi_get_link_status(void) {
    int status;

    if (!s_initialized) {
        return WRAPPER_WIFI_LINK_DOWN;
    }
    status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    switch (status) {
    case CYW43_LINK_DOWN:    return WRAPPER_WIFI_LINK_DOWN;
    case CYW43_LINK_JOIN:    return WRAPPER_WIFI_LINK_JOINING;
    case CYW43_LINK_NOIP:    return WRAPPER_WIFI_LINK_NO_IP;
    case CYW43_LINK_UP:      return WRAPPER_WIFI_LINK_UP;
    case CYW43_LINK_FAIL:    return WRAPPER_WIFI_LINK_FAIL;
    case CYW43_LINK_NONET:   return WRAPPER_WIFI_LINK_NO_NET;
    case CYW43_LINK_BADAUTH: return WRAPPER_WIFI_LINK_BAD_AUTH;
    default:                 return WRAPPER_WIFI_LINK_FAIL;
    }
}
