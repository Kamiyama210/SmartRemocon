#ifndef TEMP_FILTER_H
#define TEMP_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#define TEMP_SAMPLE_PERIOD_MS   (5000u)   ///< 測定値を追加する周期（ミリ秒）
#define TEMP_WINDOW_SIZE        (12u)     ///< 平均を取る測定値の数
#define TEMP_SPIKE_CONFIRM_NEED (3u)      ///< 急変した値をこの回数（5秒周期）続けて受けたら、新しい水準として受け入れる。

/** 測定値を追加した結果。 */
typedef enum {
    TEMP_RESULT_ACCEPTED = 0,      ///< OK
    TEMP_RESULT_REJECTED_RANGE,    ///< 値が範囲外なので捨てた
    TEMP_RESULT_REJECTED_SPIKE,    ///< 急変したので保留した
    TEMP_RESULT_LEVEL_SHIFTED,     ///< 急変が続いたので、新しい水準として受け入れた
    TEMP_RESULT_FAULT              ///< 捨てた値が続きすぎた。センサー異常
} temp_result_t;

/** 室温が安定したかの判定結果。 */
typedef struct {
    bool  is_stable;         ///< 変動の幅と傾きがどちらも基準以内か
    bool  is_valid;          ///< 測定値がそろっていて判定できたか
    float range_c;           ///< 測定値の最大と最小の差
    float slope_c_per_min;   ///< 前半の平均から後半の平均への変化の速さ
} temp_stability_t;

/** 温度のフィルタの状態。 */
typedef struct {
    float   samples[TEMP_WINDOW_SIZE];   ///< 直近の測定値（リングバッファ）
    uint8_t index;                       ///< 次に書き込む位置
    uint8_t count;                       ///< 入っている測定値の数
    uint8_t reject_streak;               ///< 連続で捨てた回数
    float   pending_value;               ///< 急変として保留している値
    uint8_t pending_streak;              ///< 保留している値に近い値が続いた回数
    bool    has_pending;                 ///< 保留している値があるか
} temp_filter_t;

/** フィルタを空の状態にする。 */
void temp_filter_init(temp_filter_t *f);

/** 測定値を追加する。範囲外や急変の値は取り込まずに結果で知らせる。 */
temp_result_t temp_filter_push(temp_filter_t *f, float new_temp);

/** 平均値（3件以上なら最大と最小を除いた平均）を返す。測定値が無ければfalse。 */
bool temp_filter_get_average(const temp_filter_t *f, float *out_avg);

/** 室温が安定したかを、変動の幅と傾きから判定する。 */
void temp_filter_get_stability(const temp_filter_t *f, temp_stability_t *out);

#endif /* TEMP_FILTER_H */
