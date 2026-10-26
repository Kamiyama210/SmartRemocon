#include "wrapper_rtos.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

_Static_assert(sizeof(TickType_t) == sizeof(uint32_t),
               "configUSE_16_BIT_TICKS must be 0 for this wrapper");
#define MIN_TICK_RATE_HZ   (100u)   ///< tickの周波数の下限。待ち時間の分解能を10ms以下にする

_Static_assert(configTICK_RATE_HZ >= MIN_TICK_RATE_HZ,
               "Tick rate too low for millisecond-resolution timeouts");
_Static_assert(configUSE_TASK_NOTIFICATIONS == 1,
               "Task notifications must be enabled");

/** ミリ秒の待ち時間をtickに変換する。 */
static inline TickType_t to_ticks(uint32_t timeout_ms) {
    return pdMS_TO_TICKS(timeout_ms);
}

bool wrapper_rtos_task_create(void (*task_func)(void *), const char *name,
                              uint32_t stack_depth, void *param,
                              uint32_t priority,
                              wrapper_task_handle_t *out_handle) {
    TaskHandle_t handle = NULL;
    BaseType_t ret = xTaskCreate(task_func, name, (configSTACK_DEPTH_TYPE)stack_depth, param, (UBaseType_t)priority, &handle);
    if (ret != pdPASS) {
        return false;
    }
    if (out_handle != NULL) {
        *out_handle = (wrapper_task_handle_t)handle;
    }
    return true;
}

void wrapper_rtos_task_delete_self(void) {
    vTaskDelete(NULL);
}

wrapper_task_handle_t wrapper_rtos_task_get_current_handle(void) {
    return (wrapper_task_handle_t)xTaskGetCurrentTaskHandle();
}

const char *wrapper_rtos_task_get_name(wrapper_task_handle_t task) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return NULL;
    }
    return pcTaskGetName((TaskHandle_t)task);
}

void wrapper_rtos_start_scheduler(void) {
    vTaskStartScheduler();
}

void wrapper_rtos_delay_ms(uint32_t ms) {
    vTaskDelay(to_ticks(ms));
}

uint32_t wrapper_rtos_task_get_tick_count(void) {
    if (wrapper_rtos_is_in_isr()) {
        return (uint32_t)xTaskGetTickCountFromISR();
    }
    return (uint32_t)xTaskGetTickCount();
}

uint32_t wrapper_rtos_ms_to_ticks(uint32_t ms) {
    return (uint32_t)to_ticks(ms);
}

void wrapper_rtos_task_delay_until(uint32_t *prev_wake_time, uint32_t increment_ms) {
    if (prev_wake_time != NULL) {
        vTaskDelayUntil((TickType_t *)prev_wake_time, to_ticks(increment_ms));
    }
}

bool wrapper_rtos_is_in_isr(void) {
    return (xPortIsInsideInterrupt() == pdTRUE);
}

/** タスクへ通知を送る。割り込みの処理中なら割り込み用のAPIを使う。 */
void wrapper_rtos_task_notify(wrapper_task_handle_t task, uint32_t value) {
    if (task == NULL) {
        return;
    }

    if (wrapper_rtos_is_in_isr()) {
        BaseType_t higher_priority_woken = pdFALSE;
        xTaskNotifyFromISR((TaskHandle_t)task, value, eSetBits,
                           &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
    } else {
        xTaskNotify((TaskHandle_t)task, value, eSetBits);
    }
}

bool wrapper_rtos_task_notify_wait(uint32_t clear_on_entry, uint32_t clear_on_exit,
                                   uint32_t *notification_value, uint32_t timeout_ms) {
    BaseType_t ret = xTaskNotifyWait(clear_on_entry, clear_on_exit,
                                     notification_value, to_ticks(timeout_ms));
    return (ret == pdTRUE);
}

wrapper_queue_t wrapper_rtos_queue_create(uint32_t length, uint32_t item_size) {
    return (wrapper_queue_t)xQueueCreate(length, item_size);
}

bool wrapper_rtos_queue_receive(wrapper_queue_t queue, void *buffer, uint32_t timeout_ms) {
    if ((queue == NULL) || (buffer == NULL)) {
        return false;
    }
    return (xQueueReceive((QueueHandle_t)queue, buffer, to_ticks(timeout_ms)) == pdPASS);
}

bool wrapper_rtos_queue_peek(wrapper_queue_t queue, void *buffer, uint32_t timeout_ms) {
    if ((queue == NULL) || (buffer == NULL)) {
        return false;
    }
    return (xQueuePeek((QueueHandle_t)queue, buffer, to_ticks(timeout_ms)) == pdPASS);
}

bool wrapper_rtos_queue_send(wrapper_queue_t queue, const void *item, uint32_t timeout_ms) {
    if ((queue == NULL) || (item == NULL)) {
        return false;
    }
    if (wrapper_rtos_is_in_isr()) {
        return false;
    }
    return (xQueueSend((QueueHandle_t)queue, item, to_ticks(timeout_ms)) == pdPASS);
}

bool wrapper_rtos_queue_overwrite(wrapper_queue_t queue, const void *item) {
    if ((queue == NULL) || (item == NULL)) {
        return false;
    }
    if (wrapper_rtos_is_in_isr()) {
        return false;
    }
    return (xQueueOverwrite((QueueHandle_t)queue, item) == pdPASS);
}

wrapper_mutex_t wrapper_rtos_mutex_create(void) {
    return (wrapper_mutex_t)xSemaphoreCreateMutex();
}

bool wrapper_rtos_mutex_take(wrapper_mutex_t mutex, uint32_t timeout_ms) {
    if (mutex == NULL) {
        return false;
    }
    if (wrapper_rtos_is_in_isr()) {
        return false;
    }
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, to_ticks(timeout_ms)) == pdTRUE);
}

void wrapper_rtos_mutex_give(wrapper_mutex_t mutex) {
    if (mutex != NULL) {
        (void)xSemaphoreGive((SemaphoreHandle_t)mutex);
    }
}
