/* Exercise real facade/PI publication with deterministic ISR interleavings.
 * Host-only hooks do not exist in target builds; no physical timing claim. */
#define pwm_set_duty fixture_pwm_set_duty
#define gpio_set_level fixture_gpio_set_level
#include "../support/fixtures/switch_io.h"
#undef pwm_set_duty
#undef gpio_set_level
#include "vision_drive_internal.h"
#include <math.h>

static unsigned pwm_writes,stage_hits[5],inject_stage,action,expected_mask;
static vd_speed_info_t interrupted;
enum { NONE, STOP, SWITCH_OFF, MODE_CHANGE, SAMPLE, REQUEST, REQUEST_ABA,
       REVERSE_REQUEST, ZERO_REQUEST, EXPIRE, REVOKE, SNAPSHOT_INPUT };

void pwm_set_duty(int channel,uint32_t duty)
{
    CHECK(mask==1);++pwm_writes;fixture_pwm_set_duty(channel,duty);
}
void gpio_set_level(int pin,int level)
{
    CHECK(mask==1);fixture_gpio_set_level(pin,level);
}
static void publish(uint16_t right,uint16_t left,int reverse)
{
    uint16_t r,l;CHECK(vd_speed_output(right,left,reverse,180,&r,&l));
}
void vision_test_output_hook(unsigned stage)
{
    unsigned selected=action;
    CHECK(stage>0&&stage<5);++stage_hits[stage];CHECK(mask==expected_mask);
    if(stage!=inject_stage||!selected)return;
    action=NONE; /* Stop paths may call back into the output facade. */
    switch(selected) {
    case STOP: vision_drive_stop();break;
    case SWITCH_OFF: vision_drive_switch_sample(0);break;
    case MODE_CHANGE: app_mode_sample(1,1,now);break;
    case SAMPLE:
        now+=10;raw3=(uint16_t)(raw3-7);raw4=(uint16_t)(raw4+9);
        vd_speed_sample_tick(now);break;
    case REQUEST: publish(50,60,0);break;
    case REQUEST_ABA: publish(50,60,0);publish(90,100,0);break;
    case REVERSE_REQUEST: publish(90,100,1);break;
    case ZERO_REQUEST: publish(0,0,0);break;
    case EXPIRE: now+=VD_SPEED_SAMPLE_MAX_AGE_MS+1U;break;
    case REVOKE: vd_output_zero();break;
    case SNAPSHOT_INPUT:
        /* Deliberate test-only mutation of task-owned values. The current
         * preparation must use its captured steering/support, not mix reads.
         * Production still requires a single serialized foreground owner. */
        vd.control.output.steering=.8f;vd.control.output.lateral=2.0f;
        vd.control.output.paired_support=0;vd.control.output.edge_support=1;break;
    default: CHECK(0);
    }
    interrupted=info();
}
static void zero(void)
{
    vd_speed_info_t s=info();
    CHECK(!pwm[0]&&!pwm[1]);
    CHECK(!s.target_right&&!s.target_left&&!s.pwm_right&&!s.pwm_left);
}
static void boot_running(void)
{
    action=NONE;expected_mask=0;inject_stage=0;
    switch_boot(0);switch_wait(30,1);CHECK(vision_drive_is_local());
    CHECK(vd.output_authorized&&pwm[0]&&pwm[1]);CHECK(!mask);
}
static void pending_sample(void)
{
    boot_running();switch_wait(100,1); /* Clear the launch ramp before comparing geometry. */
    vd.control.output.steering=0;vd.control.recovery_steer=0;vd.control.output.lateral=0;
    vd.control.output.paired_support=1;vd.control.output.edge_support=0;
    publish(90,100,0);vd_speed_control_poll(now);
    now+=10;raw3=(uint16_t)(raw3-12);raw4=(uint16_t)(raw4+12);
    vd_speed_sample_tick(now);
}
static void same_control(vd_speed_info_t a,vd_speed_info_t b)
{
    CHECK(a.target_right==b.target_right&&a.target_left==b.target_left);
    CHECK(a.pwm_right==b.pwm_right&&a.pwm_left==b.pwm_left);
}
static void output_interleavings(void)
{
    const unsigned cases[]={STOP,SWITCH_OFF,MODE_CHANGE,REVOKE,EXPIRE};
    for(unsigned stage=1;stage<=4;stage+=3)for(unsigned n=0;n<sizeof(cases)/sizeof(cases[0]);++n) {
        boot_running();unsigned before=pwm_writes;
        inject_stage=stage;action=cases[n];vd_output_apply();
        CHECK(!action&&!mask&&!vd.output_authorized);zero();
        /* STOP may write zero more than once, but must never recreate PWM. */
        CHECK(pwm_writes>=before);vd_speed_control_poll(now);zero();
        vd_output_apply();zero();
    }
    /* Startup used to inherit an outer PRIMASK across the whole output path. */
    CHECK(stage_hits[1]&&stage_hits[4]);
    boot_running();uint32_t original=pwm[2];
    inject_stage=4;action=SNAPSHOT_INPUT;vd_output_apply();
    CHECK(pwm[2]==original); /* Steering preparation used the captured input. */
    boot_running();vd.control.output.steering=NAN;vd_output_apply();
    CHECK(vd.guard_faults&VC_FAULT_NUMERIC);zero();CHECK(!mask);
    boot_running();expected_mask=1;mask=1;vd_output_apply();CHECK(mask==1);
    mask=0;expected_mask=0;
}
static void pi_interleavings(void)
{
    const unsigned cases[]={STOP,SWITCH_OFF,MODE_CHANGE,SAMPLE,REQUEST,REQUEST_ABA,
                            REVERSE_REQUEST,ZERO_REQUEST,EXPIRE,REVOKE};
    for(unsigned stage=2;stage<=3;++stage)for(unsigned n=0;n<sizeof(cases)/sizeof(cases[0]);++n) {
        pending_sample();vd_speed_info_t before=info();
        inject_stage=stage;action=cases[n];vd_speed_control_poll(now);
        vd_speed_info_t after=info();CHECK(!action&&!mask);
        if(cases[n]==STOP||cases[n]==SWITCH_OFF||cases[n]==REVOKE||cases[n]==EXPIRE) {
            zero();CHECK(!vd.output_authorized);
            if(cases[n]==EXPIRE)CHECK(after.fault_detail==VS_FAULT_STALE);
        } else if(cases[n]==ZERO_REQUEST) {
            same_control(after,interrupted); /* No epoch change: request version matters. */
            CHECK(!after.target_left&&!after.target_right&&!after.pwm_left&&!after.pwm_right);
        } else {
            same_control(after,before); /* Discard the entire speculative PI state. */
            CHECK(after.samples==interrupted.samples&&after.total3==interrupted.total3&&after.total4==interrupted.total4);
            if(cases[n]==SAMPLE) {
                CHECK(after.samples==before.samples+1&&after.raw3==-7&&after.raw4==9);
                /* The newest sample cannot cover the now-missed prior sample. */
                vd_speed_control_poll(now);CHECK(info().fault_detail==VS_FAULT_BACKLOG);zero();
            }
        }
    }
    pending_sample();vd_speed_info_t before=info();vd_speed_control_poll(now);
    vd_speed_info_t after=info();CHECK(!after.faults);
    CHECK(after.target_right!=before.target_right||after.target_left!=before.target_left);
    vd_speed_control_poll(now);same_control(info(),after); /* At most once per sample. */
    /* A test-only live geometry change during calculation cannot mix inputs. */
    pending_sample();inject_stage=3;action=SNAPSHOT_INPUT;vd_speed_control_poll(now);
    same_control(info(),after);
    /* ABA rejection must not consume a sample or change the hidden integral. */
    pending_sample();inject_stage=2;action=REQUEST_ABA;vd_speed_control_poll(now);
    vd_speed_control_poll(now);same_control(info(),after);
    pending_sample();expected_mask=1;mask=1;vd_speed_control_poll(now);CHECK(mask==1);
    mask=0;expected_mask=0;
}
int main(void)
{
    output_interleavings();pi_interleavings();
    printf("C26 SNAPSHOT PASS: %u checks; ISR interleavings and mask restoration, no target timing claim.\n",checks);
    return 0;
}
