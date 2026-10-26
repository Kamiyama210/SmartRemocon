#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Pico SDKとの連携 */
#define configSUPPORT_PICO_SYNC_INTEROP                 1   ///< Pico SDKのロック機構とFreeRTOSを安全に連携させる

/* マルチコア（RP2350の2コア） */
#define configNUMBER_OF_CORES                           2   ///< 管理するコアの数
#define configTICK_CORE                                 0   ///< OSのtickを担当するコア
#define configRUN_MULTIPLE_PRIORITIES                   1   ///< 優先度が異なるタスクでも、2つのコアで同時に実行できる
#define configUSE_CORE_AFFINITY                         1   ///< タスクごとに実行するコアを指定できる
#define configUSE_PASSIVE_IDLE_HOOK                     0   ///< 2つ目のコアのアイドル時に、関数を呼び出さない

/* 基本のシステム設定 */
#define configUSE_PREEMPTION                            1   ///< 優先度の高いタスクが動けるようになったら、すぐに切り替える
#define configUSE_TIME_SLICING                          1   ///< 同じ優先度のタスクが複数あれば、tickごとに順番に実行する
#define configUSE_IDLE_HOOK                             0   ///< アイドル時に関数を呼び出さない
#define configUSE_TICK_HOOK                             0   ///< tickごとに関数を呼び出さない
#define configCPU_CLOCK_HZ                              150000000   ///< CPUのクロック（Hz）。RP2350の標準の150MHz
#define configTICK_RATE_HZ                              ((TickType_t)1000)   ///< tickの周波数（Hz）。1msごと
#define configMAX_PRIORITIES                            5   ///< 優先度の段階数（0〜4）
#define configMINIMAL_STACK_SIZE                        256   ///< アイドルタスクのスタックサイズ（ワード）
#define configMAX_TASK_NAME_LEN                         16   ///< タスク名の最大長（終端を含む）
#define configUSE_16_BIT_TICKS                          0   ///< tickを32ビットで数える
#define configIDLE_SHOULD_YIELD                         1   ///< アイドルタスクと同じ優先度のタスクがあれば、アイドルタスクが譲る
#define configUSE_NEWLIB_REENTRANT                      1   ///< printfなどのC標準ライブラリを、複数のタスクから安全に使えるようにする

/* メモリの確保（heap_4を使用） */
#define configSUPPORT_DYNAMIC_ALLOCATION                1   ///< タスクやキューをヒープから確保する
#define configSUPPORT_STATIC_ALLOCATION                 0   ///< 静的な確保は使わない
#define configTOTAL_HEAP_SIZE                           (128 * 1024)   ///< FreeRTOSのヒープの大きさ

/* ミューテックスとセマフォ */
#define configUSE_MUTEXES                               1   ///< ミューテックスを使う
#define configUSE_RECURSIVE_MUTEXES                     1   ///< 再帰的なミューテックスを使う
#define configUSE_COUNTING_SEMAPHORES                   1   ///< カウンティングセマフォを使う

/* FreeRTOSの割り込み処理を、Pico SDKの割り込みの名前に結び付ける */
#define vPortSVCHandler                                 isr_svcall
#define xPortPendSVHandler                              isr_pendsv
#define xPortSysTickHandler                             isr_systick

/* Cortex-M33（RP2350）の設定 */
#define configENABLE_FPU                                1   ///< 浮動小数点演算器を使う。タスクの切り替えでFPUのレジスタも保存する
#define configENABLE_MPU                                0   ///< メモリ保護ユニットは使わない
#define configENABLE_TRUSTZONE                          0   ///< TrustZoneは使わない
#define configRUN_FREERTOS_SECURE_ONLY                  1   ///< セキュア側だけで動かす

/* 割り込みの優先度（Cortex-M33） */
#define configPRIO_BITS                                 4   ///< 割り込みの優先度のビット数
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15   ///< 最も低い割り込みの優先度
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5   ///< これより優先度が低い（数値が大きい）割り込みからだけ、FreeRTOSのAPIを呼べる
#define configKERNEL_INTERRUPT_PRIORITY                 (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))   ///< 上の値を、レジスタに書く形（上位ビット）に直したもの
#define configMAX_SYSCALL_INTERRUPT_PRIORITY            (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))   ///< 上の値を、レジスタに書く形（上位ビット）に直したもの

/* ソフトウェアタイマー */
#define configUSE_TIMERS                                1   ///< ソフトウェアタイマーを使う
#define configTIMER_TASK_PRIORITY                       (configMAX_PRIORITIES - 1)   ///< タイマーを処理するタスクの優先度（最高）
#define configTIMER_QUEUE_LENGTH                        10   ///< タイマーへの依頼をためておける数
#define configTIMER_TASK_STACK_DEPTH                    256   ///< タイマーを処理するタスクのスタックサイズ（ワード）

/* 使うAPIの有効化 */
#define INCLUDE_vTaskDelay                              1   ///< 指定時間だけ待つAPIを有効にする
#define INCLUDE_vTaskSuspend                            1   ///< タスクを止めるAPIを有効にする
#define INCLUDE_xTimerPendFunctionCall                  1   ///< 処理をタイマーのタスクに頼むAPIを有効にする
#define INCLUDE_xSemaphoreGetMutexHolder                1   ///< ミューテックスを持っているタスクを調べるAPIを有効にする
#define INCLUDE_vTaskDelayUntil                         1   ///< 一定周期で起床するAPIを有効にする
#define INCLUDE_vTaskDelete                             1   ///< タスクを削除するAPIを有効にする
#define INCLUDE_xTaskGetSchedulerState                  1   ///< スケジューラが動いているかを調べるAPIを有効にする

/* そのほかの機能 */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS         5   ///< タスクごとのデータを持つ枠の数
#define configUSE_TASK_NOTIFICATIONS                    1   ///< タスク通知を使う

/* FreeRTOSの内部チェック（assert）を、C標準のassertにする */
#ifndef configASSERT
#include <assert.h>
#define configASSERT( x )                               assert( x )
#endif

#endif /* FREERTOS_CONFIG_H */
