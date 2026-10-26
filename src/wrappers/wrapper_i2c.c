#include "wrapper_i2c.h"
#include "wrapper_common.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define I2C_PORT              i2c_default   ///< 使用するI2Cポート

#define I2C_TIMEOUT_PER_BYTE_US   (2000u)   ///< 1バイトあたりのタイムアウト（µs）。100kHzで1バイト約90µs
#define I2C_TIMEOUT_BASE_US       (5000u)   ///< 転送ごとに足すタイムアウト（µs）

#define I2C_RECOVERY_PULSES       (9u)      ///< バスの回復で送るSCLのパルス数。I2C仕様のバスクリア
#define I2C_RECOVERY_HALF_US      (5u)      ///< 回復のパルスの半周期（µs）。100kHz相当

#define I2C_PIN_NONE              (0xFFu)   ///< ピンがまだ決まっていないことを表す値

static uint8_t         s_sda_pin = I2C_PIN_NONE;   ///< SDAのピン番号
static uint8_t         s_scl_pin = I2C_PIN_NONE;   ///< SCLのピン番号
static bool            s_initialized = false; ///< 初期化が済んでいるか

/** 転送長に応じたタイムアウト（マイクロ秒）を返す。 */
static uint32_t timeout_for(size_t len) {
    return I2C_TIMEOUT_BASE_US + (uint32_t)len * I2C_TIMEOUT_PER_BYTE_US;
}

/** SCLを9回パルスさせてからSTOPを作り、SDAをLowのまま離さないスレーブを解放する。 */
static void i2c_bus_recover(void) {
    if ((s_sda_pin > MAX_GPIO_PIN) || (s_scl_pin > MAX_GPIO_PIN)) {
        return;
    }

    gpio_set_function((uint)s_scl_pin, GPIO_FUNC_SIO);
    gpio_set_function((uint)s_sda_pin, GPIO_FUNC_SIO);
    gpio_set_dir((uint)s_scl_pin, GPIO_OUT);
    gpio_set_dir((uint)s_sda_pin, GPIO_IN);
    gpio_pull_up((uint)s_sda_pin);

    for (uint8_t i = 0u; i < I2C_RECOVERY_PULSES; i++) {
        gpio_put((uint)s_scl_pin, 0);
        busy_wait_us(I2C_RECOVERY_HALF_US);
        gpio_put((uint)s_scl_pin, 1);
        busy_wait_us(I2C_RECOVERY_HALF_US);
    }

    gpio_set_dir((uint)s_sda_pin, GPIO_OUT);
    gpio_put((uint)s_sda_pin, 0);
    busy_wait_us(I2C_RECOVERY_HALF_US);
    gpio_put((uint)s_scl_pin, 1);
    busy_wait_us(I2C_RECOVERY_HALF_US);
    gpio_put((uint)s_sda_pin, 1);
    busy_wait_us(I2C_RECOVERY_HALF_US);

    gpio_set_function((uint)s_sda_pin, GPIO_FUNC_I2C);
    gpio_set_function((uint)s_scl_pin, GPIO_FUNC_I2C);
}

bool wrapper_i2c_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate) {
    if ((sda_pin > MAX_GPIO_PIN) || (scl_pin > MAX_GPIO_PIN) || (baudrate == 0u)) {
        return false;
    }

    s_sda_pin = sda_pin;
    s_scl_pin = scl_pin;

    i2c_init(I2C_PORT, baudrate);
    gpio_set_function((uint)sda_pin, GPIO_FUNC_I2C);
    gpio_set_function((uint)scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up((uint)sda_pin);
    gpio_pull_up((uint)scl_pin);

    s_initialized = true;
    return true;
}

bool wrapper_i2c_write(uint8_t addr, const uint8_t *src, size_t len, bool nostop) {
    int ret;

    if (!s_initialized || (src == NULL) || (len == 0u)) {
        return false;
    }

    ret = i2c_write_timeout_us(I2C_PORT, addr, src, len, nostop, timeout_for(len));

    if (ret == PICO_ERROR_TIMEOUT) {
        i2c_bus_recover();
    }

    return (ret >= 0) && ((size_t)ret == len);
}

bool wrapper_i2c_read(uint8_t addr, uint8_t *dst, size_t len, bool nostop) {
    int ret;

    if (!s_initialized || (dst == NULL) || (len == 0u)) {
        return false;
    }

    ret = i2c_read_timeout_us(I2C_PORT, addr, dst, len, nostop, timeout_for(len));

    if (ret == PICO_ERROR_TIMEOUT) {
        i2c_bus_recover();
    }

    return (ret >= 0) && ((size_t)ret == len);
}

