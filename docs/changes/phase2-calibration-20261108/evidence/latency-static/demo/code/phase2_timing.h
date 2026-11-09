#ifndef PHASE2_TIMING_H
#define PHASE2_TIMING_H
#include <stdint.h>
int pt_command(const char *,char *,unsigned,int,uint32_t);
int pt_active(void);
void pt_poll(uint32_t,int);
void pt_record(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
#endif
