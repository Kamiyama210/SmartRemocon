#ifndef WDT_MONITOR_H
#define WDT_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#define WDT_INVALID_ID  (0xFFFFFFFFu)   ///< 登録に失敗したことを表すID

/** 監視対象のタスクの状態。 */
typedef enum {
    WDT_TASK_RUNNING = 0,   ///< 処理中
    WDT_TASK_WAITING        ///< 待ち中
} wdt_task_state_t;

/** 監視の準備をし、監視タスクを生成する。ハードウェアWDTはまだ起動しない。失敗したらfalse。 */
bool wdt_monitor_init(void);

/** 登録済みのタスク数がexpected_task_countと一致すれば、ハードウェアWDTを起動する。一致しなければfalse。 */
bool wdt_monitor_start(uint32_t expected_task_count);

/** 呼び出したタスクを監視対象に登録し、IDを返す。失敗したらWDT_INVALID_ID。nameは静的な文字列を渡す。 */
uint32_t wdt_register_task(const char *name,
                           uint32_t running_timeout_ms,
                           uint32_t waiting_timeout_ms);

/** 呼び出したタスクの状態を更新し、監視の時間をリセットする。登録したタスク以外からの呼び出しは無視する。 */
void wdt_task_set_state(uint32_t wdt_id, wdt_task_state_t state);

/** 異常を検知したときに呼ばれる。弱シンボルで、既定はシステムのリセット。 */
void wdt_on_fault(void);

#endif /* WDT_MONITOR_H */
