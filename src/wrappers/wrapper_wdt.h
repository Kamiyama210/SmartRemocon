#ifndef WRAPPER_WDT_H
#define WRAPPER_WDT_H

#include <stdint.h>
#include <stdbool.h>

/** ハードウェアWDTを起動する。0またはチップの上限を超える値ならfalse。 */
bool wrapper_wdt_init(uint32_t timeout_ms);

/** ハードウェアWDTのタイマーをリセットする。 */
void wrapper_wdt_feed(void);

/** 前回のリセットがWDTによるものか。 */
bool wrapper_wdt_caused_reboot(void);

/** 割り込みを止め、書き込みを確定させてから再起動する。戻らない。 */
void wrapper_wdt_reboot(void);

#endif /* WRAPPER_WDT_H */
