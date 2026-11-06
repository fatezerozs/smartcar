#ifndef B3_TRACE_H
#define B3_TRACE_H
#include <stdint.h>
#include "b3_wire.h"
void b3_init(void);
uint32_t b3_filter(uint8_t *data,uint32_t length);
void b3_poll(void);
void b3_encoder_read(uint16_t left,uint16_t right);
void b3_pit_sample(void);
void b3_pi_begin(uint32_t now);
void b3_pi_end(unsigned serviced,uint32_t masked_cycles,const void *locked_snapshot,uint32_t consumed_sequence);
void b3_mode(int a,int b,unsigned raw,unsigned selected,unsigned settled,uint32_t revision);
void b3_vision(uint32_t sequence,uint32_t capture_ms);
void b3_pwm_write(uint32_t pin,uint32_t requested,uint32_t applied,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t mctrl);
#endif
