#include "stop_trace.h"
#include "vision_drive_internal.h"
#include "vision_speed.h"
#include "wheel_speed.h"
#include "board_encoder.h"
#include <limits.h>
#if VISION_ROUTE_SWITCH
#include "vision_power.h"
#include "vision_wheel_targets.h"
#endif
static volatile vd_speed_info_t speed;
const char *vision_drive_speed_fault_name(unsigned detail)
{
    static const char *const names[]={"none","sample_gap","control_gap","counter_delta",
        "output_config","sample_stale","sample_backlog","target_range",
        "reverse_right","reverse_left","no_count_right","no_count_left","response_overspeed"};
    return detail<sizeof(names)/sizeof(names[0])?names[detail]:"unknown";
}
#if VISION_ENCODER_ENABLE
/* Foreground control service runs PI under an IRQ mask. The 10ms ISR samples
 * modular counters and may cut output; it never starts motors or parses UART. */
static ws_pi_t pi[2];
static uint16_t previous[2];
static uint32_t consumed,last_control;
static uint32_t sample_interval;
static volatile uint8_t active,reset_pending;
static unsigned backwards[2],stalled[2],response_backwards[2],response_stalled[2];
static uint16_t request_right,request_left,request_cap;
static int request_reverse;
#if VISION_ROUTE_SWITCH
static unsigned slow_backwards[2];
#endif
static int stopped(void)
{return !vd.motion.phase&&!vd.probe_active&&vd.control.output.state==VC_DISARMED&&!vd.guard_faults;}
static int32_t total_add(int32_t total,int32_t d)
{
    int64_t v=(int64_t)total+d;
    return v>INT32_MAX?INT32_MAX:v<INT32_MIN?INT32_MIN:(int32_t)v;
}
static void trip(unsigned detail,uint32_t now)
{
    if(!speed.faults) {
        speed.fault_detail=(uint8_t)detail;speed.fault_control_age_ms=now-last_control;
        speed.fault_sample_age_ms=now-speed.sample_ms;speed.fault_interval_ms=sample_interval;
        speed.fault_raw3=speed.raw3;speed.fault_raw4=speed.raw4;
        speed.fault_target_right=speed.target_right;speed.fault_target_left=speed.target_left;
        speed.fault_pwm_right=speed.pwm_right;speed.fault_pwm_left=speed.pwm_left;
    }
    speed.faults|=VC_FAULT_ENCODER;vd.guard_faults|=VC_FAULT_ENCODER;
    vd_output_zero(); /* ISR-safe: no PI arithmetic, only integer reset flags. */
}
#endif
void vd_speed_init(uint32_t now)
{
    memset((void *)&speed,0,sizeof(speed));speed.sample_ms=now;
#if VISION_ENCODER_ENABLE
    board_encoder_read(&previous[0],&previous[1]);
    consumed=last_control=sample_interval=0;active=0;reset_pending=1;
    memset(response_backwards,0,sizeof(response_backwards));memset(response_stalled,0,sizeof(response_stalled));
#endif
}
void vd_speed_stop(void)
{
#if VISION_ENCODER_ENABLE
    active=0;reset_pending=1;request_right=request_left=request_cap=0;
    ws_pi_reset(&pi[0]);ws_pi_reset(&pi[1]); /* Integer-only, including from PIT1 STOP. */
    speed.target_right=speed.target_left=0;speed.pwm_right=speed.pwm_left=0;
    st_reset_control(timer_get(GPT_TIM_1));
#endif
}
void vd_speed_sample_tick(uint32_t now)
{
#if VISION_ENCODER_ENABLE
    uint16_t current[2];uint32_t elapsed=now-speed.sample_ms;
    if(elapsed<VD_SPEED_SAMPLE_MIN_INTERVAL_MS)return;
    board_encoder_read(&current[0],&current[1]);
    {int32_t d3=(uint16_t)(current[0]-previous[0]);int32_t d4=(uint16_t)(current[1]-previous[1]);
     if(d3>=32768)d3-=65536;if(d4>=32768)d4-=65536;
     speed.raw3=(int16_t)((int64_t)d3*10/elapsed);speed.raw4=(int16_t)((int64_t)d4*10/elapsed);
     speed.total3=total_add(speed.total3,d3);speed.total4=total_add(speed.total4,d4);}
    previous[0]=current[0];previous[1]=current[1];speed.sample_ms=now;sample_interval=elapsed;++speed.samples;
    st_context(elapsed,speed.faults,speed.fault_detail,speed.enabled);
    st_sample(now,speed.samples,speed.raw3,speed.raw4,speed.total3,speed.total4,vd.motion.phase,vd.motion.reason);
    if(vd.motion.phase==VD_MOTION_FORWARD) {
        if(elapsed>VD_SPEED_SAMPLE_MAX_INTERVAL_MS){trip(VS_FAULT_SAMPLE_GAP,now);return;}
        if((int32_t)speed.raw3*speed.sign3>120 || (int32_t)speed.raw4*speed.sign4>120){trip(12U,now);return;}
        {int32_t m[2]={(int32_t)speed.raw3*speed.sign3,(int32_t)speed.raw4*speed.sign4};
         for(unsigned j=0;j<2;++j){
          response_backwards[j]=m[j]<-4?response_backwards[j]+1U:0U;
          response_stalled[j]=(!speed.enabled&&vd.motion.actual_permille>=180U&&m[j]==0)?response_stalled[j]+1U:0U;
          if(response_backwards[j]>=3U){trip(j?VS_FAULT_REVERSE_RIGHT:VS_FAULT_REVERSE_LEFT,now);return;}
          if(response_stalled[j]>=50U){trip(j?VS_FAULT_NOCOUNT_RIGHT:VS_FAULT_NOCOUNT_LEFT,now);return;}
         }}
        if(st_distance_exhausted(speed.total3,speed.total4)){vd_motion_cancel(12U,now);return;}
    } else {
        memset(response_backwards,0,sizeof(response_backwards));memset(response_stalled,0,sizeof(response_stalled));
    }
    if(speed.enabled&&active) {
        if(elapsed>VD_SPEED_SAMPLE_MAX_INTERVAL_MS)trip(VS_FAULT_SAMPLE_GAP,now);
        else if((uint32_t)(now-last_control)>=VD_SPEED_CONTROL_STALL_TIMEOUT_MS)trip(VS_FAULT_CONTROL_GAP,now);
        else if(speed.raw3<=-30000||speed.raw3>=30000||speed.raw4<=-30000||speed.raw4>=30000)trip(VS_FAULT_DELTA,now);
    }
#else
    (void)now;
#endif
}
void vision_drive_speed_get(vd_speed_info_t *out)
{uint32_t m=__get_PRIMASK();__disable_irq();*out=speed;__set_PRIMASK(m);}
int vision_drive_encoder_signs(int s3,int s4)
{
#if VISION_ENCODER_ENABLE
    if(!stopped()||(s3!=1&&s3!=-1)||(s4!=1&&s4!=-1))return 0;
    speed.sign3=(int8_t)s3;speed.sign4=(int8_t)s4;speed.signs_confirmed=1;vd_speed_stop();return 1;
#else
    (void)s3;(void)s4;return 0;
#endif
}
int vision_drive_speed_config(unsigned max_ticks,float kp,float ki)
{
#if VISION_ENCODER_ENABLE
    uint16_t kp_q8,ki_q8;
    if(!stopped()||!max_ticks||max_ticks>60||!(kp>0&&kp<=5)||!(ki>=0&&ki<=0.2f))return 0;
    kp_q8=(uint16_t)(kp*256.0f);ki_q8=(uint16_t)(ki*256.0f);
    if(!kp_q8)return 0;
    speed.max_ticks=(uint16_t)max_ticks;speed.kp_q8=kp_q8;speed.ki_q8=ki_q8;vd_speed_stop();return 1;
#else
    (void)max_ticks;(void)kp;(void)ki;return 0;
#endif
}
int vision_drive_speed_enable(int enable)
{
#if VISION_ENCODER_ENABLE
    if(!stopped()||(enable!=0&&enable!=1))return 0;
    if(enable&&(!speed.signs_confirmed||!speed.max_ticks||!speed.kp_q8||
        !vd.profile.hardware_confirmed||!vd.profile.motor_direction_confirmed||
        vd.control.config.duty_cap<=0||vd.control.config.duty_cap>0.60001f||
        vd.profile.motor1_scale!=1.0f||vd.profile.motor2_scale!=1.0f||
        !speed.samples||(uint32_t)(timer_get(GPT_TIM_1)-speed.sample_ms)>VD_SPEED_SAMPLE_MAX_AGE_MS))return 0;
    speed.enabled=(uint8_t)enable;speed.faults=0;speed.fault_detail=0;
    speed.fault_control_age_ms=speed.fault_sample_age_ms=speed.fault_interval_ms=0;
    vd_speed_stop();return 1;
#else
    return enable==0;
#endif
}
#if VISION_ENCODER_ENABLE
typedef struct {
    vd_speed_info_t speed;
    ws_pi_t pi[2];
    uint32_t consumed,last_control,sample_interval;
    uint8_t active,reset_pending;
    unsigned backwards[2],stalled[2];
#if VISION_ROUTE_SWITCH
    unsigned slow_backwards[2];
#endif
} speed_work_t;
static void work_trip(speed_work_t *w,unsigned detail,uint32_t now)
{
    if(!w->speed.faults) {
        w->speed.fault_detail=(uint8_t)detail;
        w->speed.fault_control_age_ms=now-w->last_control;
        w->speed.fault_sample_age_ms=now-w->speed.sample_ms;
        w->speed.fault_interval_ms=w->sample_interval;
        w->speed.fault_raw3=w->speed.raw3;w->speed.fault_raw4=w->speed.raw4;
        w->speed.fault_target_right=w->speed.target_right;w->speed.fault_target_left=w->speed.target_left;
        w->speed.fault_pwm_right=w->speed.pwm_right;w->speed.fault_pwm_left=w->speed.pwm_left;
    }
    w->speed.faults|=VC_FAULT_ENCODER;
}
static int speed_control_output(speed_work_t *w,uint16_t right,uint16_t left,int reverse,uint16_t cap,uint16_t *out_right,uint16_t *out_left)
{
    *out_right=right;*out_left=left;
#if VISION_ENCODER_ENABLE
    if(w->speed.enabled) {
        uint32_t now=timer_get(GPT_TIM_1),seq=w->speed.samples,periods=seq-w->consumed;
        uint16_t pwm_cap=cap<vd.motion.requested_permille?cap:vd.motion.requested_permille;
#if VISION_ROUTE_SWITCH
        if(vd.control.output.state==VC_RUNNING&&!vd.motion.phase&&!vd.probe_active)
            pwm_cap=MT_ROUTE_PWM_FULL_SCALE;
#endif
        *out_right=*out_left=0;
        if(vd.guard_faults||!cap||cap>600U){work_trip(w,VS_FAULT_OUTPUT,now);return 0;}
        if((uint32_t)(now-w->speed.sample_ms)>VD_SPEED_SAMPLE_MAX_AGE_MS){work_trip(w,VS_FAULT_STALE,now);return 0;}
        if(w->reset_pending){ws_pi_reset(&w->pi[0]);ws_pi_reset(&w->pi[1]);memset(w->backwards,0,sizeof(w->backwards));
            memset(w->stalled,0,sizeof(w->stalled));
#if VISION_ROUTE_SWITCH
            memset(w->slow_backwards,0,sizeof(w->slow_backwards));
#endif
            w->reset_pending=0;w->consumed=seq;periods=0;w->last_control=now;}
        if(!right&&!left){ws_pi_reset(&w->pi[0]);ws_pi_reset(&w->pi[1]);w->active=0;w->consumed=seq;w->last_control=now;return 1;}
        /* Arming the w->speed-output lease is distinct from steering settle.
         * The next hardware sample may be up to 10ms away. Do not let its
         * ISR compare against a timestamp from the zero-output interval. */
        if(!w->active){w->last_control=now;w->consumed=seq;periods=0;}
        w->active=1;
        if(periods) {
            unsigned target_ticks=w->speed.max_ticks;
#if VISION_ROUTE_SWITCH
            int track=vd.control.output.state==VC_RUNNING&&!vd.motion.phase&&!vd.probe_active;
            if(track)target_ticks=vt_route_ticks(target_ticks,
                vd.control.output.steering,vd.control.recovery_steer,vd.control.output.lateral,
                vd.control.output.paired_support,vd.control.output.edge_support);
#endif
            /* Diagnostic request is the per-run effort ceiling; max_ticks is
             * the separate full-ramp closed-loop target in counts/10ms. */
            unsigned denominator=vd.motion.requested_permille;
            if(!denominator){work_trip(w,VS_FAULT_OUTPUT,now);return 0;}
            int32_t targets[2]={(int32_t)right*target_ticks/denominator,(int32_t)left*target_ticks/denominator};
            int32_t measured[2]={(int32_t)w->speed.raw4*w->speed.sign4,(int32_t)w->speed.raw3*w->speed.sign3};
            /* No queue: the latest count cannot stand in for missed measurements. */
            if(periods>VD_SPEED_MAX_UNCONSUMED_SAMPLES){work_trip(w,VS_FAULT_BACKLOG,now);return 0;}
            if(w->sample_interval<VD_SPEED_SAMPLE_MIN_INTERVAL_MS || w->sample_interval>VD_SPEED_SAMPLE_MAX_INTERVAL_MS){work_trip(w,VS_FAULT_SAMPLE_GAP,now);return 0;}
            if(reverse){measured[0]=-measured[0];measured[1]=-measured[1];}
#if VISION_ROUTE_SWITCH
            if(track){
                unsigned rise=1;
                if(vd.control.output.speed_boost==1&&vd.control.straight_tracking&&
                   (uint32_t)(now-vd.control.straight_since_ms)>=MT_ROUTE_STRAIGHT_CONFIRM_MS)
                    rise=MT_ROUTE_STRAIGHT_TARGET_RISE;
                vt_ramp_targets(targets,w->speed.target_right,w->speed.target_left,periods*rise);
            }
#endif
            for(unsigned i=0;i<2;++i) {
#if VISION_ROUTE_SWITCH
                /* The shared w->speed envelope above retains differential even
                 * during launch. Steering geometry may change the inner target
                 * at the controller's bounded steering rate. */
                int32_t before=i?w->speed.target_left:w->speed.target_right;
                /* Do not carry straight-w->speed integral into a lower target
                 * at a junction / sharp turn. The new PI error still handles
                 * measured overspeed; no reverse torque is requested. */
                /* Do not erode torque on every one-count target fluctuation.
                 * Track only a substantial drop (>=25%, at least 3 counts). */
                if(track&&before>0&&before-targets[i]>=3&&targets[i]*4<=before*3)
                    w->pi[i].integral_q8=(int32_t)((int64_t)w->pi[i].integral_q8*targets[i]/before);
#endif
                if(targets[i]>w->speed.max_ticks){work_trip(w,VS_FAULT_TARGET,now);return 0;}
#if VISION_ROUTE_SWITCH
                /* Small counts during take-up are not proof of a driven reverse.
                 * Require meaningful drive for the fast path; persistent small
                 * negative motion at >=half cap is still stopped after 200ms. */
                /* C26_16 field starts tripped at just 3 reverse counts.
                 * Keep the 200ms persistent-reverse path, but do not turn a
                 * 30ms pulse/gear take-up transient into a latched start fault. */
                w->backwards[i]=(targets[i]>=3&&w->pi[i].output>=cap/4U&&measured[i]<-4)?w->backwards[i]+periods:0;
                w->slow_backwards[i]=(targets[i]>=3&&w->pi[i].output>=cap/2U&&measured[i]<0)?w->slow_backwards[i]+periods:0;
                if(w->slow_backwards[i]>=20){work_trip(w,i?VS_FAULT_REVERSE_LEFT:VS_FAULT_REVERSE_RIGHT,now);return 0;}
#else
                w->backwards[i]=(targets[i]>0&&measured[i]<0)?w->backwards[i]+periods:0;
#endif
                if(w->backwards[i]>=3){work_trip(w,i?VS_FAULT_REVERSE_LEFT:VS_FAULT_REVERSE_RIGHT,now);return 0;}
#if VISION_ROUTE_SWITCH
                {uint16_t ff=(uint16_t)((uint32_t)targets[i]*cap/(track?MT_ROUTE_BASE_TICKS:w->speed.max_ticks));
                 uint16_t effort=track?vp_effort_limit(w->pi[i].output,periods):pwm_cap;
                 ws_pi_step_ff_ms(&w->pi[i],targets[i],measured[i],w->speed.kp_q8,w->speed.ki_q8,effort,w->sample_interval,ff);}
#else
                ws_pi_step_ff_ms(&w->pi[i],targets[i],measured[i],w->speed.kp_q8,w->speed.ki_q8,pwm_cap,w->sample_interval,0);
#endif
                /* Keep the old no-count threshold independent of the larger
                 * PWM headroom: a w->stalled motor must not wait for full duty. */
                w->stalled[i]=(targets[i]>0&&w->pi[i].output>=(unsigned)cap*9U/10U&&measured[i]==0)?w->stalled[i]+periods:0;
                if(w->stalled[i]>=50){work_trip(w,i?VS_FAULT_NOCOUNT_LEFT:VS_FAULT_NOCOUNT_RIGHT,now);return 0;}
            }
            w->speed.target_right=(int16_t)targets[0];w->speed.target_left=(int16_t)targets[1];
            w->speed.measured_right=(int16_t)measured[0];w->speed.measured_left=(int16_t)measured[1];
            w->consumed=seq;w->last_control=now;
        }
        w->speed.pwm_right=*out_right=w->pi[0].output;w->speed.pwm_left=*out_left=w->pi[1].output;
    }
#else
    (void)reverse;(void)cap;
#endif
    return 1;
}

#endif

/* Target publication never consumes a sample or advances PI. All three APIs
 * below are called with interrupts masked by the foreground/output facade. */
static int speed_publish(uint16_t right,uint16_t left,int reverse,uint16_t cap,
                    uint16_t *out_right,uint16_t *out_left)
{
    *out_right=right;*out_left=left;
#if VISION_ENCODER_ENABLE
    if(speed.enabled) {
        uint32_t now=timer_get(GPT_TIM_1);
        *out_right=*out_left=0;
        if(speed.faults || vd.guard_faults)return 0;
        if(!cap || cap>600U || right>cap || left>cap) {
            trip(VS_FAULT_OUTPUT,now);return 0;
        }
        if((uint32_t)(now-speed.sample_ms)>VD_SPEED_SAMPLE_MAX_AGE_MS){trip(VS_FAULT_STALE,now);return 0;}
        request_right=right;request_left=left;request_reverse=reverse;request_cap=cap;
        /* Zero targets take effect immediately, without waiting for service. */
        if(!right&&!left) {
            vd_speed_stop();return 1;
        }
        *out_right=speed.pwm_right;*out_left=speed.pwm_left;
    }
#else
    (void)reverse;(void)cap;
#endif
    return 1;
}
int vd_speed_output(uint16_t right,uint16_t left,int reverse,uint16_t cap,
                    uint16_t *out_right,uint16_t *out_left)
{
    uint32_t mask=__get_PRIMASK();int ok;__disable_irq();
    ok=speed_publish(right,left,reverse,cap,out_right,out_left);
    __DMB();__set_PRIMASK(mask);return ok;
}
void vd_speed_control_poll(uint32_t now)
{
#if VISION_ENCODER_ENABLE
    uint32_t mask=__get_PRIMASK(),epoch,seq;uint16_t right,left,cap,req_r,req_l;
    int reverse;speed_work_t w;
    __disable_irq();
    if(!speed.enabled || !request_cap || speed.faults || vd.guard_faults){__set_PRIMASK(mask);return;}
    if((uint32_t)(now-speed.sample_ms)>VD_SPEED_SAMPLE_MAX_AGE_MS) {
        trip(VS_FAULT_STALE,now);__set_PRIMASK(mask);return;
    }
    epoch=vd.output_epoch;seq=speed.samples;
    w.speed=speed;memcpy(w.pi,pi,sizeof(pi));w.consumed=consumed;w.last_control=last_control;
    w.sample_interval=sample_interval;w.active=active;w.reset_pending=reset_pending;
    memcpy(w.backwards,backwards,sizeof(backwards));memcpy(w.stalled,stalled,sizeof(stalled));
#if VISION_ROUTE_SWITCH
    memcpy(w.slow_backwards,slow_backwards,sizeof(slow_backwards));
#endif
    req_r=request_right;req_l=request_left;cap=request_cap;reverse=request_reverse;
#if VISION_MIDTERM_C31_AUTONOMOUS
    __set_PRIMASK(mask);
#endif
    (void)speed_control_output(&w,req_r,req_l,reverse,cap,&right,&left);
#if VISION_MIDTERM_C31_AUTONOMOUS
#ifdef VISION_HOST_OUTPUT_HOOK
    {extern void vision_test_output_hook(unsigned stage);vision_test_output_hook(2);}
#endif
    __disable_irq();
#endif
    /* A stop/reset or a newer sample invalidates all speculative PI state.
     * Never overwrite an ISR's newer counts or resurrect its cleared PI. */
    if(epoch==vd.output_epoch && seq==speed.samples && cap==request_cap &&
       req_r==request_right && req_l==request_left && !vd.guard_faults) {
        if(w.consumed!=consumed)st_control(w.consumed,w.last_control,w.speed.target_right,w.speed.target_left,w.speed.measured_right,w.speed.measured_left,w.pi[0].integral_q8,w.pi[1].integral_q8);
        speed=w.speed;memcpy(pi,w.pi,sizeof(pi));consumed=w.consumed;last_control=w.last_control;
        active=w.active;reset_pending=w.reset_pending;
        memcpy(backwards,w.backwards,sizeof(backwards));memcpy(stalled,w.stalled,sizeof(stalled));
#if VISION_ROUTE_SWITCH
        memcpy(slow_backwards,w.slow_backwards,sizeof(slow_backwards));
#endif
        if(w.speed.faults){vd.guard_faults|=w.speed.faults;vd_output_zero();}
    }
    __DMB();__set_PRIMASK(mask);
#else
    (void)now;
#endif
}
