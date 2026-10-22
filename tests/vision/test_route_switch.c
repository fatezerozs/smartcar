#define ROUTE_TEST_FUNCTION legacy_route_main
#include "test_route_trial.c"
#undef main
static int switch_a,switch_b;
static int switch_delta_left=-15,switch_delta_right=15;
static void switch_step(int frames)
{
    now+=10;raw3=(uint16_t)(raw3+switch_delta_left);raw4=(uint16_t)(raw4+switch_delta_right);
    app_mode_sample(switch_a,switch_b,now);vision_drive_safety_tick();
    if(app_mode_take_change())vd_route_switch_mode_stop();
    vision_drive_set_local_allowed(app_mode_allows(APP_MODE_DRIVE));vision_drive_poll(now);
    if(frames&&now%20U==0)vision_drive_observe(&route_lane,++route_sequence,now,now);
}
static void switch_wait(unsigned n,int frames){while(n--)switch_step(frames);}
static void switch_boot(int a)
{
    now=0;raw3=raw4=0;mask=0;route_sequence=0;switch_a=a;switch_b=1;switch_delta_left=-15;switch_delta_right=15;
    memset(pwm,0,sizeof(pwm));app_mode_init(a,switch_b,now);vision_drive_init(now);set_lane(0);
}
#ifndef SWITCH_TEST_FUNCTION
#define SWITCH_TEST_FUNCTION main
#endif
int SWITCH_TEST_FUNCTION(void)
{
    vd_last_stop_t before,after;
    switch_boot(1);switch_wait(300,1);CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
    switch_b=0;switch_wait(10,1);CHECK(!vision_drive_is_local());
    vision_drive_stop();switch_wait(5,1); /* STOP/query while already OFF. */
    switch_a=0;switch_wait(30,1);CHECK(vision_drive_is_local());
    switch_wait(1200,1);CHECK(vision_drive_is_local()); /* No UART heartbeat. */
    while(now%20U)switch_step(1);
    route_lane.valid=0;switch_wait(2,1);CHECK(vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
    set_lane(0);switch_wait(2,1);CHECK(vision_drive_is_local());
    route_lane.valid=0;switch_wait(100,1);CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
    vision_drive_last_stop(&before);CHECK(before.sequence&&(before.faults&VC_FAULT_VISION));
    set_lane(0);switch_wait(6,1);CHECK(!vision_drive_is_local()); /* fewer than five new frames */
    switch_wait(30,1);CHECK(vision_drive_is_local()); /* No OFF->ON needed. */
    vision_drive_last_stop(&after);CHECK(after.sequence==before.sequence&&after.faults==before.faults);
    switch_wait(12,0);CHECK(!pwm[0]&&!pwm[1]);
    switch_wait(30,1);CHECK(vision_drive_is_local()); /* Recovered camera stream. */
    switch_b=1;switch_wait(10,1);CHECK(vision_drive_is_local());
    switch_a=1;switch_step(1);CHECK(!pwm[0]&&!pwm[1]);switch_wait(100,1);CHECK(!vision_drive_is_local());
    vision_drive_last_stop(&after);CHECK(after.sequence>before.sequence);
    switch_boot(0);switch_wait(30,1);CHECK(vision_drive_is_local()); /* Boot ON is now intentional. */
    vision_drive_stop();switch_wait(100,1);CHECK(!vision_drive_is_local()); /* Explicit STOP remains effective. */
    switch_a=1;switch_wait(10,1);switch_a=0;switch_wait(30,1);CHECK(vision_drive_is_local());
    switch_wait(12010,1);CHECK(vision_drive_is_local()&&pwm[0]&&pwm[1]);
    switch_wait(60010,1);CHECK(vision_drive_is_local()); /* No former 2-minute deadline. */
    switch_a=1;switch_wait(1,1);CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
    switch_wait(100,1);CHECK(!vision_drive_is_local());
    switch_boot(0);route_lane.valid=0;switch_wait(300,1);CHECK(!pwm[0]&&!pwm[1]);
    set_lane(0);switch_wait(30,1);CHECK(vision_drive_is_local()); /* Waiting for a lane, no request expiry. */
    /* Real stop was reverse_right after 61ms. Reproduce tiny negative
     * take-up pulses, then verify real sustained reverse still trips. */
    switch_boot(0);switch_delta_left=1;switch_delta_right=-2;switch_wait(25,1);
    CHECK(vision_drive_is_local()&&!info().faults);
    switch_delta_left=-15;switch_delta_right=15;switch_wait(100,1);
    CHECK(vision_drive_is_local()&&!info().faults);
    /* Recorded C26_16 launch faults: +3 left / -3 right caused a reverse
     * latch after 30ms. A short low-count transient may recover normally. */
    for(unsigned burst=0;burst<3;++burst){
        switch_delta_left=3;switch_delta_right=-3;switch_wait(8,1);
        CHECK(vision_drive_is_local()&&!info().faults);
        switch_delta_left=-15;switch_delta_right=15;switch_wait(15,1);
        CHECK(vision_drive_is_local()&&!info().faults);
    }
    switch_delta_left=8;switch_delta_right=-8;switch_wait(40,1);
    CHECK(!vision_drive_is_local()&&info().fault_detail==VS_FAULT_REVERSE_RIGHT&&!pwm[0]&&!pwm[1]);
    vision_drive_last_stop(&after);CHECK(after.speed_detail==VS_FAULT_REVERSE_RIGHT&&after.raw4<0&&after.pwm_right>0);
    switch_boot(0);switch_wait(100,1);switch_delta_left=1;switch_delta_right=-1;switch_wait(150,1);
    CHECK(info().fault_detail==VS_FAULT_REVERSE_RIGHT&&!pwm[0]&&!pwm[1]);
    printf("C26 LEVEL PASS: %u checks; zero-output loss, five-frame recovery, retained stop reason; simulated IO.\n",checks);
    return 0;
}
