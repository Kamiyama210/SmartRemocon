#ifndef WRAPPER_JSON_H
#define WRAPPER_JSON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** cJSONのメモリ確保をFreeRTOSのヒープに向ける。解析を使う前に1回呼ぶ。 */
void wrapper_json_init(void);

/** cJSONで文字列を解析する。失敗したらNULL。 */
void* wrapper_json_parse(const char* str);

/** 解析したJSONを解放する。 */
void wrapper_json_delete(void* json);

/** 文字列の項目を読む。項目が無い、文字列でない、または終端を含めてmax_lenに収まらなければfalse。 */
bool wrapper_json_get_string(void* json, const char* key, char* out_val, size_t max_len);

/** 数値の項目を整数として読む。項目が無い、数値でない、整数でない、またはintに収まらなければfalse。 */
bool wrapper_json_get_int(void* json, const char* key, int* out_val);

/** 真偽値の項目を読む。項目が無い、または真偽値でなければfalse。 */
bool wrapper_json_get_bool(void* json, const char* key, bool* out_val);

/** 項目があるかを返す。 */
bool wrapper_json_has_key(void* json, const char* key);

#endif /* WRAPPER_JSON_H */