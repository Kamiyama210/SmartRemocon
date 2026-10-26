#ifndef IR_TX_H
#define IR_TX_H

#include <stdint.h>
#include <stdbool.h>

#define IR_TX_MAX_PULSES          (512u)   ///< 1回に送信できるパルス列の最大の要素数。エアコンの信号439個が収まる
#define IR_TX_ENTRIES_PER_PULSE   (2u)     ///< 点灯と消灯の組1つが使う要素数

/** IR送信（PIOとDMA）を初期化する。確保に失敗したらfalse。 */
bool ir_tx_init(void);

/**
 * パルス列の送信を開始し、完了を待たずに戻る。
 * durations_usは点灯時間・消灯時間・点灯時間…の順に並んだマイクロ秒の値。
 * 引数が不正、要素数が多すぎる、または前回の送信中ならfalse。
 */
bool ir_tx_send(const uint16_t *durations_us, uint16_t length);

/** 送信中か。 */
bool ir_tx_is_busy(void);

#endif /* IR_TX_H */
