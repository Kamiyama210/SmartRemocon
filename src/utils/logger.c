#include "logger.h"
#include "wrapper_sys.h"
#include "wrapper_rtos.h"

/** 起動からの経過ミリ秒を返す（64bitのタイマーから求める）。 */
uint32_t log_time_ms(void) {
    return wrapper_get_time_ms();
}

/** 実行中のタスク名を返す。 */
const char *log_context(void) {
    return wrapper_rtos_task_get_name(wrapper_rtos_task_get_current_handle());
}

/** 割り込みの処理中かを返す。 */
bool log_is_in_isr(void) {
    return wrapper_rtos_is_in_isr();
}
