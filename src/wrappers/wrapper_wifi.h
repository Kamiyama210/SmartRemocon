#ifndef WRAPPER_WIFI_H
#define WRAPPER_WIFI_H

#include <stdint.h>
#include <stdbool.h>

/** Wi-Fiの接続状態の変化を受け取るコールバック。lwIPの処理の中から呼ばれる。 */
typedef void (*wrapper_wifi_status_cb_t)(bool connected);

/** Wi-Fiの接続状態。 */
typedef enum {
    WRAPPER_WIFI_LINK_DOWN = 0,   ///< 未接続
    WRAPPER_WIFI_LINK_JOINING,    ///< 接続処理中
    WRAPPER_WIFI_LINK_NO_IP,      ///< 認証は通ったがIPアドレスが未取得
    WRAPPER_WIFI_LINK_UP,         ///< 使用できる
    WRAPPER_WIFI_LINK_FAIL,       ///< 接続に失敗した
    WRAPPER_WIFI_LINK_NO_NET,     ///< アクセスポイントが見つからない
    WRAPPER_WIFI_LINK_BAD_AUTH    ///< 認証に失敗した
} wrapper_wifi_link_t;

/** Wi-Fiのハードウェアを初期化し、接続状態の変化の通知を登録する。初期化済みなら通知先だけ更新する。 */
bool wrapper_wifi_init(wrapper_wifi_status_cb_t status_cb);

/** アクセスポイントへの接続を開始し、完了を待たずに戻る。 */
bool wrapper_wifi_connect(const char *ssid, const char *password);

/** アクセスポイントから切断する。 */
void wrapper_wifi_disconnect(void);

/** Wi-Fiが使用できる状態か。 */
bool wrapper_wifi_is_link_up(void);

/** 現在の接続状態を返す。 */
wrapper_wifi_link_t wrapper_wifi_get_link_status(void);

#endif /* WRAPPER_WIFI_H */
