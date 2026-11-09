#ifndef VISUAL_TRACE_H
#define VISUAL_TRACE_H
#include "vision_midterm.h"
int vq_command(const char *,char *,unsigned,int,uint32_t);
void vq_poll(uint32_t,int);
int vq_quiet(void);
void vq_record(const uint8_t *,const vision_result_t *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
#endif
