#include "vision_planner_shadow.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"shadow:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static vps_config_t config(void)
{
    vps_config_t c;memset(&c,0,sizeof(c));c.planner.revision=1;
    c.planner.limits=(sp_limits_t){SP_SIMULATION,3,1,2,1,2,4,6,.02f,.02f,.0001f,.05f};
    c.planner.ticks_per_metre_left=11000;c.planner.ticks_per_metre_right=12000;
    c.planner.max_left_ticks_10ms=c.planner.max_right_ticks_10ms=1000;
    c.planner.frame_max_age_ms=100;c.planner.encoder_max_age_ms=30;c.planner.candidate_max_age_ms=20;
    c.policy=(sp_policy_t){100,30,300,60,1000,3,3,3,1,.01f};return c;
}
static void sync(vps_input_t *in)
{
    sp_input_t *i=&in->observation;
    in->identity.frame_seq=i->frame_seq;in->identity.capture_ms=i->capture_ms;
    in->identity.encoder_seq=i->encoder_seq;in->identity.encoder_ms=i->encoder_ms;
    in->near_preview.sequence=in->full_preview.sequence=i->frame_seq;
    in->near_preview.capture_ms=in->full_preview.capture_ms=i->capture_ms;
}
static vps_input_t input(void)
{
    vps_input_t in;memset(&in,0,sizeof(in));in.valid=VP_VALID_ALL;in.external_admitted=1;
    in.identity.output_epoch=9;in.identity.config_revision=1;
    in.requested_speed_m_s=1;
    in.observation.new_frame=in.observation.near_valid=in.observation.far_valid=1;
    in.observation.encoder_valid=in.observation.capture_odometer_valid=1;
    in.observation.measured_speed=.5f;in.observation.now_ms=100;
    in.observation.capture_ms=in.observation.encoder_ms=100;
    in.near_preview.units=in.full_preview.units=SP_METRES;
    in.near_preview.count=in.full_preview.count=SP_CAPACITY;
    for(unsigned j=0;j<SP_CAPACITY;++j){in.near_preview.distance[j]=(float)j*.15f;in.full_preview.distance[j]=(float)j*.25f;}
    sync(&in);return in;
}
static void step(vps_input_t *in,int near_ok,int far_ok)
{
    sp_input_t *i=&in->observation;i->now_ms+=10;i->capture_ms=i->encoder_ms=i->now_ms;
    ++i->frame_seq;++i->encoder_seq;i->new_frame=1;i->near_valid=(uint8_t)near_ok;i->far_valid=(uint8_t)far_ok;
    i->odometer_m+=.005f;i->odometer_at_capture_m=i->odometer_m;sync(in);
}
static void arm(vps_state_t *s,vps_input_t *in,vps_config_t c)
{*in=input();CHECK(vps_init(s,VP_SHADOW,&c));CHECK(vps_arm(s,in));}
static void zero(vps_result_t r)
{CHECK(!r.candidate.left_ticks_s&&!r.candidate.right_ticks_s&&!r.candidate.left_ticks_10ms&&!r.candidate.right_ticks_10ms);}
static void admission(void)
{
    vps_state_t s;vps_config_t c=config();vps_input_t in=input();vps_result_t r;
    CHECK(!vps_init(&s,VP_DISABLED,&c));CHECK(vps_tick(&s,&in,&r)==VPS_INACTIVE);zero(r);
    c.policy.far_frames=0;CHECK(!vps_init(&s,VP_SHADOW,&c));c=config();
    CHECK(vps_init(&s,VP_SHADOW,&c));in.external_admitted=0;CHECK(!vps_arm(&s,&in));
    in.external_admitted=1;in.observation.far_valid=0;CHECK(!vps_arm(&s,&in));
    step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);CHECK(s.supervisor.mode==SP_DISARMED);zero(r);
    CHECK(vps_arm(&s,&in));CHECK(!vps_arm(&s,&in));
    step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_READY);
    CHECK(vps_result_matches(&s,&r,&in.identity,in.observation.now_ms));
    in.external_admitted=0;step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);
    CHECK(s.supervisor.mode==SP_STOP_LATCHED);in.external_admitted=1;CHECK(!vps_arm(&s,&in));
    step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);zero(r);
    arm(&s,&in,c);in.identity.output_epoch++;step(&in,1,1);
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO&&s.supervisor.mode==SP_STOP_LATCHED);
    arm(&s,&in,c);step(&in,1,1);in.identity.config_revision++;
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);CHECK(!vps_arm(&s,&in));zero(r);
    arm(&s,&in,c);CHECK(vps_tick(&s,NULL,&r)==VPS_FAULT);zero(r);
    CHECK(vps_tick(NULL,NULL,&r)==VPS_INACTIVE);CHECK(vps_tick(NULL,NULL,NULL)==VPS_FAULT);
}
static void state_and_samples(void)
{
    vps_state_t s;vps_input_t in;vps_result_t r,previous;vps_config_t c=config();arm(&s,&in,c);
    step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_READY);previous=r;
    CHECK(s.trajectory.speed>.5f&&s.control_revision==r.candidate.stamp.control_revision);
    CHECK(vps_tick(&s,&in,&r)==VPS_WAIT);zero(r);
    CHECK(!vps_result_matches(&s,&previous,&in.identity,in.observation.now_ms));
    float held=s.trajectory.speed;step(&in,1,1);in.observation.new_frame=0;
    in.observation.frame_seq=s.supervisor.last_frame_seq;in.observation.capture_ms=s.supervisor.last_capture_ms;sync(&in);
    CHECK(vps_tick(&s,&in,&r)==VPS_READY&&s.trajectory.speed>held);
    CHECK(r.candidate.stamp.frame_seq==s.supervisor.last_frame_seq);
    /* Encoder gaps cannot replay one measurement as two control steps. */
    step(&in,1,1);++in.observation.encoder_seq;sync(&in);
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
    arm(&s,&in,c);step(&in,1,1);in.observation.encoder_seq=0;sync(&in);
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
    arm(&s,&in,c);step(&in,1,1);in.valid&=~VP_VALID_ENCODER;
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
    arm(&s,&in,c);step(&in,1,1);in.identity.frame_seq++;
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
    /* Both sequence and time wraps are valid forward progress. */
    in=input();in.observation.frame_seq=in.observation.encoder_seq=UINT32_MAX;
    in.observation.now_ms=in.observation.capture_ms=in.observation.encoder_ms=UINT32_MAX-5;sync(&in);
    CHECK(vps_init(&s,VP_SHADOW,&c)&&vps_arm(&s,&in));step(&in,1,1);
    CHECK(vps_tick(&s,&in,&r)==VPS_READY&&r.candidate.stamp.encoder_seq==0);
}
static void degradation(void)
{
    vps_state_t s;vps_input_t in;vps_result_t r;vps_config_t c=config();arm(&s,&in,c);
    step(&in,1,0);CHECK(vps_tick(&s,&in,&r)==VPS_READY);CHECK(s.supervisor.far_active);
    CHECK(r.candidate.cap_m_s<=c.planner.limits.near_cap);
    vp_stamp_t id=in.identity;id.request_revision++;CHECK(!vps_result_matches(&s,&r,&id,in.observation.now_ms));
    /* Keep a candidate just before far deadline; expiry outranks publication. */
    for(unsigned j=0;j<5;++j){step(&in,1,0);CHECK(vps_tick(&s,&in,&r)==VPS_READY);}
    CHECK(vps_result_matches(&s,&r,&in.identity,in.observation.now_ms));
    CHECK(!vps_result_matches(&s,&r,&in.identity,in.observation.now_ms+10));
    step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_READY);
    CHECK(s.supervisor.mode==SP_CONTROLLED_STOP&&r.candidate.action==VP_CONTROLLED_STOP);
    CHECK(s.trajectory.speed>0); /* No immediate zero disguised as controlled stop. */
    for(unsigned j=0;j<3;++j){step(&in,1,1);in.observation.measured_speed=0;(void)vps_tick(&s,&in,&r);}
    CHECK(s.supervisor.mode==SP_STOP_LATCHED);zero(r);CHECK(!vps_arm(&s,&in));
    /* Near recovery retains far budget and reseeds from actual measured speed. */
    c.policy.far_budget_ms=500;arm(&s,&in,c);step(&in,1,0);CHECK(vps_tick(&s,&in,&r)==VPS_READY);
    uint32_t start=s.supervisor.far_started_ms;
    step(&in,0,0);CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);CHECK(s.supervisor.motor_inhibit);zero(r);
    for(unsigned j=0;j<3;++j){step(&in,1,0);(void)vps_tick(&s,&in,&r);}
    CHECK(r.status==VPS_READY&&s.supervisor.far_started_ms==start);CHECK(s.trajectory.speed>=.5f);
    /* Time budgets continue without a new image publication. */
    step(&in,1,0);in.observation.new_frame=0;sync(&in);CHECK(vps_tick(&s,&in,&r)==VPS_READY);
    c=config();c.policy.far_distance_m=.01f;arm(&s,&in,c);step(&in,1,0);
    in.observation.odometer_at_capture_m=0;in.observation.odometer_m=.02f;
    CHECK(vps_tick(&s,&in,&r)==VPS_READY&&s.supervisor.mode==SP_CONTROLLED_STOP);
}
static void geometry_and_deadlines(void)
{
    vps_state_t s;vps_input_t in;vps_result_t r;vps_config_t c=config();arm(&s,&in,c);
    step(&in,1,1);in.observation.capture_ms=100;in.observation.odometer_at_capture_m=0;sync(&in);
    /* Duplicate frame is ignored; the cached horizon still advances by odometry. */
    in.observation.frame_seq=0;sync(&in);CHECK(vps_tick(&s,&in,&r)==VPS_READY);
    CHECK(s.supervisor.last_capture_ms==100);
    sp_preview_t rebased;CHECK(sp_preview_advance(&in.full_preview,.5f,&rebased));
    CHECK(fabsf(rebased.distance[rebased.count-1]-3.25f)<1e-6f);
    CHECK(!sp_preview_advance(&in.full_preview,3.75f,&rebased));
    step(&in,1,1);in.observation.new_frame=0;in.observation.odometer_m=10;sync(&in);
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
    arm(&s,&in,c);step(&in,1,1);in.near_preview.units=SP_NORMALIZED;
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO&&s.supervisor.motor_inhibit);zero(r);
    arm(&s,&in,c);step(&in,1,0);in.observation.capture_odometer_valid=0;
    CHECK(vps_tick(&s,&in,&r)!=VPS_READY);zero(r);
    arm(&s,&in,c);step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_READY);
    CHECK(!vps_result_matches(&s,&r,&in.identity,in.observation.now_ms+20));
    in.observation.now_ms+=100;CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
    arm(&s,&in,c);step(&in,1,1);in.requested_speed_m_s=NAN;
    CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);zero(r);
}
static void independent_timeouts(void)
{
    vps_state_t s;vps_input_t in;vps_result_t r;vps_config_t c=config();
    c.policy.near_window_ms=20;arm(&s,&in,c);step(&in,0,0);
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);
    for(unsigned j=0;j<2;++j){step(&in,1,1);(void)vps_tick(&s,&in,&r);}
    CHECK(s.supervisor.mode==SP_FAULT_LATCHED&&(s.supervisor.faults&SP_FAULT_NEAR));zero(r);
    c=config();c.policy.far_budget_ms=10;c.policy.stop_timeout_ms=30;arm(&s,&in,c);
    for(unsigned j=0;j<5;++j){step(&in,1,0);(void)vps_tick(&s,&in,&r);}
    CHECK(s.supervisor.mode==SP_FAULT_LATCHED&&(s.supervisor.faults&SP_FAULT_CONTROL));zero(r);
    c=config();c.policy.far_budget_ms=1000;arm(&s,&in,c);
    for(unsigned j=0;j<10;++j){
        step(&in,1,1);in.observation.new_frame=0;
        in.observation.frame_seq=0;in.observation.capture_ms=100;sync(&in);
        (void)vps_tick(&s,&in,&r);
    }
    CHECK(s.supervisor.mode==SP_FAULT_LATCHED&&(s.supervisor.faults&SP_FAULT_FRAME));zero(r);
    /* Captured origin is required even for a normal full preview, not only
     * the initial far-loss budget calculation in the supervisor. */
    arm(&s,&in,c);in.observation.capture_odometer_valid=0;CHECK(!vps_arm(&s,&in));
    step(&in,1,1);CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);zero(r);
}
static void control_period_boundary(void)
{
    vps_state_t s;vps_input_t in;vps_result_t r;vps_config_t c=config();
    /* The measured-data candidate permits an integer 20ms sample gap.
     * Exercise the complete supervisor/planner path at that exact boundary. */
    c.planner.limits.max_dt_s=.02f;
    const unsigned periods[]={1,10,19,20,21};
    for(unsigned j=0;j<sizeof(periods)/sizeof(periods[0]);++j){
        arm(&s,&in,c);step(&in,1,1);
        in.observation.now_ms=100+periods[j];
        in.observation.capture_ms=in.observation.encoder_ms=in.observation.now_ms;sync(&in);
        if(periods[j]<=20){
            CHECK(vps_tick(&s,&in,&r)==VPS_READY);
            CHECK(vps_result_matches(&s,&r,&in.identity,in.observation.now_ms));
            CHECK(s.supervisor.faults==0);
        }else{
            CHECK(vps_tick(&s,&in,&r)==VPS_FAULT);
            CHECK(s.supervisor.faults&SP_FAULT_CONTROL);zero(r);
        }
    }
}
static void independent_full_evidence(void)
{
    vps_state_t s;vps_input_t in;vps_result_t r;vps_config_t c=config();arm(&s,&in,c);
    step(&in,0,1);in.valid=VP_VALID_ENCODER;
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO);zero(r);
    CHECK(!s.have_near&&s.have_full&&s.full_preview.sequence==in.observation.frame_seq);
    CHECK(s.supervisor.authorized&&s.supervisor.motor_inhibit&&!s.supervisor.faults);
    CHECK(!s.supervisor.far_active); /* No second far-loss penalty for near loss. */
    uint32_t retained=s.full_preview.sequence;
    step(&in,0,1);in.observation.new_frame=0;
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO&&s.full_preview.sequence==retained);
    in.valid=VP_VALID_ALL;
    for(unsigned j=0;j<c.policy.near_frames;++j){step(&in,1,1);(void)vps_tick(&s,&in,&r);}
    CHECK(r.status==VPS_READY&&!s.supervisor.motor_inhibit&&!s.supervisor.faults);
    CHECK(s.trajectory.speed<=in.observation.measured_speed+c.planner.limits.accel*.01f);
    step(&in,0,1);in.full_preview.curvature[0]=NAN;
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO&&!s.have_full);zero(r);
    CHECK(s.supervisor.authorized&&!s.supervisor.faults); /* Isolate bad candidate. */
    step(&in,0,1);in.observation.hard_stop=1;
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO&&!s.supervisor.authorized);zero(r);
    step(&in,1,1);in.observation.hard_stop=0;in.full_preview.curvature[0]=0;
    CHECK(vps_tick(&s,&in,&r)==VPS_ZERO&&!s.supervisor.authorized);zero(r);
}
int main(void)
{
    admission();state_and_samples();degradation();geometry_and_deadlines();independent_timeouts();control_period_boundary();independent_full_evidence();
    printf("PLANNER SHADOW PASS: %u checks; no PI, motor or real-time claim.\n",checks);return 0;
}
