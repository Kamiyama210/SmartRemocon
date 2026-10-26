#ifndef BME280_H
#define BME280_H

#include <stdint.h>
#include <stdbool.h>

/** I2Cとセンサーを初期化する。何度呼んでもよい。失敗したらfalse。 */
bool bme280_init(void);

/** 温度（℃）と湿度（%RH）を読み取る。通信に失敗した、または測定値が無効ならfalse。 */
bool bme280_read(float *temperature, float *humidity);

#endif /* BME280_H */
