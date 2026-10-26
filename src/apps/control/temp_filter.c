#include "temp_filter.h"
#include "logger.h"
#include "sys.h"

#include <math.h>
#include <string.h>

#define TEMP_VALID_MAX_C        (45.0f)   ///< 受け付ける温度の上限
#define TEMP_VALID_MIN_C        (5.0f)    ///< 受け付ける温度の下限
#define TEMP_MAX_DELTA_C        (2.0f)    ///< 前回からの変化がこれを超えたら急変とみなす
#define TEMP_LEVEL_TOLERANCE_C  (1.0f)    ///< 保留中の値と同じ水準とみなす差
#define TEMP_FAULT_STREAK       (12u)     ///< 捨てた値がこの回数（5秒周期で60秒）続いたらセンサー異常とする。
#define STABILITY_RANGE_C       (0.3f)    ///< 安定とみなす変動の幅の上限
#define STABILITY_SLOPE_C_MAX   (0.15f)   ///< 安定とみなす傾きの上限
#define TRIM_MIN_COUNT          (3u)      ///< 最大と最小を除いて平均するのに必要な数
#define TRIM_EXCLUDE_COUNT      (2u)      ///< 平均から除く数（最大と最小）
#define TEMP_WINDOW_HALF        (TEMP_WINDOW_SIZE / 2u)   ///< 傾きを求めるときの前半・後半の数

void temp_filter_init(temp_filter_t *f) {
    if (f == NULL) {
        return;
    }
    (void)memset(f, 0, sizeof(*f));
}

/** 測定値をリングバッファに追加する。 */
static void push_sample(temp_filter_t *f, float value) {
    f->samples[f->index] = value;
    f->index = (uint8_t)((f->index + 1u) % TEMP_WINDOW_SIZE);
    if (f->count < TEMP_WINDOW_SIZE) {
        f->count++;
    }
}

/** 最後に追加した測定値を返す。 */
static float last_sample(const temp_filter_t *f) {
    uint8_t last_idx = (f->index == 0u) ? (TEMP_WINDOW_SIZE - 1u) : (uint8_t)(f->index - 1u);
    return f->samples[last_idx];
}


/** リングバッファを空にし、valueの1件から始め直す。測定値がそろうまで、安定の判定はしない。 */
static void restart_window_with(temp_filter_t *f, float value) {
    f->index = 0u;
    f->count = 0u;
    push_sample(f, value);
}

temp_result_t temp_filter_push(temp_filter_t *f, float new_temp) {
    if (f == NULL) {
        return TEMP_RESULT_FAULT;
    }

    if ((new_temp > TEMP_VALID_MAX_C) || (new_temp < TEMP_VALID_MIN_C) ||
        isnan(new_temp)) {
        LOG_WARN("Temp out of range rejected: %.2f", (double)new_temp);
        f->has_pending = false;
        f->reject_streak++;
        return (f->reject_streak >= TEMP_FAULT_STREAK) ? TEMP_RESULT_FAULT : TEMP_RESULT_REJECTED_RANGE;
    }

    if (f->count == 0u) {
        push_sample(f, new_temp);
        f->reject_streak = 0u;
        f->has_pending   = false;
        return TEMP_RESULT_ACCEPTED;
    }

    if (fabsf(new_temp - last_sample(f)) > TEMP_MAX_DELTA_C) {

        if (f->has_pending && (fabsf(new_temp - f->pending_value) <= TEMP_LEVEL_TOLERANCE_C)) {
            f->pending_streak++;

            if (f->pending_streak >= TEMP_SPIKE_CONFIRM_NEED) {
                LOG_INFO("Temperature level shift confirmed: %.2f -> %.2f",
                         (double)last_sample(f), (double)new_temp);
                restart_window_with(f, new_temp);
                f->has_pending   = false;
                f->reject_streak = 0u;
                return TEMP_RESULT_LEVEL_SHIFTED;
            }
        } else {
            f->pending_value  = new_temp;
            f->pending_streak = 1u;
            f->has_pending    = true;
        }

        LOG_WARN("Temp spike held: %.2f (pending %u/%u)", (double)new_temp, (unsigned)f->pending_streak, (unsigned)TEMP_SPIKE_CONFIRM_NEED);

        f->reject_streak++;
        return (f->reject_streak >= TEMP_FAULT_STREAK) ? TEMP_RESULT_FAULT : TEMP_RESULT_REJECTED_SPIKE;
    }

    push_sample(f, new_temp);
    f->reject_streak = 0u;
    f->has_pending   = false;
    return TEMP_RESULT_ACCEPTED;
}

bool temp_filter_get_average(const temp_filter_t *f, float *out_avg) {
    if ((f == NULL) || (out_avg == NULL) || (f->count == 0u)) {
        return false;
    }

    if (f->count < TRIM_MIN_COUNT) {
        float sum = 0.0f;
        for (uint8_t i = 0u; i < f->count; i++) {
            sum += f->samples[i];
        }
        *out_avg = sum / (float)f->count;
        return true;
    }

    float sum = 0.0f;
    float max_v = f->samples[0];
    float min_v = f->samples[0];
    for (uint8_t i = 0u; i < f->count; i++) {
        float v = f->samples[i];
        sum += v;
        if (v > max_v) { max_v = v; }
        if (v < min_v) { min_v = v; }
    }

    *out_avg = (sum - max_v - min_v) / (float)(f->count - TRIM_EXCLUDE_COUNT);
    return true;
}

void temp_filter_get_stability(const temp_filter_t *f, temp_stability_t *out) {
    if ((f == NULL) || (out == NULL)) {
        return;
    }
    (void)memset(out, 0, sizeof(*out));

    if (f->count < TEMP_WINDOW_SIZE) {
        out->is_valid = false;
        return;
    }
    out->is_valid = true;

    float max_v = f->samples[0];
    float min_v = f->samples[0];
    for (uint8_t i = 1u; i < TEMP_WINDOW_SIZE; i++) {
        if (f->samples[i] > max_v) { max_v = f->samples[i]; }
        if (f->samples[i] < min_v) { min_v = f->samples[i]; }
    }
    out->range_c = max_v - min_v;

    const uint8_t half = TEMP_WINDOW_HALF;
    float first_sum = 0.0f;
    float second_sum = 0.0f;

    for (uint8_t i = 0u; i < half; i++) {
        first_sum  += f->samples[(f->index + i) % TEMP_WINDOW_SIZE];
        second_sum += f->samples[(f->index + half + i) % TEMP_WINDOW_SIZE];
    }

    float delta = (second_sum - first_sum) / (float)half;
    float span_min = ((float)half * (float)TEMP_SAMPLE_PERIOD_MS) / (float)MS_PER_MIN;
    out->slope_c_per_min = delta / span_min;

    out->is_stable = (out->range_c <= STABILITY_RANGE_C) &&
                     (fabsf(out->slope_c_per_min) <= STABILITY_SLOPE_C_MAX);
}
