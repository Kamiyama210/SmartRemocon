#include "wrapper_sys.h"

#include "pico/stdlib.h"
#include "pico/time.h"

void wrapper_stdio_init(void) {
    stdio_init_all();
}

void wrapper_busy_wait_ms(uint32_t ms) {
    busy_wait_ms(ms);
}

uint32_t wrapper_get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}
