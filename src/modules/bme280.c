#include "bme280.h"
#include "wrapper_i2c.h"
#include "wrapper_sys.h"
#include "board_config.h"
#include "logger.h"
#include "sys.h"

#include <stddef.h>

#define BME280_I2C_ADDR        (0x76u)     ///< I2Cアドレス（SDOをGNDに接続した場合）
#define BME280_CHIP_ID_VALUE   (0x60u)     ///< チップIDレジスタの正しい値（データシートの値）

#define BME280_REG_CALIB_T     (0x88u)     ///< 温度の補正係数の先頭レジスタ
#define BME280_REG_CALIB_H1    (0xA1u)     ///< 湿度の補正係数H1のレジスタ
#define BME280_REG_CALIB_H2    (0xE1u)     ///< 湿度の補正係数H2〜H6の先頭レジスタ
#define BME280_REG_CHIP_ID     (0xD0u)     ///< チップIDレジスタ
#define BME280_REG_RESET       (0xE0u)     ///< リセットレジスタ
#define BME280_REG_CTRL_HUM    (0xF2u)     ///< 湿度の測定設定レジスタ
#define BME280_REG_CTRL_MEAS   (0xF4u)     ///< 温度・気圧の測定設定レジスタ
#define BME280_REG_CONFIG      (0xF5u)     ///< 待機時間・フィルタの設定レジスタ
#define BME280_REG_DATA        (0xF7u)     ///< 測定値レジスタの先頭

#define BME280_RESET_CMD       (0xB6u)     ///< リセットレジスタに書くとリセットされる値（データシートの値）
#define BME280_RESET_WAIT_MS   (10u)       ///< リセット後の待ち時間。データシートの起動時間2msより長くする
#define BME280_CTRL_HUM_VALUE  (0x01u)     ///< 湿度：オーバーサンプリングx1
#define BME280_CTRL_MEAS_VALUE (0x27u)     ///< 温度x1、気圧x1、ノーマルモード
#define BME280_CONFIG_VALUE    (0xA0u)     ///< 待機時間1000ms、フィルタなし

#define BME280_RAW_TEMP_SKIP   (0x80000)   ///< 温度の測定が行われなかったときの生値（データシートの値）
#define BME280_RAW_HUM_SKIP    (0x8000)    ///< 湿度の測定が行われなかったときの生値（データシートの値）
#define BME280_CALIB_ERASED    (0xFFFFu)   ///< 補正係数が正しく読めなかったときの値

#define CALIB_T_LEN            (6u)        ///< 温度の補正係数のバイト数
#define CALIB_T1_OFS           (0u)        ///< 温度の補正係数の中での、T1の位置
#define CALIB_T2_OFS           (2u)        ///< 温度の補正係数の中での、T2の位置
#define CALIB_T3_OFS           (4u)        ///< 温度の補正係数の中での、T3の位置

#define CALIB_H_LEN            (7u)        ///< 湿度の補正係数H2〜H6のバイト数
#define CALIB_H2_OFS           (0u)        ///< 湿度の補正係数の中での、H2の位置
#define CALIB_H3_OFS           (2u)        ///< 湿度の補正係数の中での、H3の位置
#define CALIB_H4_MSB_OFS       (3u)        ///< 湿度の補正係数の中での、H4の上位の位置
#define CALIB_H45_SHARED_OFS   (4u)        ///< H4の下位4ビットとH5の下位4ビットが入っている位置
#define CALIB_H5_MSB_OFS       (5u)        ///< 湿度の補正係数の中での、H5の上位の位置
#define CALIB_H6_OFS           (6u)        ///< 湿度の補正係数の中での、H6の位置

#define DATA_LEN               (8u)        ///< 測定値レジスタのバイト数（気圧3、温度3、湿度2）
#define DATA_TEMP_MSB_OFS      (3u)        ///< 測定値の中での、温度の上位バイトの位置
#define DATA_TEMP_LSB_OFS      (4u)        ///< 測定値の中での、温度の中位バイトの位置
#define DATA_TEMP_XLSB_OFS     (5u)        ///< 測定値の中での、温度の下位4ビットの位置
#define DATA_HUM_MSB_OFS       (6u)        ///< 測定値の中での、湿度の上位バイトの位置
#define DATA_HUM_LSB_OFS       (7u)        ///< 測定値の中での、湿度の下位バイトの位置

#define NIBBLE_BITS            (4u)        ///< 4ビット分のシフト量
#define NIBBLE_MASK            (0x0Fu)     ///< 下位4ビットの範囲
#define TEMP_MSB_SHIFT         (12u)       ///< 温度の生値（20ビット）での、上位バイトの位置

/** センサー固有の補正係数。 */
typedef struct {
    uint16_t dig_T1;   ///< 温度の補正係数T1
    int16_t  dig_T2;   ///< 温度の補正係数T2
    int16_t  dig_T3;   ///< 温度の補正係数T3
    uint8_t  dig_H1;   ///< 湿度の補正係数H1
    int16_t  dig_H2;   ///< 湿度の補正係数H2
    uint8_t  dig_H3;   ///< 湿度の補正係数H3
    int16_t  dig_H4;   ///< 湿度の補正係数H4
    int16_t  dig_H5;   ///< 湿度の補正係数H5
    int8_t   dig_H6;   ///< 湿度の補正係数H6
} bme280_calib_data_t;

static bme280_calib_data_t s_calib;    ///< 読み出した補正係数
static bool    s_ready      = false;   ///< 初期化が済んでいるか

/** リトルエンディアンの2バイトを16ビットの値にする。 */
static uint16_t u16_le(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[1] << BITS_PER_BYTE) | p[0]);
}

/** レジスタに1バイト書き込む。 */
static bool write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[] = { reg, value };
    return wrapper_i2c_write(BME280_I2C_ADDR, buf, sizeof(buf), false);
}

/** 指定したレジスタから連続して読み出す。 */
static bool read_regs(uint8_t reg, uint8_t *buf, size_t len) {
    if (!wrapper_i2c_write(BME280_I2C_ADDR, &reg, sizeof(reg), true)) {
        return false;
    }
    return wrapper_i2c_read(BME280_I2C_ADDR, buf, len, false);
}

/** 補正係数を読み出す。値が明らかにおかしければfalse。 */
static bool read_calibration(void) {
    uint8_t buf[CALIB_T_LEN];
    uint8_t h_buf[CALIB_H_LEN];

    if (!read_regs(BME280_REG_CALIB_T, buf, sizeof(buf))) {
        return false;
    }
    s_calib.dig_T1 = u16_le(&buf[CALIB_T1_OFS]);
    s_calib.dig_T2 = (int16_t)u16_le(&buf[CALIB_T2_OFS]);
    s_calib.dig_T3 = (int16_t)u16_le(&buf[CALIB_T3_OFS]);

    if ((s_calib.dig_T1 == 0u) || (s_calib.dig_T1 == BME280_CALIB_ERASED)) {
        LOG_ERROR("BME280 calibration looks invalid (dig_T1=%u)",
                  (unsigned)s_calib.dig_T1);
        return false;
    }

    if (!read_regs(BME280_REG_CALIB_H1, &s_calib.dig_H1, sizeof(s_calib.dig_H1))) {
        return false;
    }
    if (!read_regs(BME280_REG_CALIB_H2, h_buf, sizeof(h_buf))) {
        return false;
    }
    s_calib.dig_H2 = (int16_t)u16_le(&h_buf[CALIB_H2_OFS]);
    s_calib.dig_H3 = h_buf[CALIB_H3_OFS];
    s_calib.dig_H4 = (int16_t)(((int16_t)h_buf[CALIB_H4_MSB_OFS] << NIBBLE_BITS) |
                               (h_buf[CALIB_H45_SHARED_OFS] & NIBBLE_MASK));
    s_calib.dig_H5 = (int16_t)(((int16_t)h_buf[CALIB_H5_MSB_OFS] << NIBBLE_BITS) |
                               (h_buf[CALIB_H45_SHARED_OFS] >> NIBBLE_BITS));
    s_calib.dig_H6 = (int8_t)h_buf[CALIB_H6_OFS];

    return true;
}

/**
 * I2Cを初期化し、チップIDの確認、リセット、補正係数の読み出し、測定の設定を行う。
 * 湿度の設定は温度・気圧の設定を書いた時点で反映されるため、CTRL_HUM、CTRL_MEASの順に書く。
 */
bool bme280_init(void) {
    uint8_t chip_id = 0u;

    s_ready = false;

    if (!wrapper_i2c_init(GPIO_I2C_SDA, GPIO_I2C_SCL, I2C_BAUDRATE_HZ)) {
        LOG_ERROR("I2C init failed");
        return false;
    }

    if (!read_regs(BME280_REG_CHIP_ID, &chip_id, sizeof(chip_id))) {
        LOG_ERROR("BME280 not responding");
        return false;
    }
    if (chip_id != BME280_CHIP_ID_VALUE) {
        LOG_ERROR("Unexpected chip ID: 0x%02X (expected 0x%02X)",
                  (unsigned)chip_id, (unsigned)BME280_CHIP_ID_VALUE);
        return false;
    }

    if (!write_reg(BME280_REG_RESET, BME280_RESET_CMD)) {
        LOG_ERROR("BME280 reset failed");
        return false;
    }
    wrapper_busy_wait_ms(BME280_RESET_WAIT_MS);

    if (!read_calibration()) {
        LOG_ERROR("BME280 calibration read failed");
        return false;
    }

    if (!write_reg(BME280_REG_CTRL_HUM, BME280_CTRL_HUM_VALUE) ||
        !write_reg(BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VALUE) ||
        !write_reg(BME280_REG_CONFIG, BME280_CONFIG_VALUE)) {
        LOG_ERROR("BME280 config write failed");
        return false;
    }

    s_ready = true;
    return true;
}

/** 測定値を読み出し、データシートの補正式（浮動小数点版）で温度と湿度を求める。 */
bool bme280_read(float *temperature, float *humidity) {
    uint8_t data[DATA_LEN];
    int32_t raw_temp;
    int32_t raw_hum;
    int32_t t_fine;
    float var1, var2, var_h;

    if (!s_ready || (temperature == NULL) || (humidity == NULL)) {
        return false;
    }

    if (!read_regs(BME280_REG_DATA, data, sizeof(data))) {
        return false;
    }

    raw_temp = (int32_t)(((uint32_t)data[DATA_TEMP_MSB_OFS] << TEMP_MSB_SHIFT) |
                         ((uint32_t)data[DATA_TEMP_LSB_OFS] << NIBBLE_BITS)    |
                         ((uint32_t)data[DATA_TEMP_XLSB_OFS] >> NIBBLE_BITS));
    raw_hum  = (int32_t)(((uint32_t)data[DATA_HUM_MSB_OFS] << BITS_PER_BYTE) | data[DATA_HUM_LSB_OFS]);

    if ((raw_temp == BME280_RAW_TEMP_SKIP) || (raw_hum == BME280_RAW_HUM_SKIP)) {
        LOG_WARN("BME280 measurement skipped (not configured?)");
        return false;
    }

    /* BME280データシート 8.1「Compensation formulas in double precision floating point」の引用 */
    var1 = (((float)raw_temp) / 16384.0f - ((float)s_calib.dig_T1) / 1024.0f) *
           ((float)s_calib.dig_T2);
    var2 = ((((float)raw_temp) / 131072.0f - ((float)s_calib.dig_T1) / 8192.0f) *
            (((float)raw_temp) / 131072.0f - ((float)s_calib.dig_T1) / 8192.0f)) *
           ((float)s_calib.dig_T3);

    t_fine       = (int32_t)(var1 + var2);
    *temperature = (var1 + var2) / 5120.0f;

    var_h = ((float)t_fine) - 76800.0f;
    var_h = ((float)raw_hum - (((float)s_calib.dig_H4) * 64.0f +
             ((float)s_calib.dig_H5) / 16384.0f * var_h)) *
            (((float)s_calib.dig_H2) / 65536.0f *
             (1.0f + ((float)s_calib.dig_H6) / 67108864.0f * var_h *
              (1.0f + ((float)s_calib.dig_H3) / 67108864.0f * var_h)));
    var_h = var_h * (1.0f - ((float)s_calib.dig_H1) * var_h / 524288.0f);

    if (var_h > 100.0f) { var_h = 100.0f; }
    if (var_h < 0.0f)   { var_h = 0.0f;   }
    /* 引用ここまで */
    *humidity = var_h;

    return true;
}
