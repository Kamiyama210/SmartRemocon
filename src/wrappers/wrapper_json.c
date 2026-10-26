#include "wrapper_json.h"
#include "cJSON.h"
#include "FreeRTOS.h"
#include <limits.h>
#include <math.h>
#include <string.h>

/** cJSONのメモリ確保を、FreeRTOSのヒープ（複数タスクから使っても安全）で行うようにする。 */
void wrapper_json_init(void) {
    cJSON_Hooks hooks = {
        .malloc_fn = pvPortMalloc,
        .free_fn   = vPortFree,
    };
    cJSON_InitHooks(&hooks);
}

void* wrapper_json_parse(const char* str) {
    return (void*)cJSON_Parse(str);
}

void wrapper_json_delete(void* json) {
    if (json != NULL) {
        cJSON_Delete((cJSON*)json);
    }
}

bool wrapper_json_get_string(void* json, const char* key, char* out_val, size_t max_len) {
    if (json == NULL || key == NULL || out_val == NULL || max_len == 0u) return false;

    cJSON* node = cJSON_GetObjectItemCaseSensitive((cJSON*)json, key);
    if (!cJSON_IsString(node) || (node->valuestring == NULL)) {
        return false;
    }
    size_t len = strlen(node->valuestring);
    if (len >= max_len) {
        return false;
    }
    (void)memcpy(out_val, node->valuestring, len + 1u);
    return true;
}

bool wrapper_json_get_int(void* json, const char* key, int* out_val) {
    if (json == NULL || key == NULL || out_val == NULL) return false;

    cJSON* node = cJSON_GetObjectItemCaseSensitive((cJSON*)json, key);
    if (!cJSON_IsNumber(node)) {
        return false;
    }
    double v = node->valuedouble;
    if ((v != floor(v)) || (v < (double)INT_MIN) || (v > (double)INT_MAX)) {
        return false;
    }
    *out_val = (int)v;
    return true;
}

bool wrapper_json_get_bool(void* json, const char* key, bool* out_val) {
    if (json == NULL || key == NULL || out_val == NULL) return false;

    cJSON* node = cJSON_GetObjectItemCaseSensitive((cJSON*)json, key);
    if (cJSON_IsBool(node)) {
        *out_val = cJSON_IsTrue(node);
        return true;
    }
    return false;
}

bool wrapper_json_has_key(void* json, const char* key) {
    if (json == NULL || key == NULL) return false;
    return cJSON_HasObjectItem((cJSON*)json, key);
}
