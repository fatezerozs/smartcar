#ifndef DIAG_H
#define DIAG_H
#include <stdint.h>
#include "ff.h"
uint32_t diag_ms(void);
void diag_emit(const char *s);
void diag_status(void);
void diag_command(const char *s);
uint32_t diag_crc(const uint8_t *p,unsigned n);
uint32_t diag_us(void);
int diag_set_clock(uint32_t hz);
int diag_bench(uint32_t hz);
void diag_bench_result(void);
int diag_export_command(const char *s);
void diag_send_bytes(const uint8_t *p,unsigned n);
int diag_uart_baud(uint32_t baud);
#endif
