#include "b3_trace.h"
#include <assert.h>
#include <stdio.h>
b3_probe_dwt_t b3_probe_dwt;
static unsigned begins,ends,modes;
void b3_pi_begin(uint32_t now){(void)now;++begins;}
void b3_pi_end(unsigned serviced,uint32_t cycles,const void *snapshot,uint32_t consumed){(void)serviced;(void)cycles;(void)snapshot;(void)consumed;++ends;}
void b3_mode(int a,int b,unsigned raw,unsigned selected,unsigned settled,uint32_t revision)
{(void)a;(void)b;(void)raw;(void)selected;(void)settled;(void)revision;++modes;}
static void check(void) __attribute__((destructor));
static void check(void){assert(begins&&begins==ends&&modes);printf("PASS private hook coverage: PI begin/end=%u mode=%u\n",begins,modes);}
