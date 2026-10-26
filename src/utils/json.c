#include "json.h"
#include "wrapper_json.h"

void json_init(void) {
    wrapper_json_init();
}

json_handle_t json_parse(const char* str) {
    return wrapper_json_parse(str);
}

void json_delete(json_handle_t json) {
    wrapper_json_delete(json);
}

bool json_get_string(json_handle_t json, const char* key, char* out_val, size_t max_len) {
    return wrapper_json_get_string(json, key, out_val, max_len);
}

bool json_get_int(json_handle_t json, const char* key, int* out_val) {
    return wrapper_json_get_int(json, key, out_val);
}

bool json_get_bool(json_handle_t json, const char* key, bool* out_val) {
    return wrapper_json_get_bool(json, key, out_val);
}

bool json_has_key(json_handle_t json, const char* key) {
    return wrapper_json_has_key(json, key);
}
