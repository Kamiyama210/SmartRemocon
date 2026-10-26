#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define GPIO_IR_TX          (15u)       ///< 赤外線LEDを駆動するピン

#define GPIO_I2C_SDA        (4u)        ///< BME280のI2CのSDAピン
#define GPIO_I2C_SCL        (5u)        ///< BME280のI2CのSCLピン
#define I2C_BAUDRATE_HZ     (100000u)   ///< I2Cの通信速度。標準モード

#endif /* BOARD_CONFIG_H */