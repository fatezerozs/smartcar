#include "pid.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
int main(void){My_Pid_Init();assert(speed_KP==20&&speed_KD==0);assert(fabsf(speed_KI-.9765625f)<.000001f);
for(int i=0;i<10000;i++){float x=PidLocCtrl(&speed_pid_l,193.698f,1);assert(isfinite(x)&&x>=-5000&&x<=5000);assert(fabsf(speed_pid_l.integrator)<=5000);}
float prior=speed_pid_l.out;float x=PidLocCtrl(&speed_pid_l,-40,1);assert(x<prior);My_Pid_Init();assert(speed_pid_l.integrator==0&&speed_pid_l.out==0);
printf("PID PASS: 20000 saturation/integrator checks plus Phase2 output units, release and reset; synthetic only\n");return 0;}
