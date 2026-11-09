#include "phase2_timing.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){char b[400];unsigned i;
assert(pt_command("timing_start",b,sizeof b,0,100));assert(strstr(b,"REJECT"));assert(!pt_active());
pt_command("timing_start",b,sizeof b,1,100);assert(pt_active());
pt_record(1,100,101,102,105,106,3,0);pt_command("timing_row 0",b,sizeof b,1,107);assert(strstr(b,"REJECT"));
pt_command("timing_clear",b,sizeof b,1,107);assert(strstr(b,"REJECT"));
for(i=1;i<256;i++)pt_record(i+1,100+i*20,101+i*20,102+i*20,105+i*20,106+i*20,3,0);
assert(!pt_active());pt_record(999,0,0,0,0,0,0,0);pt_command("timing_info",b,sizeof b,1,9999);assert(strstr(b,"count=256 active=0 reason=1"));puts(b);
pt_command("timing_row 255",b,sizeof b,1,9999);assert(strstr(b,"seq=256"));pt_command("timing_row 255x",b,sizeof b,1,9999);assert(strstr(b,"REJECT"));
pt_command("timing_clear",b,sizeof b,1,9999);pt_command("timing_start",b,sizeof b,1,0xfffffff0U);pt_poll(0x00007520U,1);assert(!pt_active());pt_command("timing_info",b,sizeof b,1,0);assert(strstr(b,"reason=2"));
pt_command("timing_clear",b,sizeof b,1,0);pt_command("timing_start",b,sizeof b,1,0);pt_poll(1,0);assert(!pt_active());pt_command("timing_info",b,sizeof b,1,0);assert(strstr(b,"reason=3"));puts("PASS: capacity, immutable freeze, strict row parsing, clear gate, OBSERVE gate, mode abort, timeout wrap.");return 0;}
