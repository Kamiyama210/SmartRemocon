#ifndef WRAPPER_SYS_H
#define WRAPPER_SYS_H

#include <stdint.h>

/** 標準入出力を初期化する。 */
void wrapper_stdio_init(void);

/** 指定したミリ秒だけビジーウェイトする。他のタスクにCPUを譲らない。 */
void wrapper_busy_wait_ms(uint32_t ms);

/** 起動からの経過時間（ミリ秒）を返す。 */
uint32_t wrapper_get_time_ms(void);

#endif /* WRAPPER_SYS_H */
