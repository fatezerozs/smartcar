/* Actual C26 scheduler, PI instances and final writer, simulated IO only. */
#define pwm_set_duty fixture_pwm_set_duty
#include "../support/fixtures/switch_io.h"
#undef pwm_set_duty
#include "vision_drive_internal.h"
#include "vision_planner_runtime.h"
static unsigned inject,stage,nonzero_writes;
void pwm_set_duty(int channel,uint32_t duty)
{CHECK(mask==1);if(channel<2&&duty)++nonzero_writes;fixture_pwm_set_duty(channel,duty);}
void vision_test_output_hook(unsigned at)
{
    if(inject&&stage==at){unsigned action=inject;inject=0;
        if(action==1)vision_drive_stop();
        else if(action==2){app_mode_sample(1,1,now);vision_drive_safety_tick();}
        else if(action==3){now+=10;raw3=(uint16_t)(raw3-5);raw4=(uint16_t)(raw4+10);vd_speed_sample_tick(now);}
    }
}
static vdr_config_t config(void)
{
    vdr_config_t c;memset(&c,0,sizeof(c));c.planning.planner.revision=1;
    c.planning.planner.limits=(sp_limits_t){SP_SIMULATION,2,1,2,1,2,4,6,.02f,.02f,.0001f,.05f};
    c.planning.planner.ticks_per_metre_left=1000;c.planning.planner.ticks_per_metre_right=2000;
    c.planning.planner.max_left_ticks_10ms=c.planning.planner.max_right_ticks_10ms=100;
    c.planning.planner.frame_max_age_ms=100;c.planning.planner.encoder_max_age_ms=30;c.planning.planner.candidate_max_age_ms=20;
    c.planning.policy=(sp_policy_t){100,30,300,200,1000,3,3,3,1,.01f};
    c.odometry=(vm_odom_config_t){1,30,20,1000,2000,-1,1};
    c.vision.homography[0]=.01f;c.vision.homography[2]=-.93f;c.vision.homography[4]=-.01f;
    c.vision.homography[5]=1.2f;c.vision.homography[8]=1;
    c.vision.min_x_m=-1;c.vision.max_x_m=1;c.vision.min_y_m=0;c.vision.max_y_m=1;
    c.vision.max_step_m=.5f;c.vision.near_count=c.vision.full_count=5;c.vision.min_confidence=50;
    c.vision.revision=1;c.vision.max_age_ms=100;
    for(unsigned j=0;j<5;++j){c.vision.near_rows[j]=(uint8_t)(110-j*10);c.vision.full_rows[j]=(uint8_t)(110-j*15);}
    c.requested_speed_m_s=.5f;c.pwm_cap=200;c.feedforward_left=100;c.feedforward_right=110;return c;
}
static vdr_info_t runtime(void){vdr_info_t s;vision_drive_planner_info(&s);return s;}
static void zero(void)
{CHECK(!pwm[0]&&!pwm[1]&&!info().pwm_left&&!info().pwm_right&&!info().target_left&&!info().target_right);}
static void boot(vdr_mode_t mode)
{
    inject=0;switch_boot(1);switch_wait(10,1);switch_delta_left=-5;switch_delta_right=10;
    vdr_config_t c=config();CHECK(vision_drive_planner_configure(mode,&c));
    CHECK(vd_speed_source()==(mode==VDR_CONTROL?VD_SPEED_METRIC:VD_SPEED_LEGACY));
    switch_a=0;switch_wait(40,1);CHECK(vision_drive_is_local());
}
static void shadow_near_loss_is_diagnostic(void)
{
    boot(VDR_SHADOW);uint32_t epoch=vd.output_epoch;
    route_lane.row_flags[100]=1; /* Near-only configured row; full rows remain real. */
    switch_wait(4,1);
    CHECK(!runtime().near_valid&&runtime().far_valid);
    CHECK(runtime().supervisor_mode==SP_RECOVERY_NO_LANE);
    CHECK(vd.output_epoch==epoch&&vd.output_authorized&&!vd.guard_faults);
    CHECK(vd_speed_source()==VD_SPEED_LEGACY&&pwm[0]&&pwm[1]);
    set_lane(0);switch_wait(15,1);
    CHECK(runtime().supervisor_mode==SP_NORMAL_TRACK&&runtime().result_valid);
    vision_drive_stop();zero();
}
#if VISION_PLANNER_ACTIVE
static void source_and_writer(void)
{
    boot(VDR_CONTROL);CHECK(pwm[0]&&pwm[1]);
    CHECK(info().target_left==5&&info().target_right==10);
    CHECK(info().pwm_left<=200&&info().pwm_right<=200&&pwm[0]<=2000&&pwm[1]<=2000);
    CHECK(!vd_speed_select_source(VD_SPEED_LEGACY));
    CHECK(!vision_drive_planner_configure(VDR_OFF,NULL));
    uint16_t r=123,l=123;CHECK(!vd_speed_output(100,100,0,180,&r,&l));CHECK(!r&&!l);
    /* Running the public poll repeatedly must hold output and never advance PI. */
    vd_speed_info_t before=info();uint32_t plans=runtime().plans;
    for(unsigned j=0;j<100;++j)vision_drive_poll(now);
    CHECK(runtime().plans==plans&&info().pwm_left==before.pwm_left&&info().pwm_right==before.pwm_right);
    /* A change to legacy visual motor demand/mix must not change direct targets. */
    vd.control.output.duty=.01f;vd.control.output.steering=.8f;vd_output_apply();
    CHECK(info().target_left==5&&info().target_right==10);
    vision_drive_stop();zero();CHECK(!vd.output_authorized);
    vd.control.output.state=VC_RUNNING;vd.control.output.observation_valid=1;vd_output_apply();zero();
    switch_a=1;switch_wait(10,1);CHECK(vision_drive_planner_configure(VDR_OFF,NULL));
    CHECK(vd_speed_source()==VD_SPEED_LEGACY);
    switch_a=0;switch_wait(40,1);CHECK(vision_drive_is_local()&&info().target_left>5);
    boot(VDR_SHADOW);CHECK(vd_speed_source()==VD_SPEED_LEGACY&&runtime().plans>0&&pwm[0]&&pwm[1]);
}
static void faults_and_recovery(void)
{
    boot(VDR_CONTROL);uint32_t epoch=vd.output_epoch;
    route_lane.row_flags[110]=0;switch_wait(4,1);zero();
    CHECK(vd.output_epoch==epoch); /* Brief near loss inhibits without revoking. */
    set_lane(0);switch_wait(15,1);CHECK(pwm[0]&&pwm[1]);
    route_lane.row_flags[50]=0;switch_wait(40,1);
    CHECK(runtime().supervisor_mode==SP_CONTROLLED_STOP);
    switch_delta_left=switch_delta_right=0;switch_wait(6,1);
    CHECK(!vision_drive_is_local());zero();CHECK(!vd.output_authorized);
    set_lane(0);switch_wait(30,1);zero(); /* Held ON cannot restart. */
    switch_delta_left=-5;switch_delta_right=10;
    switch_a=1;switch_wait(10,1);switch_a=0;switch_wait(40,1);CHECK(pwm[0]&&pwm[1]);
    vision_drive_trip(VC_FAULT_ENCODER);zero();switch_a=1;switch_wait(10,1);switch_a=0;switch_wait(40,1);
    CHECK(vd.guard_faults&VC_FAULT_ENCODER);zero();
}
static void interruptions(void)
{
    for(unsigned event=1;event<=2;++event)for(unsigned at=1;at<=4;++at){
        boot(VDR_CONTROL);stage=at;inject=event;
        unsigned before=nonzero_writes;
        if(at==2||at==3){
            now+=10;raw3=(uint16_t)(raw3-5);raw4=(uint16_t)(raw4+10);vd_speed_sample_tick(now);
            vd_speed_control_poll(now);
        }else vd_output_apply();
        CHECK(!inject&&!mask);zero();CHECK(nonzero_writes==before);
        vd_output_apply();zero();
    }
    boot(VDR_CONTROL);stage=2;inject=3;
    now+=10;raw3=(uint16_t)(raw3-5);raw4=(uint16_t)(raw4+10);vd_speed_sample_tick(now);
    vd_speed_info_t before=info();vd_speed_control_poll(now);
    CHECK(!inject&&info().samples==before.samples+1&&info().target_left==before.target_left);
    vision_drive_stop();zero();
}
static void configuration_and_deadlines(void)
{
    switch_boot(1);switch_wait(10,1);vdr_config_t c=config(),bad=c;
    bad.vision.max_age_ms=0;CHECK(!vision_drive_planner_configure(VDR_CONTROL,&bad));
    bad=c;bad.odometry.revision=2;CHECK(!vision_drive_planner_configure(VDR_CONTROL,&bad));
    bad=c;bad.planning.planner.max_left_ticks_10ms=1000;CHECK(!vision_drive_planner_configure(VDR_CONTROL,&bad));
    bad=c;bad.pwm_cap=1001;CHECK(!vision_drive_planner_configure(VDR_CONTROL,&bad));
    CHECK(vd_speed_source()==VD_SPEED_LEGACY);
    /* Saturated feedforward must respect the independent final PWM cap. */
    c.feedforward_right=c.feedforward_left=800;
    CHECK(vision_drive_planner_configure(VDR_CONTROL,&c));
    switch_delta_left=-5;switch_delta_right=10;switch_a=0;switch_wait(40,1);
    CHECK(pwm[0]==2000&&pwm[1]==2000);
    vdr_output_t t;vd_planner_runtime_prepare(vd.output_epoch,&t);CHECK(t.valid&&t.valid_for_ms);
    uint16_t r=0,l=0;CHECK(vd_planner_runtime_output(&t,&r,&l)&&r&&l);
    /* Deadline may expire after unmasked preparation, before commit. */
    now+=t.valid_for_ms;CHECK(!vd_planner_runtime_output(&t,&r,&l)&&!r&&!l);
    boot(VDR_CONTROL);vd_planner_runtime_prepare(vd.output_epoch,&t);CHECK(t.valid);
    now+=10;raw3=(uint16_t)(raw3-5);raw4=(uint16_t)(raw4+10);vd_speed_sample_tick(now);
    CHECK(!vd_planner_runtime_output(&t,&r,&l)&&!r&&!l);
    boot(VDR_CONTROL);vd_planner_runtime_prepare(vd.output_epoch,&t);CHECK(t.valid);
    vision_drive_stop();CHECK(!vd_planner_runtime_output(&t,&r,&l)&&!r&&!l);zero();
    boot(VDR_CONTROL);stage=1;inject=3;unsigned before=nonzero_writes;
    vd_output_apply();CHECK(!inject&&nonzero_writes==before);zero();
    vision_drive_poll(now); /* Consume the injected sample before the next ISR. */
    switch_wait(4,1);CHECK(pwm[0]&&pwm[1]); /* New feedback resumes normally. */
}
static void visual_fault_classification(void)
{
    switch_boot(1);switch_wait(10,1);vdr_config_t c=config();
    c.planning.planner.frame_max_age_ms=c.planning.policy.frame_timeout_ms=50;c.vision.max_age_ms=50;
    c.odometry.interpolation_gap_ms=5; /* Exact captures work, midpoint spans are rejected. */
    CHECK(vision_drive_planner_configure(VDR_CONTROL,&c));
    switch_delta_left=-5;switch_delta_right=10;switch_a=0;switch_wait(40,1);CHECK(pwm[0]&&pwm[1]);
    uint32_t epoch=vd.output_epoch;
    vd_planner_runtime_observe(&route_lane,++route_sequence,now-5,now);vision_drive_poll(now);vd_output_apply();
    CHECK(!vd.guard_faults&&runtime().session_active&&vd.output_epoch==epoch&&pwm[0]&&pwm[1]);
    switch_wait(6,0);CHECK(vd.guard_faults&VC_FAULT_FRAME);
    CHECK(!(vd.guard_faults&(VC_FAULT_CONFIG|VC_FAULT_RUNTIME)));zero();
    switch_wait(20,1);zero(); /* Fresh frames cannot clear a held-ON fault. */
    switch_a=1;switch_wait(10,1);CHECK(!vd.guard_faults);
    switch_a=0;switch_wait(40,1);CHECK(pwm[0]&&pwm[1]);
}
#endif
int main(void)
{
    shadow_near_loss_is_diagnostic();
#if VISION_PLANNER_ACTIVE
    source_and_writer();faults_and_recovery();interruptions();configuration_and_deadlines();visual_fault_classification();
#else
    boot(VDR_SHADOW);CHECK(runtime().mode==VDR_SHADOW&&runtime().plans>0);
    CHECK(vd_speed_source()==VD_SPEED_LEGACY&&pwm[0]&&pwm[1]);
    switch_a=1;switch_wait(10,1);vdr_config_t c=config();
    CHECK(!vision_drive_planner_configure(VDR_CONTROL,&c));
    CHECK(!vd_speed_select_source(VD_SPEED_METRIC));
    CHECK(vision_drive_planner_configure(VDR_OFF,NULL));vision_drive_stop();zero();
#endif
    printf("PLANNER RUNTIME PASS %u checks; actual C26/PI/writer, simulated hardware.\n",checks);return 0;
}
