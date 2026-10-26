#include "wrapper_pio_ir.h"
#include "wrapper_common.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#include "ir_tx.pio.h"

#define US_PER_S        (1000000u)   ///< 1秒のマイクロ秒

static PIO      s_pio         = NULL;    ///< 使用するPIO
static uint     s_sm          = 0u;      ///< 使用するステートマシン
static uint     s_offset      = 0u;      ///< PIOプログラムの配置位置
static int      s_dma_chan    = -1;      ///< 使用するDMAチャネル
static uint32_t s_carrier_hz  = 0u;      ///< 搬送波の周波数
static bool     s_initialized = false;   ///< 初期化が済んでいるか

/**
 * 空いているPIOのステートマシンとDMAチャネルを確保し、PIOプログラムを読み込む。
 * DMAは配列を順に読み、PIOの送信FIFOに空きができるたびに1件ずつ書き込むよう設定する。
 */
bool wrapper_pio_ir_init(uint8_t pin, uint32_t carrier_hz) {
    dma_channel_config dma_cfg;

    if (s_initialized) {
        return true;
    }
    if ((pin > MAX_GPIO_PIN) || (carrier_hz == 0u)) {
        return false;
    }

    if ((carrier_hz * ir_tx_CYCLES_PER_CARRIER) > clock_get_hz(clk_sys)) {
        return false;
    }

    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &ir_tx_program, &s_pio, &s_sm, &s_offset, pin, 1, true)) {
        return false;
    }

    ir_tx_program_init(s_pio, s_sm, s_offset, pin, carrier_hz);

    s_dma_chan = dma_claim_unused_channel(false);
    if (s_dma_chan < 0) {
        pio_remove_program_and_unclaim_sm(&ir_tx_program, s_pio, s_sm, s_offset);
        return false;
    }

    dma_cfg = dma_channel_get_default_config((uint)s_dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);

    channel_config_set_dreq(&dma_cfg, pio_get_dreq(s_pio, s_sm, true));

    dma_channel_configure((uint)s_dma_chan, &dma_cfg,
                          &s_pio->txf[s_sm],
                          NULL,
                          0,
                          false);

    s_carrier_hz  = carrier_hz;
    s_initialized = true;
    return true;
}

bool wrapper_pio_ir_start(const uint32_t *words, uint16_t count) {
    if (!s_initialized || (words == NULL) || (count == 0u)) {
        return false;
    }
    if (!wrapper_pio_ir_is_done()) {
        return false;
    }

    dma_channel_set_read_addr((uint)s_dma_chan, words, false);
    dma_channel_set_trans_count((uint)s_dma_chan, count, true);

    return true;
}

bool wrapper_pio_ir_is_done(void) {
    if (!s_initialized) {
        return true;
    }
    return (!dma_channel_is_busy((uint)s_dma_chan)) &&
           pio_sm_is_tx_fifo_empty(s_pio, s_sm);
}

uint32_t wrapper_pio_ir_period_us(void) {
    if (s_carrier_hz == 0u) {
        return 0u;
    }
    return US_PER_S / s_carrier_hz;
}
