#include "payload.h"
#include "sys.h"

#include <stdio.h>
#include <string.h>

/** 列挙値と通信用の文字列の組。 */
typedef struct {
    int         value;   ///< 列挙値
    const char *wire;    ///< 通信用の文字列
} wire_entry_t;

/** 運転モードの対応表。 */
static const wire_entry_t MODE_TABLE[] = {
    { AC_MODE_COOL, "cooling" },
    { AC_MODE_HEAT, "heating" },
};

/** 風量の対応表。 */
static const wire_entry_t FAN_TABLE[] = {
    { AC_FAN_AUTO,  "auto" },
    { AC_FAN_QUIET, "1"    },
    { AC_FAN_2,     "2"    },
    { AC_FAN_3,     "3"    },
    { AC_FAN_4,     "4"    },
};

/** 風向きの対応表。 */
static const wire_entry_t AIRFLOW_TABLE[] = {
    { AC_DIR_AUTO, "auto" },
    { AC_DIR_1,    "1"    },
    { AC_DIR_2,    "2"    },
    { AC_DIR_3,    "3"    },
    { AC_DIR_4,    "4"    },
    { AC_DIR_5,    "5"    },
};

/** 対応表から、文字列に対応する列挙値を探す。見つからなければfalse。 */
static bool value_of(const wire_entry_t *table, size_t size, const char *wire, int *out) {
    if (wire == NULL) {
        return false;
    }
    for (size_t i = 0u; i < size; i++) {
        if (strcmp(table[i].wire, wire) == 0) {
            *out = table[i].value;
            return true;
        }
    }
    return false;
}

/** 対応表から、列挙値に対応する文字列を探す。見つからなければNULL。 */
static const char *wire_of(const wire_entry_t *table, size_t size, int value) {
    for (size_t i = 0u; i < size; i++) {
        if (table[i].value == value) {
            return table[i].wire;
        }
    }
    return NULL;
}

bool payload_mode_from_wire(const char *wire, ac_mode_t *out) {
    int v = 0;
    if ((out == NULL) || !value_of(MODE_TABLE, ARRAY_SIZE(MODE_TABLE), wire, &v)) {
        return false;
    }
    *out = (ac_mode_t)v;
    return true;
}

bool payload_fan_from_wire(const char *wire, ac_fan_t *out) {
    int v = 0;
    if ((out == NULL) || !value_of(FAN_TABLE, ARRAY_SIZE(FAN_TABLE), wire, &v)) {
        return false;
    }
    *out = (ac_fan_t)v;
    return true;
}

bool payload_airflow_from_wire(const char *wire, ac_dir_t *out) {
    int v = 0;
    if ((out == NULL) || !value_of(AIRFLOW_TABLE, ARRAY_SIZE(AIRFLOW_TABLE), wire, &v)) {
        return false;
    }
    *out = (ac_dir_t)v;
    return true;
}

size_t payload_format_state(const ac_state_t *state, char *buf, size_t buf_size) {
    const char *mode;
    const char *fan;
    const char *airflow;
    int written;

    if ((state == NULL) || (buf == NULL) || (buf_size == 0u)) {
        return 0u;
    }

    mode    = wire_of(MODE_TABLE, ARRAY_SIZE(MODE_TABLE), (int)state->mode);
    fan     = wire_of(FAN_TABLE, ARRAY_SIZE(FAN_TABLE), (int)state->fan);
    airflow = wire_of(AIRFLOW_TABLE, ARRAY_SIZE(AIRFLOW_TABLE), (int)state->dir);
    if ((mode == NULL) || (fan == NULL) || (airflow == NULL)) {
        return 0u;
    }

    written = snprintf(buf, buf_size,
                       "{\"power\":%s,\"mode\":\"%s\",\"target\":%u,\"fan\":\"%s\",\"airflow\":\"%s\"}",
                       state->power_on ? "true" : "false", mode,
                       (unsigned)state->temperature, fan, airflow);

    if ((written < 0) || ((size_t)written >= buf_size)) {
        return 0u;
    }
    return (size_t)written;
}

size_t payload_format_sensor(float temperature, float humidity, char *buf, size_t buf_size) {
    int written;

    if ((buf == NULL) || (buf_size == 0u)) {
        return 0u;
    }

    written = snprintf(buf, buf_size, "{\"temp\":%.2f,\"hum\":%.2f}", (double)temperature, (double)humidity);

    if ((written < 0) || ((size_t)written >= buf_size)) {
        return 0u;
    }
    return (size_t)written;
}
