#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* OSとの連携 */
#define NO_SYS                      0   ///< OSあり（FreeRTOS）で動かす
#define LWIP_SOCKET                 0   ///< ソケットAPIは使わない
#define LWIP_NETCONN                0   ///< netconn APIは使わない

/* メモリ */
#define MEM_ALIGNMENT               4   ///< メモリを4バイト境界にそろえる（Cortex-M33）
#define MEM_SIZE                    (16 * 1024)   ///< lwIPのヒープの大きさ
#define MEMP_NUM_TCP_SEG            32   ///< 同時に扱えるTCPセグメントの数
#define MEMP_NUM_ARP_QUEUE          10   ///< 宛先のMACアドレスの解決を待つ間、保持できるパケットの数
#define MEMP_NUM_SYS_TIMEOUT        16   ///< lwIPの内部で同時に使えるタイマーの数
#define PBUF_POOL_SIZE              24   ///< 受信に使うバッファの数

/* プロトコル */
#define LWIP_ARP                    1   ///< IPアドレスからMACアドレスを調べる（ARP）
#define LWIP_ETHERNET               1   ///< イーサネットの形式のフレームを使う
#define LWIP_ICMP                   1   ///< pingに応答する

/* TCP */
#define TCP_WND                     (8 * TCP_MSS)   ///< 受信ウィンドウの大きさ。確認応答を待たずに受け取れる量
#define TCP_MSS                     1460   ///< 1セグメントの最大のデータ長。イーサネットの標準の値
#define TCP_SND_BUF                 (8 * TCP_MSS)   ///< 送信バッファの大きさ
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))   ///< 送信を待つセグメントの数。lwIPが推奨する式

/* ネットワークインターフェース */
#define LWIP_NETIF_STATUS_CALLBACK  1   ///< インターフェースの状態の変化を通知する
#define LWIP_NETIF_LINK_CALLBACK    1   ///< リンクの状態の変化を通知する
#define LWIP_NETIF_HOSTNAME         1   ///< DHCPでホスト名をルーターに伝える

/* lwIPのタスク */
#define TCPIP_THREAD_STACKSIZE      1024   ///< lwIPの処理を行うタスクのスタックサイズ
#define DEFAULT_THREAD_STACKSIZE    1024   ///< lwIPが作るそのほかのタスクのスタックサイズ
#define TCPIP_MBOX_SIZE             8   ///< lwIPのタスクへの依頼をためておける数
#define LWIP_TCPIP_CORE_LOCKING     1   ///< ほかのタスクから、ロックを取ってlwIPを直接呼べるようにする

/* IP */
#define LWIP_IPV4                   1   ///< IPv4を使う
#define LWIP_DHCP                   1   ///< DHCPでIPアドレスを取得する
#define LWIP_DNS                    0   ///< DNSは使わない

#endif /* _LWIPOPTS_H */
