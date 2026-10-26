#include "wrapper_wdt.h"

#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include "pico/platform.h"

#define WDT_HW_MAX_MS  (16777u)   ///< RP2350で設定できるタイムアウトの上限。24bitのカウンタを1µsごとに減らす

bool wrapper_wdt_init(uint32_t timeout_ms) {
    if (timeout_ms == 0u) {
        return false;
    }
    if (timeout_ms > WDT_HW_MAX_MS) {
        return false;
    }
    watchdog_enable(timeout_ms, true);
    return true;
}

void wrapper_wdt_feed(void) {
    watchdog_update();
}

bool wrapper_wdt_caused_reboot(void) {
    return watchdog_caused_reboot();
}

void wrapper_wdt_reboot(void) {
    __dsb();
    (void)save_and_disable_interrupts();
    watchdog_reboot(0, 0, 0);
    while (true) {
        tight_loop_contents();
    }
}
