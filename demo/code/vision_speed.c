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
        "reverse_right","reverse_left","no_count_right","no_count_left"};
    return detail<sizeof(names)/sizeof(names[0])?names[detail]:"unknown";
}
#if VISION_ENCODER_ENABLE
/* Only the output layer runs PI (under its IRQ mask). The 10ms ISR samples
 * modular counters and may cut output; it never starts motors or parses UART. */
static ws_pi_t pi[2];
static uint16_t previous[2];
static uint32_t consumed,last_control;
static uint32_t sample_interval;
static volatile uint8_t active,reset_pending;
static unsigned backwards[2],stalled[2];
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
#endif
}
void vd_speed_stop(void)
{
#if VISION_ENCODER_ENABLE
    active=0;reset_pending=1;
    ws_pi_reset(&pi[0]);ws_pi_reset(&pi[1]); /* Integer-only, including from PIT1 STOP. */
    speed.target_right=speed.target_left=0;speed.pwm_right=speed.pwm_left=0;
#endif
}
void vd_speed_sample_tick(uint32_t now)
{
#if VISION_ENCODER_ENABLE
    uint16_t current[2];uint32_t elapsed=now-speed.sample_ms;
    if(elapsed<10U)return;
    board_encoder_read(&current[0],&current[1]);
    {int32_t d3=(uint16_t)(current[0]-previous[0]);int32_t d4=(uint16_t)(current[1]-previous[1]);
     if(d3>=32768)d3-=65536;if(d4>=32768)d4-=65536;
     speed.raw3=(int16_t)((int64_t)d3*10/elapsed);speed.raw4=(int16_t)((int64_t)d4*10/elapsed);
     speed.total3=total_add(speed.total3,d3);speed.total4=total_add(speed.total4,d4);}
    previous[0]=current[0];previous[1]=current[1];speed.sample_ms=now;sample_interval=elapsed;++speed.samples;
    if(speed.enabled&&active) {
        if(elapsed>20U)trip(VS_FAULT_SAMPLE_GAP,now);
        else if((uint32_t)(now-last_control)>=50U)trip(VS_FAULT_CONTROL_GAP,now);
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
    if(!stopped()||!max_ticks||max_ticks>1000||!(kp>0&&kp<=100)||!(ki>=0&&ki<=10))return 0;
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
        vd.control.config.duty_cap<=0||vd.control.config.duty_cap>(VISION_ROUTE_SWITCH?.18f:.15f)||
        vd.profile.motor1_scale!=1.0f||vd.profile.motor2_scale!=1.0f||
        !speed.samples||(uint32_t)(timer_get(GPT_TIM_1)-speed.sample_ms)>30U))return 0;
    speed.enabled=(uint8_t)enable;speed.faults=0;speed.fault_detail=0;
    speed.fault_control_age_ms=speed.fault_sample_age_ms=speed.fault_interval_ms=0;
    vd_speed_stop();return 1;
#else
    return enable==0;
#endif
}
int vd_speed_output(uint16_t right,uint16_t left,int reverse,uint16_t cap,uint16_t *out_right,uint16_t *out_left)
{
    *out_right=right;*out_left=left;
#if VISION_ENCODER_ENABLE
    if(speed.enabled) {
        uint32_t now=timer_get(GPT_TIM_1),seq=speed.samples,periods=seq-consumed;
        uint16_t pwm_cap=cap;
#if VISION_ROUTE_SWITCH
        if(vd.control.output.state==VC_RUNNING&&!vd.motion.phase&&!vd.probe_active)
            pwm_cap=MT_ROUTE_PWM_FULL_SCALE;
#endif
        *out_right=*out_left=0;
        if(vd.guard_faults||!cap||cap>(VISION_ROUTE_SWITCH?180U:150U)){trip(VS_FAULT_OUTPUT,now);return 0;}
        if((uint32_t)(now-speed.sample_ms)>30U){trip(VS_FAULT_STALE,now);return 0;}
        if(reset_pending){ws_pi_reset(&pi[0]);ws_pi_reset(&pi[1]);memset(backwards,0,sizeof(backwards));
            memset(stalled,0,sizeof(stalled));
#if VISION_ROUTE_SWITCH
            memset(slow_backwards,0,sizeof(slow_backwards));
#endif
            reset_pending=0;consumed=seq;periods=0;last_control=now;}
        if(!right&&!left){ws_pi_reset(&pi[0]);ws_pi_reset(&pi[1]);active=0;consumed=seq;last_control=now;return 1;}
        /* Arming the speed-output lease is distinct from steering settle.
         * The next hardware sample may be up to 10ms away. Do not let its
         * ISR compare against a timestamp from the zero-output interval. */
        if(!active){last_control=now;consumed=seq;periods=0;}
        active=1;
        if(periods) {
            unsigned target_ticks=speed.max_ticks;
#if VISION_ROUTE_SWITCH
            int track=vd.control.output.state==VC_RUNNING&&!vd.motion.phase&&!vd.probe_active;
            if(track)target_ticks=vt_route_ticks(target_ticks,
                vd.control.output.steering,vd.control.recovery_steer,vd.control.output.lateral,
                vd.control.output.paired_support,vd.control.output.edge_support);
#endif
            int32_t targets[2]={(int32_t)right*target_ticks/cap,(int32_t)left*target_ticks/cap};
            int32_t measured[2]={(int32_t)speed.raw4*speed.sign4,(int32_t)speed.raw3*speed.sign3};
            if(periods>5){trip(VS_FAULT_BACKLOG,now);return 0;}
            if(reverse){measured[0]=-measured[0];measured[1]=-measured[1];}
#if VISION_ROUTE_SWITCH
            if(track){
                unsigned rise=1;
                if(vd.control.output.speed_boost==1&&vd.control.straight_tracking&&
                   (uint32_t)(now-vd.control.straight_since_ms)>=MT_ROUTE_STRAIGHT_CONFIRM_MS)
                    rise=MT_ROUTE_STRAIGHT_TARGET_RISE;
                vt_ramp_targets(targets,speed.target_right,speed.target_left,periods*rise);
            }
#endif
            for(unsigned i=0;i<2;++i) {
#if VISION_ROUTE_SWITCH
                /* The shared speed envelope above retains differential even
                 * during launch. Steering geometry may change the inner target
                 * at the controller's bounded steering rate. */
                int32_t before=i?speed.target_left:speed.target_right;
                /* Do not carry straight-speed integral into a lower target
                 * at a junction / sharp turn. The new PI error still handles
                 * measured overspeed; no reverse torque is requested. */
                /* Do not erode torque on every one-count target fluctuation.
                 * Track only a substantial drop (>=25%, at least 3 counts). */
                if(track&&before>0&&before-targets[i]>=3&&targets[i]*4<=before*3)
                    pi[i].integral_q8=(int32_t)((int64_t)pi[i].integral_q8*targets[i]/before);
#endif
                if(targets[i]>speed.max_ticks){trip(VS_FAULT_TARGET,now);return 0;}
#if VISION_ROUTE_SWITCH
                /* Small counts during take-up are not proof of a driven reverse.
                 * Require meaningful drive for the fast path; persistent small
                 * negative motion at >=half cap is still stopped after 200ms. */
                /* C26_16 field starts tripped at just 3 reverse counts.
                 * Keep the 200ms persistent-reverse path, but do not turn a
                 * 30ms pulse/gear take-up transient into a latched start fault. */
                backwards[i]=(targets[i]>=3&&pi[i].output>=cap/4U&&measured[i]<-4)?backwards[i]+periods:0;
                slow_backwards[i]=(targets[i]>=3&&pi[i].output>=cap/2U&&measured[i]<0)?slow_backwards[i]+periods:0;
                if(slow_backwards[i]>=20){trip(i?VS_FAULT_REVERSE_LEFT:VS_FAULT_REVERSE_RIGHT,now);return 0;}
#else
                backwards[i]=(targets[i]>0&&measured[i]<0)?backwards[i]+periods:0;
#endif
                if(backwards[i]>=3){trip(i?VS_FAULT_REVERSE_LEFT:VS_FAULT_REVERSE_RIGHT,now);return 0;}
#if VISION_ROUTE_SWITCH
                {uint16_t ff=(uint16_t)((uint32_t)targets[i]*cap/(track?MT_ROUTE_BASE_TICKS:speed.max_ticks));
                 uint16_t effort=track?vp_effort_limit(pi[i].output,periods):pwm_cap;
                 ws_pi_step_ff(&pi[i],targets[i],measured[i],speed.kp_q8,speed.ki_q8,effort,periods,ff);}
#else
                ws_pi_step(&pi[i],targets[i],measured[i],speed.kp_q8,speed.ki_q8,pwm_cap,periods);
#endif
                /* Keep the old no-count threshold independent of the larger
                 * PWM headroom: a stalled motor must not wait for full duty. */
                stalled[i]=(targets[i]>0&&pi[i].output>=(unsigned)cap*9U/10U&&measured[i]==0)?stalled[i]+periods:0;
                if(stalled[i]>=50){trip(i?VS_FAULT_NOCOUNT_LEFT:VS_FAULT_NOCOUNT_RIGHT,now);return 0;}
            }
            speed.target_right=(int16_t)targets[0];speed.target_left=(int16_t)targets[1];
            speed.measured_right=(int16_t)measured[0];speed.measured_left=(int16_t)measured[1];
            consumed=seq;last_control=now;
        }
        speed.pwm_right=*out_right=pi[0].output;speed.pwm_left=*out_left=pi[1].output;
    }
#else
    (void)reverse;(void)cap;
#endif
    return 1;
}
