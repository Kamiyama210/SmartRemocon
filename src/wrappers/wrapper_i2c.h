#ifndef WRAPPER_I2C_H
#define WRAPPER_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** I2Cを初期化する。引数が不正ならfalse。 */
bool wrapper_i2c_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate);

/** 書き込む。転送長に応じたタイムアウト付き。失敗したらfalse。タイムアウトしたらバスの回復を試みる。 */
bool wrapper_i2c_write(uint8_t addr, const uint8_t *src, size_t len, bool nostop);

/** 読み込む。転送長に応じたタイムアウト付き。失敗したらfalse。タイムアウトしたらバスの回復を試みる。 */
bool wrapper_i2c_read(uint8_t addr, uint8_t *dst, size_t len, bool nostop);

#endif /* WRAPPER_I2C_H */
