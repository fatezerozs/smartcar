#include "wheel_speed.h"
void ws_pi_reset(ws_pi_t *p){p->integral_q8=0;p->output=0;}
uint16_t ws_pi_step(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp,uint16_t ki,uint16_t cap,unsigned periods)
{
    int64_t error,proportional,candidate,limit=(int64_t)cap*256,output;
    if(target<=0 || !cap || !periods || periods>5){ws_pi_reset(p);return 0;}
    error=(int64_t)target-measured;proportional=error*kp;
    candidate=p->integral_q8+error*ki*periods;
    if(candidate>limit)candidate=limit;
    if(candidate<0)candidate=0;
    output=proportional+candidate;
    /* Integrate only if not pushing further into saturation. */
    if(!((output>limit && error>0)||(output<0 && error<0)))p->integral_q8=(int32_t)candidate;
    output=proportional+p->integral_q8;
    if(output<0)output=0;
    if(output>limit)output=limit;
    p->output=(uint16_t)(output/256);return p->output;
}
