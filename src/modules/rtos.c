#include "rtos.h"
#include "wrapper_rtos.h"

/** 優先度を、FreeRTOSの優先度の値にする。 */
static uint32_t to_wrapper_priority(rtos_priority_t priority) {
    switch (priority) {
    case RTOS_PRIORITY_MAX:    return WRAPPER_PRIORITY_MAX;
    case RTOS_PRIORITY_HIGH:   return WRAPPER_PRIORITY_HIGH;
    case RTOS_PRIORITY_NORMAL:
    default:                   return WRAPPER_PRIORITY_NORMAL;
    }
}

bool rtos_task_create(void (*f)(void *), const char *name,
                      uint32_t stack_depth, void *param,
                      rtos_priority_t priority,
                      rtos_task_handle_t *out_handle) {
    return wrapper_rtos_task_create(f, name, stack_depth, param, to_wrapper_priority(priority), out_handle);
}

void rtos_task_delete_current(void) {
    wrapper_rtos_task_delete_self();
}

rtos_task_handle_t rtos_task_get_current_handle(void) {
    return wrapper_rtos_task_get_current_handle();
}

void rtos_start_scheduler(void) {
    wrapper_rtos_start_scheduler();
}

void rtos_delay_ms(uint32_t ms) {
    wrapper_rtos_delay_ms(ms);
}

uint32_t rtos_task_get_tick_count(void) {
    return wrapper_rtos_task_get_tick_count();
}

uint32_t rtos_ms_to_ticks(uint32_t ms) {
    return wrapper_rtos_ms_to_ticks(ms);
}

void rtos_task_delay_until(uint32_t *prev_wake, uint32_t increment_ms) {
    wrapper_rtos_task_delay_until(prev_wake, increment_ms);
}

void rtos_task_notify(rtos_task_handle_t task, uint32_t value) {
    wrapper_rtos_task_notify(task, value);
}

bool rtos_task_notify_wait(uint32_t clear_entry, uint32_t clear_exit,
                           uint32_t *value, uint32_t timeout_ms) {
    return wrapper_rtos_task_notify_wait(clear_entry, clear_exit, value, timeout_ms);
}

queue_t rtos_queue_create(uint32_t length, uint32_t item_size) {
    return wrapper_rtos_queue_create(length, item_size);
}

bool rtos_queue_receive(queue_t q, void *buf, uint32_t timeout_ms) {
    return wrapper_rtos_queue_receive(q, buf, timeout_ms);
}

bool rtos_queue_peek(queue_t q, void *buf, uint32_t timeout_ms) {
    return wrapper_rtos_queue_peek(q, buf, timeout_ms);
}

bool rtos_queue_send(queue_t q, const void *item, uint32_t timeout_ms) {
    return wrapper_rtos_queue_send(q, item, timeout_ms);
}

bool rtos_queue_overwrite(queue_t q, const void *item) {
    return wrapper_rtos_queue_overwrite(q, item);
}

rtos_mutex_t rtos_mutex_create(void) {
    return wrapper_rtos_mutex_create();
}

bool rtos_mutex_take(rtos_mutex_t m, uint32_t timeout_ms) {
    return wrapper_rtos_mutex_take(m, timeout_ms);
}

void rtos_mutex_give(rtos_mutex_t m) {
    wrapper_rtos_mutex_give(m);
}
