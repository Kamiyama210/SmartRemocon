#include "command_parser.h"
#include "payload.h"
#include "json.h"
#include "logger.h"
#include "control.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define CMD_WIRE_MAX_LEN          (16u)    ///< 通信用の文字列の最大長。今のところ最も長い値:7文字

/** 読み取り結果 */
typedef enum {
    OPT_ABSENT = 0,   ///< 項目が無い
    OPT_OK,           ///< 正しい値があった
    OPT_INVALID       ///< 項目はあるが、型か値が不正
} optional_result_t;

/** 任意の文字列の項目を読む。 */
static optional_result_t get_optional_wire(json_handle_t json, const char *key,
                                           char *out, size_t out_size) {
    if (!json_has_key(json, key)) {
        return OPT_ABSENT;
    }
    return json_get_string(json, key, out, out_size) ? OPT_OK : OPT_INVALID;
}

/** powerがtrueの操作の、残りの項目（モード・温度・風量・風向き）を読む。1項目でも不正ならfalse。 */
static bool parse_apply(json_handle_t json, remote_command_t *out_cmd) {
    char wire[CMD_WIRE_MAX_LEN];
    int target = 0;
    optional_result_t opt;

    if (!json_get_string(json, "mode", wire, sizeof(wire)) ||
        !json_get_int(json, "target", &target)) {
        LOG_WARN("Command missing or invalid mode/target");
        return false;
    }
    if (!payload_mode_from_wire(wire, &out_cmd->mode)) {
        LOG_WARN("Unknown mode: %s", wire);
        return false;
    }
    if ((target < (int)AC_TEMP_MIN_C) || (target > (int)AC_TEMP_MAX_C)) {
        LOG_WARN("Target %d out of range [%d,%d]", target, (int)AC_TEMP_MIN_C, (int)AC_TEMP_MAX_C);
        return false;
    }
    out_cmd->target_temperature = (uint8_t)target;

    opt = get_optional_wire(json, "fan", wire, sizeof(wire));
    if (opt == OPT_OK) {
        if (!payload_fan_from_wire(wire, &out_cmd->fan)) {
            LOG_WARN("Unknown fan: %s", wire);
            return false;
        }
        out_cmd->has_fan = true;
    } else if (opt == OPT_INVALID) {
        LOG_WARN("Invalid fan value");
        return false;
    }

    opt = get_optional_wire(json, "airflow", wire, sizeof(wire));
    if (opt == OPT_OK) {
        if (!payload_airflow_from_wire(wire, &out_cmd->airflow)) {
            LOG_WARN("Unknown airflow: %s", wire);
            return false;
        }
        out_cmd->has_airflow = true;
    } else if (opt == OPT_INVALID) {
        LOG_WARN("Invalid airflow value");
        return false;
    }

    out_cmd->type = CMD_APPLY;
    return true;
}

/** ac/state/setのペイロードを解析する。1項目でも不正ならfalseを返し、out_cmdは変えない。 */
static bool parse_command(const uint8_t *payload, uint16_t len, remote_command_t *out_cmd) {
    char buf[CMD_PAYLOAD_MAX_LEN + 1u];
    json_handle_t json;
    remote_command_t cmd;
    bool ok;
    bool power = false;

    if (payload == NULL) {
        return false;
    }

    if (len > CMD_PAYLOAD_MAX_LEN) {
        LOG_WARN("Command payload too long (%u > %u), rejected", (unsigned)len, (unsigned)CMD_PAYLOAD_MAX_LEN);
        return false;
    }

    (void)memcpy(buf, payload, len);
    buf[len] = '\0';

    json = json_parse(buf);
    if (json == NULL) {
        LOG_WARN("Failed to parse JSON command");
        return false;
    }

    (void)memset(&cmd, 0, sizeof(cmd));

    if (!json_get_bool(json, "power", &power)) {
        LOG_WARN("Command has no boolean 'power' field");
        ok = false;
    } else if (!power) {
        cmd.type = CMD_STOP_AC;
        ok = true;
    } else {
        ok = parse_apply(json, &cmd);
    }

    json_delete(json);

    if (ok) {
        *out_cmd = cmd;
    }
    return ok;
}

bool command_handle_payload(const uint8_t *payload, uint16_t len) {
    remote_command_t cmd;

    if (!parse_command(payload, len, &cmd)) {
        return false;
    }
    if (!control_post_command(&cmd)) {
        LOG_ERROR("Command post failed, type=%d", (int)cmd.type);
        return false;
    }
    return true;
}
