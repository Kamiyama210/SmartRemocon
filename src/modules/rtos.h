#ifndef RTOS_H
#define RTOS_H

#include <stdint.h>
#include <stdbool.h>

typedef void *rtos_task_handle_t;   ///< タスクのハンドル
typedef void *queue_t;              ///< キューのハンドル
typedef void *rtos_mutex_t;         ///< ミューテックスのハンドル

/** タスクの優先度 */
typedef enum {
    RTOS_PRIORITY_NORMAL = 0,   ///< 優先度：中
    RTOS_PRIORITY_HIGH,         ///< 優先度：高
    RTOS_PRIORITY_MAX           ///< 優先度：最高
} rtos_priority_t;

/** タスクを生成する。out_handleはNULLでもよい。 */
bool rtos_task_create(void (*f)(void *), const char *name,
                      uint32_t stack_depth, void *param,
                      rtos_priority_t priority,
                      rtos_task_handle_t *out_handle);

/** 呼び出したタスク自身を削除する。 */
void rtos_task_delete_current(void);

/** 呼び出したタスクのハンドルを返す。 */
rtos_task_handle_t rtos_task_get_current_handle(void);

/** スケジューラを開始する。 */
void rtos_start_scheduler(void);

/** 指定したミリ秒だけ待つ。 */
void rtos_delay_ms(uint32_t ms);

/** 起動からのtick数を返す。 */
uint32_t rtos_task_get_tick_count(void);

/** ミリ秒をtickに変換する。 */
uint32_t rtos_ms_to_ticks(uint32_t ms);

/** 前回の起床時刻（tick）から、increment_msミリ秒の間隔で起床する。 */
void rtos_task_delay_until(uint32_t *prev_wake, uint32_t increment_ms);

/** タスクへ通知を送る（ビットを立てる）。割り込みの処理中からも呼べる。 */
void rtos_task_notify(rtos_task_handle_t task, uint32_t value);

/** 通知を待つ。届いたらtrue。 */
bool rtos_task_notify_wait(uint32_t clear_entry, uint32_t clear_exit,
                           uint32_t *value, uint32_t timeout_ms);

/** キューを生成する。 */
queue_t rtos_queue_create(uint32_t length, uint32_t item_size);

/** キューから1件取り出す。 */
bool rtos_queue_receive(queue_t q, void *buf, uint32_t timeout_ms);

/** キューの先頭を、取り出さずに読む。 */
bool rtos_queue_peek(queue_t q, void *buf, uint32_t timeout_ms);

/** キューへ1件入れる。 */
bool rtos_queue_send(queue_t q, const void *item, uint32_t timeout_ms);

/** 長さ1のキューの中身を上書きする。 */
bool rtos_queue_overwrite(queue_t q, const void *item);

/** ミューテックスを生成する。 */
rtos_mutex_t rtos_mutex_create(void);

/** ミューテックスを取得する。取得できたらtrue。 */
bool rtos_mutex_take(rtos_mutex_t m, uint32_t timeout_ms);

/** ミューテックスを解放する。 */
void rtos_mutex_give(rtos_mutex_t m);

#endif /* RTOS_H */
