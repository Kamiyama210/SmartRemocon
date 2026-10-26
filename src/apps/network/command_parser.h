#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#define CMD_PAYLOAD_MAX_LEN   (127u)   ///< ac/state/setのペイロードの最大長（終端を除く）。今のところ最大:約80文字

/** ac/state/setのペイロードを解析し、controlタスクへ渡す。解析か受け渡しに失敗したらfalse。 */
bool command_handle_payload(const uint8_t *payload, uint16_t len);

#endif /* COMMAND_PARSER_H */
