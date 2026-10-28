/* Real C31 controller, wheel loop and guard; board IO is simulated. */
#define SWITCH_TEST_FUNCTION legacy_switch_main
#include "test_route_switch.c"
#include "midterm_boot.h"
#include "vision_drive_internal.h"
static int buzzer,block_local;
void board_buzzer_set(int on){buzzer=on;}
static void auto_boot(uint32_t start)
{
    switch_boot(1);now=start;block_local=0;
    vision_drive_init(now);app_mode_init(1,1,now);
    vision_drive_set_local_allowed(1);midterm_boot_init(start,now);
}
static void auto_step(int frames)
{
    switch_a=(now/40U)%2U;switch_b=(now/70U)%2U;
    now+=10;raw3=(uint16_t)(raw3+switch_delta_left);raw4=(uint16_t)(raw4+switch_delta_right);
    app_mode_sample(switch_a,switch_b,now);vision_drive_safety_tick();
    if(app_mode_take_change())vd_route_switch_mode_stop();
    vision_drive_set_local_allowed(app_mode_allows(APP_MODE_DRIVE) && !block_local);
    vision_drive_poll(now);
    if(frames && now%20U==0)vision_drive_observe(&route_lane,++route_sequence,now,now);
    midterm_boot_poll(now);
    CHECK(app_mode_current()==APP_MODE_DRIVE&&app_mode_allows(APP_MODE_DRIVE));
    CHECK(!app_mode_take_change());
    vd_route_switch_poll(now); /* Even direct legacy calls cannot arm/clear. */
}
static void wait_steps(unsigned n,int frames){while(n--)auto_step(frames);}
static void wait_start(void)
{
    unsigned n=0;
    while(midterm_boot_state()==MT_BOOT_WAIT&&n++<400)auto_step(1);
#if MIDTERM_VISUAL_REVIEWED
    CHECK(midterm_boot_state()==MT_BOOT_RUNNING);
#else
    CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);
#endif
}
static void latched(void)
{
    wait_steps(400,1);
    CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]&&!pwm[2]);
    CHECK(!vision_drive_command('a',now,1)&&!vision_drive_command('c',now,1));
}
static void legacy_entries_rejected(void)
{
    unsigned char before[sizeof(vd)],outputs[sizeof(pwm)];
    memcpy(before,&vd,sizeof(vd));memcpy(outputs,pwm,sizeof(pwm));
    vision_drive_switch_sample(0);vision_drive_switch_sample(1);
    CHECK(memcmp(before,&vd,sizeof(vd))==0);
    CHECK(memcmp(outputs,pwm,sizeof(pwm))==0);
    CHECK(!vision_drive_servo_probe(VISION_SERVO_PROBE_INITIAL_US,VISION_SERVO_PROBE_MIN_MS,now));
    CHECK(!vision_drive_motor_probe(1,VISION_MOTOR_PROBE_INITIAL_PERMILLE,0,VISION_MOTOR_PROBE_MIN_MS,now));
    CHECK(!vision_drive_motor_probe(2,VISION_MOTOR_PROBE_INITIAL_PERMILLE,1,VISION_MOTOR_PROBE_MIN_MS,now));
    CHECK(!vision_drive_motion_start(VD_MOTION_LINE,VISION_MOTION_INITIAL_PERMILLE,
        VISION_MOTION_MIN_MS,vd.profile.servo_center_us,now));
    CHECK(!vision_drive_motion_reverse(now,1));
    CHECK(!vision_drive_motion_active()&&!vd.probe_active);
    CHECK(memcmp(before,&vd,sizeof(vd))==0);
    CHECK(memcmp(outputs,pwm,sizeof(pwm))==0);
}
int main(void)
{
    auto_boot(0);legacy_entries_rejected();wait_steps(300,0);
    CHECK(midterm_boot_state()==MT_BOOT_WAIT&&!pwm[0]&&!pwm[1]); /* uptime is irrelevant */
    wait_steps(300,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);
    CHECK(info().max_ticks==132&&info().sign3==-1&&info().sign4==1);
    CHECK(info().kp_q8==256&&info().ki_q8==12&&info().enabled);
    /* Even stale writes from legacy code cannot veto autonomous arm/output. */
    vd.switch_inhibit=1;vd.switch_stop_pending=1;vd.switch_seen=1;vd.switch_closed=0;
    legacy_entries_rejected();
    wait_start();
#if MIDTERM_VISUAL_REVIEWED
    wait_steps(60,1);CHECK(pwm[0]&&pwm[1]);
    legacy_entries_rejected();vd_output_apply();CHECK(pwm[0]&&pwm[1]);
    vision_drive_stop();midterm_boot_poll(now);
    CHECK(midterm_boot_state()==MT_BOOT_STOPPED);latched();
    auto_boot(0);wait_start();wait_steps(60,1);
    wait_steps(12,0);CHECK(midterm_boot_state()==MT_BOOT_STOPPED);latched();
    auto_boot(0);wait_start();wait_steps(60,1);
    CHECK(vd.output_authorized&&vd.guard_running&&!vd.guard_faults&&!vd.autonomous_stopped);
    CHECK(vd.control.have_frame&&vd.control.output.state==VC_RUNNING&&pwm[0]&&pwm[1]);
    /* Foreground sees the exact timeout boundary before any safety tick. */
    now=vd.control.last_frame_ms+VISION_CONTROL_FRAME_TIMEOUT_MS;
    vd_output_apply();
    CHECK(vd.autonomous_stopped&&vd.guard_faults==VC_FAULT_FRAME);
    CHECK(!vd.output_authorized&&!vd.guard_running&&!pwm[0]&&!pwm[1]&&!pwm[2]);
    CHECK(!vd_output_arm(now,1));
    midterm_boot_poll(now);
    CHECK(midterm_boot_state()==MT_BOOT_STOPPED);latched();
    auto_boot(0);wait_start();route_lane.valid=0;wait_steps(2,1);
    CHECK(midterm_boot_state()==MT_BOOT_STOPPED);set_lane(0);latched();
    auto_boot(0);wait_start();wait_steps(60,1);
    switch_delta_left=8;switch_delta_right=-8;wait_steps(80,1);
    CHECK(info().faults&&midterm_boot_state()==MT_BOOT_STOPPED);latched();
#else
    latched(); /* Default review is actually omitted from compiler flags. */
#endif
    auto_boot(0);wait_steps(200,1);route_lane.valid=0;wait_steps(2,1);
    CHECK(midterm_boot_state()==MT_BOOT_WAIT);set_lane(0);
    wait_steps(300,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    auto_boot(0);wait_steps(200,1);wait_steps(12,0);
    CHECK(midterm_boot_state()==MT_BOOT_WAIT);
    wait_steps(300,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    auto_boot(0);wait_steps(200,1);
    /* No foreground poll over a timeout, then a fresh frame: reset continuity. */
    now+=200;raw3=(uint16_t)(raw3-15);raw4=(uint16_t)(raw4+15);
    vision_drive_safety_tick();vision_drive_observe(&route_lane,++route_sequence,now,now);
    midterm_boot_poll(now);wait_steps(290,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    auto_boot(0);wait_steps(200,1);
    /* Duplicate sequence with updated capture times must not extend continuity. */
    for(unsigned i=0;i<350;++i){
        auto_step(0);vision_drive_observe(&route_lane,route_sequence,now,now);
        midterm_boot_poll(now);CHECK(midterm_boot_state()==MT_BOOT_WAIT&&!pwm[0]&&!pwm[1]);
    }
    wait_steps(300,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    auto_boot(0);wait_steps(200,1);
    /* New sequences carrying stale capture timestamps never qualify. */
    for(unsigned i=0;i<350;++i){auto_step(0);vision_drive_observe(&route_lane,++route_sequence,now-120U,now);}
    CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_steps(300,1);
    CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    auto_boot(0);wait_steps(200,1);vd.control.output.far_valid=0;midterm_boot_poll(now);
    wait_steps(290,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    auto_boot(0);wait_steps(200,1);vision_drive_stop();
    midterm_boot_poll(now);CHECK(midterm_boot_state()==MT_BOOT_STOPPED);latched();
    auto_boot(0);wait_steps(200,1);vd.guard_faults|=VC_FAULT_NUMERIC;midterm_boot_poll(now);
    CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);latched();
    auto_boot(0);block_local=1;wait_steps(400,1);
    CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);latched();
    auto_boot(0);vd.hardware_ready=0;wait_steps(400,1);
    CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);latched();
    auto_boot(0xFFFFFC18U);wait_steps(300,1);CHECK(midterm_boot_state()==MT_BOOT_WAIT);wait_start();
    vision_drive_stop();wait_steps(70,0);CHECK(!buzzer);
    printf("AUTO TRACK PASS: %u checks; review=%d, simulated IO only.\n",checks,MIDTERM_VISUAL_REVIEWED);
    return 0;
}
