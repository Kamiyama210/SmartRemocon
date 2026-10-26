#include "ir_tx.h"
#include "wrapper_pio_ir.h"
#include "board_config.h"
#include "logger.h"

#include <stddef.h>

#define IR_CARRIER_HZ        (38000u)   ///< 搬送波の周波数。家電用リモコンの標準
#define MIN_CYCLES           (1u)       ///< 1要素の最小の周期数。0周期は送れないため

static uint32_t s_pio_words[IR_TX_MAX_PULSES];   ///< PIOへ渡すデータ（DMAが読む）
static bool     s_initialized = false;        ///< 初期化が済んでいるか

bool ir_tx_init(void) {
    if (!wrapper_pio_ir_init(GPIO_IR_TX, IR_CARRIER_HZ)) {
        LOG_ERROR("IR PIO init failed");
        return false;
    }
    s_initialized = true;
    LOG_INFO("IR TX ready (carrier %u Hz, %u us/cycle)",
             (unsigned)IR_CARRIER_HZ,
             (unsigned)wrapper_pio_ir_period_us());
    return true;
}

/**
 * マイクロ秒の値を搬送波の周期数に変換してPIO用のデータを作り、DMAで送信を始める。
 */
bool ir_tx_send(const uint16_t *durations_us, uint16_t length) {
    uint32_t period_us;

    if (!s_initialized) {
        LOG_ERROR("ir_tx_send before init");
        return false;
    }
    if ((durations_us == NULL) || (length == 0u)) {
        return false;
    }
    if (length > IR_TX_MAX_PULSES) {
        LOG_ERROR("IR pulse count %u exceeds buffer %u",
                  (unsigned)length, (unsigned)IR_TX_MAX_PULSES);
        return false;
    }

    if (!wrapper_pio_ir_is_done()) {
        LOG_WARN("IR transmit still in progress");
        return false;
    }

    period_us = wrapper_pio_ir_period_us();
    if (period_us == 0u) {
        return false;
    }

    for (uint16_t i = 0u; i < length; i++) {
        uint32_t cycles = (uint32_t)durations_us[i] / period_us;

        if (cycles < MIN_CYCLES) {
            cycles = MIN_CYCLES;
        }

        s_pio_words[i] = ((i % IR_TX_ENTRIES_PER_PULSE) == 0u)
                       ? (WRAPPER_PIO_IR_MARK_FLAG | cycles)
                       : cycles;
    }

    if (!wrapper_pio_ir_start(s_pio_words, length)) {
        LOG_ERROR("IR DMA start failed");
        return false;
    }
    return true;
}

bool ir_tx_is_busy(void) {
    return s_initialized && (!wrapper_pio_ir_is_done());
}
