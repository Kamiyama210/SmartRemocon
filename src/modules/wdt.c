#include "wdt.h"
#include "wrapper_wdt.h"

bool wdt_init(uint32_t timeout_ms) {
    return wrapper_wdt_init(timeout_ms);
}

void wdt_feed(void) {
    wrapper_wdt_feed();
}

bool wdt_caused_reboot(void) {
    return wrapper_wdt_caused_reboot();
}

void wdt_reboot(void) {
    wrapper_wdt_reboot();
}
