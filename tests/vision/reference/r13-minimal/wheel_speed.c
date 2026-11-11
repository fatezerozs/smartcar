#include "wheel_speed.h"
void ws_pi_reset(ws_pi_t *p){p->integral_q8=0;p->output=0;}
uint16_t ws_pi_step_ff(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp,uint16_t ki,uint16_t cap,unsigned periods,uint16_t feedforward)
{
    int64_t error,proportional,candidate,limit=(int64_t)cap*256,output,minimum,maximum,bias;
    if(target<=0 || !cap || !periods || periods>5){ws_pi_reset(p);return 0;}
    if(feedforward>cap)feedforward=cap;
    bias=(int64_t)feedforward*256;
    minimum=-bias;maximum=limit-bias;
    /* Feedback must be able to REMOVE an excessive feedforward as well as
     * add torque under load. Otherwise integral>=0 imposes a persistent speed
     * floor whenever the tentative feedforward exceeds this wheel's needs.
     * With feedforward=0 these are the original 0..cap integral bounds. */
    if(p->integral_q8>maximum)p->integral_q8=(int32_t)maximum;
    if(p->integral_q8<minimum)p->integral_q8=(int32_t)minimum;
    error=(int64_t)target-measured;proportional=error*kp;
    candidate=p->integral_q8+error*ki*periods;
    if(candidate>maximum)candidate=maximum;
    if(candidate<minimum)candidate=minimum;
    output=bias+proportional+candidate;
    /* Integrate only if not pushing further into saturation. */
    if(!((output>limit && error>0)||(output<0 && error<0)))p->integral_q8=(int32_t)candidate;
    output=bias+proportional+p->integral_q8;
    if(output<0)output=0;
    if(output>limit)output=limit;
    p->output=(uint16_t)(output/256);return p->output;
}

uint16_t ws_pi_step(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp,uint16_t ki,uint16_t cap,unsigned periods)
{return ws_pi_step_ff(p,target,measured,kp,ki,cap,periods,0);}
