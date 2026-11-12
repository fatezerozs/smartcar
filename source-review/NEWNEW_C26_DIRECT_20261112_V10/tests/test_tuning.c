#include "drive_tuning.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){unsigned checks=0;servo_smoother_t s={1550,1550};float old=1550;
assert(fabsf(TARGET_SPEED_MPS*LEFT_COUNTS_PER_METER*.01f-159.516f)<.001f);
assert(fabsf(TARGET_SPEED_MPS*RIGHT_COUNTS_PER_METER*.01f-159.446f)<.001f);
for(int i=0;i<1000;i++){float x=servo_smooth_step(&s,i<500?1670.0f:1430.0f,1);assert(x>=1430&&x<=1670);assert(fabsf(x-old)<=35.001f);old=x;checks+=2;}
assert(fabsf(s.command_us-1430)<2.1f);
assert(servo_smooth_step(&s,1670,0)==1550);assert(s.filtered_us==1550);
for(int i=0;i<2000;i++){float x=servo_smooth_step(&s,i%2?1552.0f:1548.0f,1);assert(fabsf(x-1550)<2.1f);++checks;}
printf("TUNING PASS: %u bound/rate/jitter checks plus speed conversion, full travel and OFF reset; synthetic only\n",checks);return 0;}
