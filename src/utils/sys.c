#include "sys.h"
#include "wrapper_sys.h"

void sys_stdio_init(void) {
    wrapper_stdio_init();
}

void sys_busy_wait_ms(uint32_t ms) {
    wrapper_busy_wait_ms(ms);
}

uint32_t sys_get_time_ms(void) {
    return wrapper_get_time_ms();
}

bool sys_deadline_expired(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

uint32_t sys_deadline_remaining(uint32_t now_ms, uint32_t deadline_ms) {
    int32_t diff = (int32_t)(deadline_ms - now_ms);
    return (diff <= 0) ? 0u : (uint32_t)diff;
}

uint32_t sys_deadline_from_now(uint32_t duration_ms) {
    return wrapper_get_time_ms() + duration_ms;
}

bool sys_elapsed_since(uint32_t now_ms, uint32_t past_ms, uint32_t threshold_ms) {
    return (uint32_t)(now_ms - past_ms) >= threshold_ms;
}

uint32_t sys_backoff_delay_ms(uint32_t base_ms, uint32_t max_ms, uint8_t attempt, uint8_t shift_limit) {
    uint32_t shift;
    uint32_t delay;

    if (base_ms == 0u) {
        return max_ms;
    }

    shift = (attempt > shift_limit) ? shift_limit : attempt;
    delay = base_ms << shift;

    if ((delay > max_ms) || (delay < base_ms)) {
        delay = max_ms;
    }
    return delay;
}
