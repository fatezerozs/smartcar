/* Historical suite id retained; current C26 admission/fault boundary regression.
 * This is not calibrated speed-planner or hardware acceptance. */
#include "../support/fixtures/switch_io.h"
#include "vision_drive_internal.h"
static void snapshot(const char *event)
{
    vd_speed_info_t s=info();
    printf("C26,%s,%lu,%u,%lu,%u,%d,%d,%u,%u,%lu,%lu,%lu\n",event,(unsigned long)now,
        (unsigned)vd.control.output.state,(unsigned long)vd.output_epoch,(unsigned)vd.output_authorized,
        s.target_left,s.target_right,s.pwm_left,s.pwm_right,(unsigned long)pwm[0],
        (unsigned long)pwm[1],(unsigned long)vd.control.output.faults);
}
static void motors_zero(void)
{
    vd_speed_info_t s=info();
    CHECK(!s.target_left&&!s.target_right&&!s.pwm_left&&!s.pwm_right);
    CHECK(!pwm[0]&&!pwm[1]);
}
static void fault_classes(void)
{
    const uint32_t severe[]={VC_FAULT_CONFIG,VC_FAULT_COMMAND,VC_FAULT_NUMERIC,VC_FAULT_OFFSET,
        VC_FAULT_RUNTIME,VC_FAULT_EXTERNAL,VC_FAULT_ENCODER,UINT32_C(1)<<20};
    for(unsigned i=0;i<sizeof(severe)/sizeof(severe[0]);++i)for(unsigned mixed=0;mixed<2;++mixed){
        uint32_t faults=severe[i]|(mixed?VC_FAULT_VISION|VC_FAULT_FRAME:0);
        switch_boot(0);switch_wait(30,1);CHECK(vision_drive_is_local());
        vd.guard_faults|=faults;vc_trip(&vd.control,faults);vision_drive_poll(now);
        motors_zero();switch_wait(30,1);CHECK(!vision_drive_is_local());
        switch_a=1;switch_wait(10,1);motors_zero();
        CHECK((vd.control.output.faults&faults)==faults);
        CHECK((vd.guard_faults&faults)==faults);
        switch_a=0;switch_wait(30,1);motors_zero();CHECK(!vision_drive_is_local());
        CHECK((vd.control.output.faults&faults)==faults);
        CHECK(!vision_drive_command('a',now,1));
    }
    /* Internal encoder fault is independently latched in speed state. */
    switch_boot(0);switch_wait(30,1);switch_delta_left=8;switch_delta_right=-8;switch_wait(60,1);
    CHECK(info().faults&VC_FAULT_ENCODER);motors_zero();
    switch_a=1;switch_wait(10,1);CHECK(info().faults&VC_FAULT_ENCODER);
    switch_delta_left=-15;switch_delta_right=15;switch_a=0;switch_wait(30,1);
    CHECK(info().faults&VC_FAULT_ENCODER);CHECK(!vision_drive_is_local());motors_zero();
}
static void partial_and_final_veto(void)
{
    uint32_t epoch;
    switch_boot(0);switch_wait(30,1);CHECK(vision_drive_is_local());
    epoch=vd.output_epoch;
    route_lane.valid=0;route_lane.status=VISION_NO_LANE;route_lane.confidence=11;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(y<70||y>79)route_lane.row_flags[y]=0;
    for(unsigned i=0;i<20;++i){switch_step(1);if(vd.control.output.recovering)motors_zero();}
    CHECK(!vd.control.output.observation_valid&&vd.control.output.recovering);
    CHECK(vd.control.output.search_source==2&&vd.control.output.duty==0);
    CHECK(vd.output_epoch==epoch&&vd.output_authorized);motors_zero();
    /* Bypass the controller's zero-demand branch with a bounded nonzero
     * demand: the output facade/final writer must independently veto it. */
    vd.control.output.duty=.10f;vd_output_apply();motors_zero();
    for(unsigned i=0;i<5;++i){switch_step(0);vd.control.output.duty=.10f;vd_output_apply();motors_zero();}
    CHECK(vd.output_epoch==epoch&&vd.output_authorized);
    set_lane(0);switch_wait(30,1);CHECK(vision_drive_is_local()&&vd.control.output.observation_valid);
    CHECK(info().target_left&&info().target_right); /* Same lease, no auto-arm. */
    epoch=vd.output_epoch;vision_drive_stop();CHECK(vd.output_epoch!=epoch);motors_zero();
    vd.control.output.state=VC_RUNNING;vd.control.output.observation_valid=1;vd.control.output.duty=.10f;
    vd_output_apply();CHECK(!vd.output_authorized);motors_zero(); /* Revoked authority cannot be recreated. */
}
static void fresh_admission(void)
{
    uint32_t sequence,capture;
    switch_boot(0);switch_wait(30,1);CHECK(vision_drive_is_local());
    sequence=vd.control.last_sequence;capture=vd.control.last_frame_ms;
    vc_trip(&vd.control,VC_FAULT_VISION|VC_FAULT_FRAME);vision_drive_poll(now);motors_zero();
    switch_a=1;switch_wait(5,0);CHECK(vd.control.output.state==VC_DISARMED);
    switch_a=0;switch_wait(4,0);CHECK(!vision_drive_is_local());
    CHECK(vd.control.stable_frames==0);
    for(unsigned i=0;i<5;++i){
        vision_drive_observe(&route_lane,sequence-i,capture,now);
        CHECK(vd.control.stable_frames==0);CHECK(!vision_drive_is_local());motors_zero();
    }
    /* Fewer than the existing five valid frames cannot satisfy admission. */
    switch_wait(6,1);CHECK(!vision_drive_is_local());
    switch_wait(30,1);CHECK(vision_drive_is_local());
    /* A short OFF bounce never acknowledges a fault. */
    vc_trip(&vd.control,VC_FAULT_FRAME);vision_drive_poll(now);switch_a=1;switch_wait(1,1);
    switch_a=0;switch_wait(30,1);CHECK(!vision_drive_is_local());
    CHECK(vd.control.output.faults&VC_FAULT_FRAME);motors_zero();
    CHECK(vision_drive_command('c',now,1)); /* Explicit clear is not admission. */
    switch_wait(30,1);CHECK(!vision_drive_is_local());motors_zero();
    switch_a=1;switch_wait(10,1);switch_a=0;switch_wait(30,1);CHECK(vision_drive_is_local());
}
int main(void)
{
    uint32_t epoch;
    switch_boot(1);switch_wait(30,1);CHECK(!pwm[0]&&!pwm[1]);snapshot("boot_OFF");
    CHECK(!vision_drive_command('a',now,1));snapshot("UART_a_rejected");
    switch_b=0;switch_wait(10,1);switch_a=0;route_lane.valid=0;switch_wait(30,1);
    CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);snapshot("ON_without_lane");
    set_lane(0);switch_wait(30,1);CHECK(vision_drive_is_local());snapshot("first_ON_admitted");
    epoch=vd.output_epoch;switch_a=1;switch_step(1);CHECK(!pwm[0]&&!pwm[1]&&vd.output_epoch!=epoch);
    switch_wait(10,1);snapshot("physical_OFF_revokes");switch_a=0;switch_wait(30,1);
    CHECK(vision_drive_is_local());snapshot("second_ON_admitted");
    vision_drive_stop();switch_wait(30,1);CHECK(!vision_drive_is_local());snapshot("operator_STOP_inhibits");
    switch_a=1;switch_wait(10,1);switch_a=0;switch_wait(30,1);CHECK(vision_drive_is_local());snapshot("STOP_OFF_ON_admitted");
    route_lane.valid=0;switch_wait(100,1);CHECK(!vision_drive_is_local());snapshot("vision_fault");
    set_lane(0);switch_wait(30,1);CHECK(!vision_drive_is_local());snapshot("vision_recovery_ON_stays_stopped");
    switch_a=1;switch_wait(10,1);switch_a=0;switch_wait(30,1);
    CHECK(vision_drive_is_local());snapshot("vision_OFF_ON_readmitted");
    vision_drive_trip(VC_FAULT_ENCODER);switch_wait(30,1);CHECK(!vision_drive_is_local());snapshot("encoder_fault_ON_stays_stopped");
    switch_a=1;switch_wait(10,1);snapshot("stable_OFF_preserves_severe");
    CHECK(vd.control.output.faults&VC_FAULT_ENCODER);
    switch_a=0;switch_wait(30,1);CHECK(!vision_drive_is_local());snapshot("severe_OFF_ON_stays_stopped");
    fault_classes();partial_and_final_veto();fresh_admission();
    puts("C26 BOUNDARY REGRESSION PASS; admission/fault policy only, not complete v1.4 acceptance.");
    return 0;
}
