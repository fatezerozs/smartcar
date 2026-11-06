#ifndef DIAG_H
#define DIAG_H
#include <stdint.h>
#include "ff.h"
uint32_t diag_ms(void);
void diag_emit(const char *s);
void diag_status(void);
void diag_command(const char *s);
uint32_t diag_crc(const uint8_t *p,unsigned n);
#endif
