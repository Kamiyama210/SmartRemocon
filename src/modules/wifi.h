#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include <stdbool.h>

/** Wi-Fiの接続状態の変化を受け取るコールバック。lwIPの処理の中から呼ばれる。 */
typedef void (*wifi_status_cb_t)(bool connected);

/** Wi-Fiの接続に失敗した理由。 */
typedef enum {
    WIFI_FAIL_NONE = 0,          ///< 失敗していない
    WIFI_FAIL_BAD_CREDENTIALS,   ///< 認証に失敗した
    WIFI_FAIL_AP_NOT_FOUND,      ///< アクセスポイントが見つからない
    WIFI_FAIL_GENERIC            ///< その他の失敗
} wifi_fail_reason_t;

/** Wi-Fiの接続を開始する。必要ならハードウェアも初期化する。結果はstatus_cbで通知される。 */
bool wifi_start_connect(wifi_status_cb_t status_cb);

/** Wi-Fiを切断する。 */
void wifi_stop(void);

/** Wi-Fiに接続しているか。 */
bool wifi_is_connected(void);

/** 接続に失敗した理由を返す。 */
wifi_fail_reason_t wifi_get_fail_reason(void);

#endif /* WIFI_H */
