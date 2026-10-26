#ifndef WDT_H
#define WDT_H

#include <stdint.h>
#include <stdbool.h>

/** ハードウェアWDTを起動する。0またはチップの上限を超える値ならfalse。 */
bool wdt_init(uint32_t timeout_ms);

/** ハードウェアWDTのタイマーをリセットする。 */
void wdt_feed(void);

/** 前回のリセットがWDTによるものか。 */
bool wdt_caused_reboot(void);

/** 割り込みを止め、書き込みを確定させてから、WDTの機能でシステムを再起動する。戻らない。 */
void wdt_reboot(void);

#endif /* WDT_H */
