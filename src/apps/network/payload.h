#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ac_ir.h"

/** 通信用の文字列を運転モードにする。未知の文字列ならfalseを返し、outは変えない。 */
bool payload_mode_from_wire(const char *wire, ac_mode_t *out);

/** 通信用の文字列を風量にする。未知の文字列ならfalseを返し、outは変えない。 */
bool payload_fan_from_wire(const char *wire, ac_fan_t *out);

/** 通信用の文字列を風向きにする。未知の文字列ならfalseを返し、outは変えない。 */
bool payload_airflow_from_wire(const char *wire, ac_dir_t *out);

/** 最後に送ったIRの内容から、ac/stateのペイロード（JSON）をbufに書き、文字数（終端を除く）を返す。収まらない・値が不正なら0。 */
size_t payload_format_state(const ac_state_t *state, char *buf, size_t buf_size);

/** 温度と湿度から、ac/sensorのペイロード（JSON）をbufに書き、文字数（終端を除く）を返す。収まらなければ0。 */
size_t payload_format_sensor(float temperature, float humidity, char *buf, size_t buf_size);

#endif /* PAYLOAD_H */
