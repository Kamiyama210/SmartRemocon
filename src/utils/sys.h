#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include <stdbool.h>

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))   ///< 配列の要素数

#define BITS_PER_BYTE   (8u)         ///< 1バイトのビット数

#define MS_PER_MIN      (60000u)     ///< 1分のミリ秒

/** 標準入出力を初期化する。 */
void sys_stdio_init(void);

/** 指定したミリ秒だけビジーウェイトする。他のタスクにCPUを譲らない。 */
void sys_busy_wait_ms(uint32_t ms);

/** 起動からの経過時間（ミリ秒）を返す。 */
uint32_t sys_get_time_ms(void);

/** 期限を過ぎたかを返す。時刻が一周しても正しく判定する。 */
bool sys_deadline_expired(uint32_t now_ms, uint32_t deadline_ms);

/** 期限までの残り時間（ミリ秒）を返す。過ぎていれば0。 */
uint32_t sys_deadline_remaining(uint32_t now_ms, uint32_t deadline_ms);

/** 現在からduration_ms後の期限を返す。 */
uint32_t sys_deadline_from_now(uint32_t duration_ms);

/** past_msからthreshold_ms以上たったかを返す。 */
bool sys_elapsed_since(uint32_t now_ms, uint32_t past_ms, uint32_t threshold_ms);

/** 再試行の待ち時間を返す。base_msをattempt回だけ2倍にし（最大shift_limit回）、max_msを超えない。 */
uint32_t sys_backoff_delay_ms(uint32_t base_ms, uint32_t max_ms, uint8_t attempt, uint8_t shift_limit);

#endif /* SYS_H */
