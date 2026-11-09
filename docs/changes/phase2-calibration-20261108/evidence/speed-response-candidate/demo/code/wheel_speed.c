#include "wheel_speed.h"
void ws_pi_reset(ws_pi_t *p){p->integral_q8=0;p->output=0;}
uint16_t ws_pi_step_ff_ms(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp,uint16_t ki,uint16_t cap,unsigned elapsed_ms,uint16_t feedforward)
{
    int64_t error,proportional,candidate,limit=(int64_t)cap*256,output,minimum,maximum,bias;
    if(target<=0 || !cap || !elapsed_ms || elapsed_ms>50U){ws_pi_reset(p);return 0;}
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
    candidate=p->integral_q8+error*ki*elapsed_ms/10U;
    if(candidate>maximum)candidate=maximum;
    if(candidate<minimum)candidate=minimum;
    output=bias+proportional+candidate;
    /* Accept the part of an increment that reaches the saturation boundary.
     * Rejecting the entire crossing step leaves a permanent sub-cap gap.
     * Once saturated, do not integrate further into the limit. */
    if(output>limit && error>0) {
        if(bias+proportional+p->integral_q8<limit)
            p->integral_q8=(int32_t)(limit-bias-proportional);
    } else if(output<0 && error<0) {
        if(bias+proportional+p->integral_q8>0)
            p->integral_q8=(int32_t)(-bias-proportional);
    } else p->integral_q8=(int32_t)candidate;
    output=bias+proportional+p->integral_q8;
    if(output<0)output=0;
    if(output>limit)output=limit;
    p->output=(uint16_t)(output/256);return p->output;
}

uint16_t ws_pi_step(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp,uint16_t ki,uint16_t cap,unsigned periods)
{return ws_pi_step_ff(p,target,measured,kp,ki,cap,periods,0);}

/* Compatibility entry for existing nominal-10ms callers. */
uint16_t ws_pi_step_ff(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp,uint16_t ki,uint16_t cap,unsigned periods,uint16_t feedforward)
{
    if(!periods || periods>5){ws_pi_reset(p);return 0;}
    return ws_pi_step_ff_ms(p,target,measured,kp,ki,cap,periods*10U,feedforward);
}
