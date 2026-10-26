#ifndef JSON_H
#define JSON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef void* json_handle_t;   ///< 解析したJSONのハンドル

/** JSONの解析を使えるようにする。解析を使う前に1回呼ぶ。 */
void json_init(void);

/** JSONの文字列を解析する。失敗したらNULL。使い終わったらjson_deleteで解放する。 */
json_handle_t json_parse(const char* str);

/** 解析したJSONを解放する。 */
void json_delete(json_handle_t json);

/** 文字列の項目を読む。項目が無い、文字列でない、または終端を含めてmax_lenに収まらなければfalse。 */
bool json_get_string(json_handle_t json, const char* key, char* out_val, size_t max_len);

/** 数値の項目を整数として読む。項目が無い、数値でない、整数でない、またはintに収まらなければfalse。 */
bool json_get_int(json_handle_t json, const char* key, int* out_val);

/** 真偽値の項目を読む。項目が無い、または真偽値でなければfalse。 */
bool json_get_bool(json_handle_t json, const char* key, bool* out_val);

/** 項目があるかを返す。型は問わない。 */
bool json_has_key(json_handle_t json, const char* key);

#endif /* JSON_H */