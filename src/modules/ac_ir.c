#include "ac_ir.h"
#include "logger.h"
#include "ir_tx.h"
#include "sys.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IR_T_US               (425u)           ///< パルス幅の基本単位T（µs）。家製協（AEHA）フォーマットの値
#define IR_BIT_MARK_US        (IR_T_US)        ///< ビットの点灯時間（1T）
#define IR_ZERO_SPACE_US      (IR_T_US)        ///< ビット0の消灯時間（1T）
#define IR_ONE_SPACE_US       (IR_T_US * 3u)   ///< ビット1の消灯時間（3T）
#define IR_LEADER_MARK_US     (IR_T_US * 8u)   ///< リーダの点灯時間（8T）
#define IR_LEADER_SPACE_US    (IR_T_US * 4u)   ///< リーダの消灯時間（4T）
#define IR_FRAME_GAP_US       (10000u)         ///< フレーム間の消灯時間。8ms以上必要

#define TRAILER_ENTRIES       (1u)             ///< 最後の点灯が使う要素数

#define FRAME1_LEN            (8u)             ///< フレーム1のバイト数
#define FRAME2_LEN            (19u)            ///< フレーム2のバイト数

#define F2_IDX_POWER_MODE     (5u)             ///< フレーム2の、電源と運転モードのバイト
#define F2_IDX_TEMP           (6u)             ///< フレーム2の、設定温度のバイト
#define F2_IDX_FAN_DIR        (8u)             ///< フレーム2の、風量と風向のバイト
#define F2_IDX_TEMP_LIMIT     (14u)            ///< フレーム2の、設定温度が上限か下限かを表すバイト
#define F2_IDX_CHECKSUM       (FRAME2_LEN - 1u)   ///< フレーム2の、チェックサムのバイト（最後）

#define F2_POWER_ON_BIT       (0x01u)          ///< 電源を入れるビット
#define F2_MODE_SHIFT         (4u)             ///< 運転モードの位置（上位4ビット）
#define F2_MODE_MASK          (0xF0u)          ///< 運転モードの範囲
#define F2_TEMP_SHIFT         (1u)             ///< 設定温度の位置（ビット1から）
#define F2_TEMP_MASK          (0x1Eu)          ///< 設定温度の範囲（4ビット）
#define F2_FAN_SHIFT          (4u)             ///< 風量の位置（上位4ビット）
#define F2_FAN_MASK           (0xF0u)          ///< 風量の範囲
#define F2_DIR_MASK           (0x0Fu)          ///< 風向の範囲（下位4ビット）
#define F2_TEMP_LIMIT_BIT     (0x02u)          ///< 設定温度が上限か下限のときに立てるビット

#define SIGNAL_ENTRIES        (439u)           ///< 1回の信号に必要なパルス列の要素数

_Static_assert(SIGNAL_ENTRIES ==
                   (IR_TX_ENTRIES_PER_PULSE                                     /* リーダ */
                    + (FRAME1_LEN * BITS_PER_BYTE * IR_TX_ENTRIES_PER_PULSE)    /* フレーム1 */
                    + IR_TX_ENTRIES_PER_PULSE                                   /* フレーム間 */
                    + IR_TX_ENTRIES_PER_PULSE                                   /* リーダ */
                    + (FRAME2_LEN * BITS_PER_BYTE * IR_TX_ENTRIES_PER_PULSE)    /* フレーム2 */
                    + TRAILER_ENTRIES),                                         /* 最後の点灯 */
               "SIGNAL_ENTRIES must match the frame layout");
_Static_assert(SIGNAL_ENTRIES <= IR_TX_MAX_PULSES, "AC signal does not fit in the IR transmit buffer");

static uint16_t s_pulse_buffer[SIGNAL_ENTRIES];   ///< 送信するパルス列。送信が終わるまで保持する

/** フレーム1。内容は毎回同じ */
static const uint8_t FRAME1[FRAME1_LEN] = {
    0x02u, 0x20u, 0xE0u, 0x04u, 0x00u, 0x00u, 0x00u, 0x06u
};

/** フレーム2のひな形。電源・運転モード・設定温度・風量・風向・チェックサムは送信のたびに書き込む */
static const uint8_t FRAME2_TEMPLATE[FRAME2_LEN] = {
    0x02u, 0x20u, 0xE0u, 0x04u, 0x00u, 0x00u, 0x20u, 0x80u, 0x00u, 0x00u,
    0x00u, 0x06u, 0x60u, 0x00u, 0x00u, 0x80u, 0x00u, 0x06u, 0x00u
};

/** 運転モードが、送信に対応している値かを返す。 */
static bool is_known_mode(ac_mode_t mode) {
    return (mode == AC_MODE_COOL) ||
           (mode == AC_MODE_HEAT);
}

_Static_assert((uint8_t)AC_MODE_COOL <= (F2_MODE_MASK >> F2_MODE_SHIFT), "AC_MODE_COOL must fit in 4 bits");
_Static_assert((uint8_t)AC_MODE_HEAT <= (F2_MODE_MASK >> F2_MODE_SHIFT), "AC_MODE_HEAT must fit in 4 bits");
_Static_assert((AC_TEMP_MAX_C - AC_TEMP_MIN_C) <= (F2_TEMP_MASK >> F2_TEMP_SHIFT), "AC temperature range must fit in 4 bits");

/** 点灯・消灯の組を1つ追加し、次の書き込み位置を返す。 */
static uint16_t add_pulse(uint16_t *buffer, uint16_t index,
                          uint16_t on_time, uint16_t off_time) {
    buffer[index++] = on_time;
    buffer[index++] = off_time;
    return index;
}

/** 1バイトを下位ビットから順にパルスにして追加し、次の書き込み位置を返す。 */
static uint16_t add_byte(uint16_t *buffer, uint16_t index, uint8_t data) {
    for (uint8_t i = 0u; i < BITS_PER_BYTE; i++) {
        if ((data & (1u << i)) != 0u) {
            index = add_pulse(buffer, index, IR_BIT_MARK_US, IR_ONE_SPACE_US);
        } else {
            index = add_pulse(buffer, index, IR_BIT_MARK_US, IR_ZERO_SPACE_US);
        }
    }
    return index;
}

/** エアコンの設定からパルス列（マイクロ秒の点灯・消灯の並び）を作り、要素数を返す。失敗したら0。 */
static uint16_t generate_signal(const ac_state_t *state, uint16_t *pulse_buffer) {
    uint16_t idx = 0u;
    uint8_t  temp_c;
    uint8_t  temp_val;
    uint8_t  checksum = 0u;
    uint8_t  frame2[FRAME2_LEN];

    if (!is_known_mode(state->mode)) {
        LOG_ERROR("Unsupported AC mode: 0x%02X", (unsigned)state->mode);
        return 0u;
    }

    temp_c = state->temperature;
    if (temp_c < AC_TEMP_MIN_C) {
        LOG_WARN("Temperature %u below min, clamped to %u",
                 (unsigned)temp_c, (unsigned)AC_TEMP_MIN_C);
        temp_c = AC_TEMP_MIN_C;
    } else if (temp_c > AC_TEMP_MAX_C) {
        LOG_WARN("Temperature %u above max, clamped to %u",
                 (unsigned)temp_c, (unsigned)AC_TEMP_MAX_C);
        temp_c = AC_TEMP_MAX_C;
    }
    temp_val = (uint8_t)(temp_c - AC_TEMP_MIN_C);

    for (uint8_t i = 0u; i < FRAME2_LEN; i++) {
        frame2[i] = FRAME2_TEMPLATE[i];
    }

    if (state->power_on) {
        frame2[F2_IDX_POWER_MODE] |= F2_POWER_ON_BIT;
    }
    frame2[F2_IDX_POWER_MODE] |= (uint8_t)(((uint8_t)state->mode << F2_MODE_SHIFT) & F2_MODE_MASK);

    frame2[F2_IDX_TEMP] |= (uint8_t)((temp_val << F2_TEMP_SHIFT) & F2_TEMP_MASK);

    frame2[F2_IDX_FAN_DIR] = (uint8_t)(((state->fan << F2_FAN_SHIFT) & F2_FAN_MASK) | (state->dir & F2_DIR_MASK));

    if ((temp_c == AC_TEMP_MIN_C) || (temp_c == AC_TEMP_MAX_C)) {
        frame2[F2_IDX_TEMP_LIMIT] |= F2_TEMP_LIMIT_BIT;
    }

    /* チェックサムは、チェックサムより前の全バイトの和の下位8ビット */
    for (uint8_t i = 0u; i < F2_IDX_CHECKSUM; i++) {
        checksum = (uint8_t)(checksum + frame2[i]);
    }
    frame2[F2_IDX_CHECKSUM] = checksum;

    idx = add_pulse(pulse_buffer, idx, IR_LEADER_MARK_US, IR_LEADER_SPACE_US);
    for (uint8_t i = 0u; i < FRAME1_LEN; i++) {
        idx = add_byte(pulse_buffer, idx, FRAME1[i]);
    }
    idx = add_pulse(pulse_buffer, idx, IR_BIT_MARK_US, IR_FRAME_GAP_US);

    idx = add_pulse(pulse_buffer, idx, IR_LEADER_MARK_US, IR_LEADER_SPACE_US);
    for (uint8_t i = 0u; i < FRAME2_LEN; i++) {
        idx = add_byte(pulse_buffer, idx, frame2[i]);
    }
    pulse_buffer[idx++] = IR_BIT_MARK_US;

    return idx;
}

bool ac_ir_init(void) {
    return ir_tx_init();
}

/** パルス列を作って送信を始める。前回の送信中かどうかは、ir_tx_sendが確かめる。 */
bool ac_ir_send(const ac_state_t *state) {
    uint16_t len;

    if (state == NULL) {
        return false;
    }
    len = generate_signal(state, s_pulse_buffer);
    if (len == 0u) {
        LOG_ERROR("IR encode failed (unsupported state)");
        return false;
    }
    if (!ir_tx_send(s_pulse_buffer, len)) {
        LOG_ERROR("IR transmit start failed");
        return false;
    }
    return true;
}

bool ac_ir_is_busy(void) {
    return ir_tx_is_busy();
}

