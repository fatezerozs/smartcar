/* Independent route lifecycle scenarios; simulated IO only. */
#include "../support/fixtures/route_lane.h"
static void route_step(int frames,int heartbeat)
{
    tick(-15,15,heartbeat);
    if(frames && now%20U==0)vision_drive_observe(&route_lane,++route_sequence,now,now);
}
static void route_boot(void)
{
    now=0;raw3=raw4=0;mask=0;route_sequence=0;memset(pwm,0,sizeof(pwm));
    vision_drive_init(now);app_mode_init(0,0,now);app_mode_sample(0,0,30);(void)app_mode_take_change();
    set_lane(0);route_step(1,0);
}
static void warm_arm(void)
{
    for(unsigned i=0;i<30;++i)route_step(1,0);
    CHECK(vision_drive_command('a',now,1));
}
int main(void)
{
    vd_profile_t p;
    route_boot();vision_drive_get_profile(&p);
    CHECK(vision_drive_profile_ready(&p));CHECK(p.control.max_run_ms==60000);
    CHECK(p.servo_center_us==1550&&p.servo_min_us==1500&&p.servo_max_us==1600&&p.right_sign==-1);
    CHECK(p.control.axis_near_x==MT_ROUTE_AXIS_NEAR_X&&p.control.axis_far_x==MT_ROUTE_AXIS_FAR_X);
    CHECK(!p.autostart_enabled&&p.motor1_forward_level==1&&p.motor2_forward_level==1);
    CHECK(info().enabled&&info().sign3==-1&&info().sign4==1&&info().max_ticks==40);
    CHECK(info().kp_q8==256&&info().ki_q8==12);
    CHECK(!vision_drive_command('a',now,1)); /* No lane evidence yet. */
    for(unsigned i=0;i<1000;++i){route_step(1,0);CHECK(!pwm[0]&&!pwm[1]);}
    CHECK(vision_drive_output()->state==VC_DISARMED); /* No boot/3s/5s launch. */
    warm_arm();
    for(unsigned i=0;i<1200;++i){
        set_lane(i<400?-15:i<800?15:0);route_step(1,1);
        CHECK(vision_drive_output()->state==VC_RUNNING&&!vision_drive_output()->faults);
        CHECK(info().pwm_right<=150&&info().pwm_left<=150);
    }
    vision_drive_stop();CHECK(!pwm[0]&&!pwm[1]);
    for(unsigned i=0;i<100;++i)route_step(1,1);
    CHECK(vision_drive_output()->state==VC_DISARMED); /* Heartbeats never re-arm. */
    route_boot();warm_arm();
    for(unsigned i=0;i<60;++i)route_step(1,0);
    CHECK(vision_drive_output()->faults&VC_FAULT_COMMAND);CHECK(!pwm[0]&&!pwm[1]);
    route_boot();warm_arm();
    route_lane.valid=0;route_step(1,1);route_step(1,1);
    CHECK(vision_drive_output()->faults&VC_FAULT_VISION);CHECK(!pwm[0]&&!pwm[1]);
    route_boot();warm_arm();
    for(unsigned i=0;i<12;++i)route_step(0,1);
    CHECK(vision_drive_output()->faults&VC_FAULT_FRAME);CHECK(!pwm[0]&&!pwm[1]);
    route_boot();warm_arm();
    for(unsigned i=0;i<6010;++i)route_step(1,1);
    CHECK(vision_drive_output()->faults&VC_FAULT_RUNTIME);CHECK(!pwm[0]&&!pwm[1]);
    CHECK(vision_drive_command('c',now,1));
    for(unsigned i=0;i<50;++i)route_step(1,1);
    CHECK(vision_drive_output()->state==VC_DISARMED&&!pwm[0]&&!pwm[1]);
    vision_drive_get_profile(&p);CHECK(vision_drive_configure(&p));CHECK(!info().enabled);
    for(unsigned i=0;i<50;++i)route_step(1,1);
    CHECK(!info().enabled); /* Boot helper must not undo explicit reconfiguration. */
    printf("ROUTE TRAIN PASS: %u checks; synthetic left/right offsets, not physical curve acceptance.\n",checks);
    return 0;
}
