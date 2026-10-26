#ifndef WRAPPER_PIO_IR_H
#define WRAPPER_PIO_IR_H

#include <stdint.h>
#include <stdbool.h>

#define WRAPPER_PIO_IR_MARK_FLAG   (1u << 31)   ///< 搬送波を出す（点灯する）ことを表すビット

/** PIOとDMAを初期化する。確保やクロックの設定に失敗したらfalse。初期化済みなら何もせずtrue。 */
bool wrapper_pio_ir_init(uint8_t pin, uint32_t carrier_hz);

/**
 * パルス列の送信をDMAで開始し、完了を待たずに戻る。
 * wordsの各要素は、bit31が点灯・消灯、bit30〜0が搬送波の周期数。送信が終わるまでwordsを保持しておくこと。
 * 引数が不正、または前回の送信中ならfalse。
 */
bool wrapper_pio_ir_start(const uint32_t *words, uint16_t count);

/** 送信が完了したか（DMAの転送が終わり、PIOのFIFOが空になったか）。 */
bool wrapper_pio_ir_is_done(void);

/** 搬送波の1周期のマイクロ秒を返す（切り捨て）。 */
uint32_t wrapper_pio_ir_period_us(void);

#endif /* WRAPPER_PIO_IR_H */
