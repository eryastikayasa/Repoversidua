#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_control_init(void);
void uart_control_send(const char *cmd);
int  uart_control_read(char *buf, size_t max_len); // return jumlah byte terbaca, -1 jika tidak ada data
bool uart_control_execute_command(const char *cmd);

#ifdef __cplusplus
}
#endif
