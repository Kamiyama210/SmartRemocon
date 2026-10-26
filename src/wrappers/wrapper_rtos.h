#ifndef WRAPPER_RTOS_H
#define WRAPPER_RTOS_H

#include <stdint.h>
#include <stdbool.h>

typedef void *wrapper_task_handle_t;   ///< タスクのハンドル（実体はFreeRTOSのTaskHandle_t）
typedef void *wrapper_queue_t;         ///< キューのハンドル（実体はFreeRTOSのQueueHandle_t）
typedef void *wrapper_mutex_t;         ///< ミューテックスのハンドル（実体はFreeRTOSのSemaphoreHandle_t）

#define WRAPPER_PRIORITY_NORMAL  (2u)   ///< 優先度：中
#define WRAPPER_PRIORITY_HIGH    (3u)   ///< 優先度：高
#define WRAPPER_PRIORITY_MAX     (4u)   ///< 優先度：最高（configMAX_PRIORITIES - 1）

/** 割り込みの処理中か。 */
bool wrapper_rtos_is_in_isr(void);

/** タスクを生成する。out_handleはNULLでもよい。 */
bool wrapper_rtos_task_create(void (*task_func)(void *), const char *name,
                              uint32_t stack_depth, void *param,
                              uint32_t priority,
                              wrapper_task_handle_t *out_handle);

/** 呼び出したタスク自身を削除する。 */
void wrapper_rtos_task_delete_self(void);

/** 呼び出したタスクのハンドルを返す。 */
wrapper_task_handle_t wrapper_rtos_task_get_current_handle(void);

/** タスク名を返す。スケジューラの開始前はNULL。 */
const char *wrapper_rtos_task_get_name(wrapper_task_handle_t task);

/** スケジューラを開始する。 */
void wrapper_rtos_start_scheduler(void);

/** 指定したミリ秒だけ待つ。 */
void wrapper_rtos_delay_ms(uint32_t ms);

/** 起動からのtick数を返す。割り込みの処理中からも呼べる。 */
uint32_t wrapper_rtos_task_get_tick_count(void);

/** ミリ秒をtickに変換する。 */
uint32_t wrapper_rtos_ms_to_ticks(uint32_t ms);

/** 前回の起床時刻（tick）から、increment_msミリ秒の間隔で起床する。 */
void wrapper_rtos_task_delay_until(uint32_t *prev_wake_time, uint32_t increment_ms);

/** タスクへ通知を送る（ビットを立てる）。割り込みの処理中からも呼べる。 */
void wrapper_rtos_task_notify(wrapper_task_handle_t task, uint32_t value);

/** 通知を待つ。届いたらtrue。 */
bool wrapper_rtos_task_notify_wait(uint32_t clear_on_entry, uint32_t clear_on_exit,
                                   uint32_t *notification_value, uint32_t timeout_ms);

/** キューを生成する。 */
wrapper_queue_t wrapper_rtos_queue_create(uint32_t length, uint32_t item_size);

/** キューから1件取り出す。 */
bool wrapper_rtos_queue_receive(wrapper_queue_t queue, void *buffer, uint32_t timeout_ms);

/** キューの先頭を、取り出さずに読む。 */
bool wrapper_rtos_queue_peek(wrapper_queue_t queue, void *buffer, uint32_t timeout_ms);

/** キューへ1件入れる。割り込みの処理中からは呼べない。 */
bool wrapper_rtos_queue_send(wrapper_queue_t queue, const void *item, uint32_t timeout_ms);

/** 長さ1のキューの中身を上書きする。 */
bool wrapper_rtos_queue_overwrite(wrapper_queue_t queue, const void *item);

/** 優先度継承付きのミューテックスを生成する。 */
wrapper_mutex_t wrapper_rtos_mutex_create(void);

/** ミューテックスを取得する。取得できたらtrue。割り込みの処理中からは取得できない。 */
bool wrapper_rtos_mutex_take(wrapper_mutex_t mutex, uint32_t timeout_ms);

/** ミューテックスを解放する。 */
void wrapper_rtos_mutex_give(wrapper_mutex_t mutex);

#endif /* WRAPPER_RTOS_H */
