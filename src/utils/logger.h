#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define LOG_LEVEL_DEBUG 0   ///< ログレベル：デバッグ
#define LOG_LEVEL_INFO  1   ///< ログレベル：情報
#define LOG_LEVEL_WARN  2   ///< ログレベル：警告
#define LOG_LEVEL_ERROR 3   ///< ログレベル：エラー
#define LOG_LEVEL_NONE  4   ///< ログレベル：出力しない

#ifndef SYSTEM_LOG_LEVEL
#define SYSTEM_LOG_LEVEL LOG_LEVEL_INFO   ///< 出力するログの最低レベル
#endif

/** 起動からの経過ミリ秒を返す。 */
uint32_t log_time_ms(void);

/** 実行中のタスク名を返す。NULLでもよい。 */
const char *log_context(void);

/** 割り込みの処理中ならtrueを返す。 */
bool log_is_in_isr(void);

/** 時刻・レベル・タスク名を付けてログを1行出力する。割り込みの処理中は出力しない。 */
#define LOG_EMIT(letter, format, ...)                                        \
    do {                                                                     \
        if (!log_is_in_isr()) {                                              \
            const char *_ctx = log_context();                                \
            printf("[%8lu][" letter "][%s] " format "\n",                    \
                   (unsigned long)log_time_ms(),                             \
                   (_ctx != NULL) ? _ctx : "-",                              \
                   ##__VA_ARGS__);                                           \
        }                                                                    \
    } while (0)

#if SYSTEM_LOG_LEVEL <= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(format, ...) LOG_EMIT("D", format, ##__VA_ARGS__)   ///< デバッグのログを出す
#else
    #define LOG_DEBUG(format, ...) do { } while (0)
#endif

#if SYSTEM_LOG_LEVEL <= LOG_LEVEL_INFO
    #define LOG_INFO(format, ...)  LOG_EMIT("I", format, ##__VA_ARGS__)   ///< 情報のログを出す
#else
    #define LOG_INFO(format, ...)  do { } while (0)
#endif

#if SYSTEM_LOG_LEVEL <= LOG_LEVEL_WARN
    #define LOG_WARN(format, ...)  LOG_EMIT("W", format, ##__VA_ARGS__)   ///< 警告のログを出す
#else
    #define LOG_WARN(format, ...)  do { } while (0)
#endif

#if SYSTEM_LOG_LEVEL <= LOG_LEVEL_ERROR
    #define LOG_ERROR(format, ...) LOG_EMIT("E", format, ##__VA_ARGS__)   ///< エラーのログを出す
#else
    #define LOG_ERROR(format, ...) do { } while (0)
#endif

#endif /* LOGGER_H */
