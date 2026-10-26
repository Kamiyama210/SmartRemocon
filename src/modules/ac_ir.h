#ifndef AC_IR_H
#define AC_IR_H

#include <stdint.h>
#include <stdbool.h>

/** 運転モード。値はIR信号のビット値。 */
typedef enum {
    AC_MODE_COOL = 0x03,   ///< 冷房
    AC_MODE_HEAT = 0x04    ///< 暖房
} ac_mode_t;

/** 風量。値はIR信号のビット値。 */
typedef enum {
    AC_FAN_QUIET = 0x03,   ///< 静音（1段階目）
    AC_FAN_2     = 0x04,   ///< 2段階目
    AC_FAN_3     = 0x05,   ///< 3段階目
    AC_FAN_4     = 0x07,   ///< 4段階目
    AC_FAN_AUTO  = 0x0A    ///< 自動
} ac_fan_t;

/** 風向き。値はIR信号のビット値。 */
typedef enum {
    AC_DIR_1    = 0x01,   ///< 1段階目
    AC_DIR_2    = 0x02,   ///< 2段階目
    AC_DIR_3    = 0x03,   ///< 3段階目
    AC_DIR_4    = 0x04,   ///< 4段階目
    AC_DIR_5    = 0x05,   ///< 5段階目
    AC_DIR_AUTO = 0x0F    ///< 自動
} ac_dir_t;

#define AC_TEMP_MIN_C   (16u)   ///< 送れる設定温度の下限。16℃を基準に4bitで符号化する。範囲外は丸めて送る
#define AC_TEMP_MAX_C   (30u)   ///< 送れる設定温度の上限。範囲外は丸めて送る

/** IRで送るエアコンの設定。 */
typedef struct {
    bool      power_on;      ///< 運転するか
    ac_mode_t mode;          ///< 運転モード
    uint8_t   temperature;   ///< 設定温度（AC_TEMP_MIN_C〜AC_TEMP_MAX_C）
    ac_fan_t  fan;           ///< 風量
    ac_dir_t  dir;           ///< 風向き
} ac_state_t;

/** IR送信を初期化する。失敗したらfalse。 */
bool ac_ir_init(void);

/** エアコンの設定からIR信号を作り、送信を開始する。完了は待たない。前回の送信中、または設定が不正ならfalse。 */
bool ac_ir_send(const ac_state_t *state);

/** 送信中か。 */
bool ac_ir_is_busy(void);

#endif /* AC_IR_H */
