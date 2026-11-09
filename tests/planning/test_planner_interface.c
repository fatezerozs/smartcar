/* Synthetic metric inputs exercise the exact target interface and shared
 * kernel. They are not vehicle calibration or physical stopping evidence. */
#include "vision_planner.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"interface:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static vp_config_t config(void)
{
    vp_config_t c;memset(&c,0,sizeof(c));c.revision=7;
    c.limits=(sp_limits_t){SP_SIMULATION,3,1,2,1,2,4,6,.02f,.02f,.0001f,.05f};
    c.ticks_per_metre_left=11000;c.ticks_per_metre_right=12000;c.rear_track_m=.15f;
    c.max_left_ticks_10ms=c.max_right_ticks_10ms=1000;
    c.frame_max_age_ms=100;c.encoder_max_age_ms=30;c.candidate_max_age_ms=20;
    return c;
}
static vp_input_t input(void)
{
    vp_input_t in;memset(&in,0,sizeof(in));
    in.stamp=(vp_stamp_t){9,5,7,UINT32_MAX,UINT32_MAX,100,110,3};
    in.now_ms=110;in.valid=VP_VALID_ALL;in.action=VP_TRACK;
    in.preview.units=SP_METRES;in.preview.count=SP_CAPACITY;
    in.preview.sequence=in.stamp.frame_seq;in.preview.capture_ms=in.stamp.capture_ms;
    for(unsigned j=0;j<SP_CAPACITY;++j)in.preview.distance[j]=(float)j*.25f;
    in.dt_s=.01f;in.requested_speed_m_s=1;in.measured_speed_m_s=1;in.trajectory.speed=1;
    return in;
}
static void zero(vp_candidate_t c)
{
    CHECK(c.action==VP_ZERO_NOW);CHECK(!c.trajectory.speed&&!c.trajectory.accel);
    CHECK(!c.left_ticks_s&&!c.right_ticks_s&&!c.left_ticks_10ms&&!c.right_ticks_10ms&&!c.cap_m_s);
}
static void expect(vp_mode_t m,const vp_config_t *c,const vp_input_t *in,vp_status_t status)
{
    vp_candidate_t out;memset(&out,0x55,sizeof(out));CHECK(vp_prepare(m,c,in,&out)==status);
    CHECK(out.status==status);zero(out);CHECK(!vp_candidate_matches(m,&out,in?&in->stamp:NULL,110));
}
static void gates(void)
{
    vp_config_t c=config();vp_input_t in=input();vp_candidate_t out;
    expect(VP_DISABLED,&c,&in,VP_OFF);expect((vp_mode_t)2,&c,&in,VP_OFF);
    expect(VP_SHADOW,NULL,&in,VP_BAD_CONFIG);expect(VP_SHADOW,&c,NULL,VP_BAD_INPUT);
    CHECK(vp_prepare(VP_SHADOW,&c,&in,NULL)==VP_BAD_INPUT);
    in.action=VP_ZERO_NOW;in.measured_speed_m_s=NAN;in.dt_s=NAN;
    expect(VP_DISABLED,NULL,&in,VP_ZERO);expect(VP_SHADOW,NULL,&in,VP_ZERO);
    for(unsigned bits=0;bits<VP_VALID_ALL;++bits){in=input();in.valid=bits;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);}
    in=input();in.valid=VP_VALID_ALL|16;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.action=(vp_action_t)3;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.near_only=2;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.stamp.config_revision=6;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);
    in=input();c.revision=0;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.limits.provenance=(sp_provenance_t)2;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.candidate_max_age_ms=31;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.frame_max_age_ms=UINT32_C(0x80000000);expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.encoder_max_age_ms=0;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.compensate=1;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c.geometry_calibrated=1;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    c.rear_track_m=NAN;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.ticks_per_metre_left=INFINITY;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);c=config();
    c.max_right_ticks_10ms=1001;expect(VP_SHADOW,&c,&in,VP_BAD_CONFIG);
}
static void data_validation(void)
{
    vp_config_t c=config();vp_input_t in=input();
    in.preview.units=SP_NORMALIZED;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview.sequence=0;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview.capture_ms++;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview.count=1;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview.count=SP_CAPACITY+1;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview.distance[2]=NAN;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview.curvature[0]=INFINITY;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.near_only=1;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.preview_scope=VP_NEAR_PREVIEW;expect(VP_SHADOW,&c,&in,VP_BAD_GEOMETRY);
    in=input();in.dt_s=0;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.dt_s=.051f;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.trajectory.accel=NAN;expect(VP_SHADOW,&c,&in,VP_INFEASIBLE);
    in=input();in.measured_speed_m_s=NAN;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.requested_speed_m_s=INFINITY;expect(VP_SHADOW,&c,&in,VP_BAD_INPUT);
    in=input();in.stamp.encoder_ms=in.now_ms+1;expect(VP_SHADOW,&c,&in,VP_EXPIRED);
    in=input();in.stamp.capture_ms=in.now_ms+1;expect(VP_SHADOW,&c,&in,VP_EXPIRED);
    in=input();in.now_ms=in.stamp.encoder_ms+c.encoder_max_age_ms;expect(VP_SHADOW,&c,&in,VP_EXPIRED);
}
static void quantization_and_cap(void)
{
    vp_config_t c=config();vp_input_t in=input();vp_candidate_t out;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(out.left_ticks_10ms==110&&out.right_ticks_10ms==120&&!out.quantized);
    CHECK(fabsf(out.left_ticks_s-11000)<.01f&&fabsf(out.right_ticks_s-12000)<.01f);
    c.ticks_per_metre_left=11394;c.ticks_per_metre_right=11389;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(out.left_ticks_10ms==113&&out.right_ticks_10ms==113&&out.quantized);
    in.trajectory.speed=in.measured_speed_m_s=in.requested_speed_m_s=.001f;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(!out.left_ticks_10ms&&!out.right_ticks_10ms&&out.left_ticks_s>0&&out.quantized);
    in=input();c.max_left_ticks_10ms=100;expect(VP_SHADOW,&c,&in,VP_TARGET_RANGE);c=config();
    c.compensate=c.geometry_calibrated=1;
    for(unsigned j=0;j<SP_CAPACITY;++j)in.preview.curvature[j]=.2f;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(out.left_ticks_s/c.ticks_per_metre_left>out.right_ticks_s/c.ticks_per_metre_right);
    in=input();in.trajectory.speed=in.requested_speed_m_s=.5f;
    in.near_only=1;in.preview_scope=VP_NEAR_PREVIEW;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY&&out.cap_m_s<=c.limits.near_cap);
    in=input();in.trajectory.speed=2;in.near_only=1;in.preview_scope=VP_NEAR_PREVIEW;
    expect(VP_SHADOW,&c,&in,VP_INFEASIBLE);
}
static void semantics_and_oracle(void)
{
    vp_config_t c=config();vp_input_t in=input();vp_candidate_t out;
    sp_plan_t plan;sp_trajectory_t trajectory=in.trajectory;
    CHECK(sp_plan(&in.preview,&c.limits,in.measured_speed_m_s,0,0,&plan));
    CHECK(sp_shape(&trajectory,&c.limits,in.requested_speed_m_s,plan.current_cap,in.dt_s,0));
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(out.trajectory.speed==trajectory.speed&&out.trajectory.accel==trajectory.accel&&out.cap_m_s==plan.current_cap);
    in.action=VP_CONTROLLED_STOP;in.requested_speed_m_s=2;
    float previous=1;int finished=0;
    for(unsigned j=0;j<200;++j){
        CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
        CHECK(out.action==VP_CONTROLLED_STOP&&out.trajectory.speed<=previous+1e-6f);
        CHECK(out.cap_m_s==plan.current_cap); /* Stopping cannot ignore geometry. */
        if(!j)CHECK(out.trajectory.speed>0); /* Controlled stop differs from immediate zero. */
        previous=out.trajectory.speed;in.trajectory=out.trajectory;
        if(out.trajectory.speed<1e-5f){finished=1;break;}
    }
    CHECK(finished);
    in.action=VP_ZERO_NOW;expect(VP_SHADOW,&c,&in,VP_ZERO);
    /* Truncated near horizon can make a requested controlled stop infeasible. */
    in=input();in.action=VP_CONTROLLED_STOP;
    for(unsigned j=0;j<SP_CAPACITY;++j)in.preview.distance[j]=(float)j*.001f;
    expect(VP_SHADOW,&c,&in,VP_INFEASIBLE);
}
static void quantization_boundaries(void)
{
    vp_config_t c=config();vp_input_t in=input();vp_candidate_t out;
    for(unsigned n=1;n<1000;++n){
        c.ticks_per_metre_left=nextafterf((float)n*100,-INFINITY);
        c.ticks_per_metre_right=nextafterf((float)n*100,INFINITY);
        CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
        CHECK(out.left_ticks_10ms==n-1&&out.right_ticks_10ms==n);
        CHECK((float)out.left_ticks_10ms*100<=out.left_ticks_s);
        CHECK((float)out.right_ticks_10ms*100<=out.right_ticks_s);
    }
}
static void identities_and_expiry(void)
{
    vp_config_t c=config();vp_input_t in=input();vp_candidate_t out;vp_stamp_t current;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(vp_candidate_matches(VP_SHADOW,&out,&in.stamp,110));
    CHECK(!vp_candidate_matches(VP_DISABLED,&out,&in.stamp,110));
    CHECK(!vp_candidate_matches(VP_SHADOW,NULL,&in.stamp,110));
    CHECK(!vp_candidate_matches(VP_SHADOW,&out,NULL,110));
    CHECK(!vp_candidate_matches(VP_SHADOW,&out,&in.stamp,109));
    CHECK(vp_candidate_matches(VP_SHADOW,&out,&in.stamp,129));
    CHECK(!vp_candidate_matches(VP_SHADOW,&out,&in.stamp,130));
    current=in.stamp;current.output_epoch++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.request_revision++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.config_revision++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.frame_seq++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.encoder_seq++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.capture_ms++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.encoder_ms++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    current=in.stamp;current.control_revision++;CHECK(!vp_candidate_matches(VP_SHADOW,&out,&current,110));
    c.frame_max_age_ms=20;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(vp_candidate_matches(VP_SHADOW,&out,&in.stamp,119));
    CHECK(!vp_candidate_matches(VP_SHADOW,&out,&in.stamp,120));
    c=config();c.encoder_max_age_ms=20;in.stamp.encoder_ms=100;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(vp_candidate_matches(VP_SHADOW,&out,&in.stamp,119));
    CHECK(!vp_candidate_matches(VP_SHADOW,&out,&in.stamp,120));
    c=config();
    /* All three clocks and sequence IDs can wrap; none require nonzero seq. */
    in=input();in.now_ms=5;in.stamp.capture_ms=UINT32_MAX-4;in.stamp.encoder_ms=0;
    in.stamp.encoder_seq=in.stamp.frame_seq=0;in.preview.sequence=0;in.preview.capture_ms=in.stamp.capture_ms;
    CHECK(vp_prepare(VP_SHADOW,&c,&in,&out)==VP_READY);
    CHECK(vp_candidate_matches(VP_SHADOW,&out,&in.stamp,24));
    CHECK(!vp_candidate_matches(VP_SHADOW,&out,&in.stamp,25));
}
int main(void)
{
    gates();data_validation();quantization_and_cap();semantics_and_oracle();quantization_boundaries();identities_and_expiry();
    printf("PLANNER INTERFACE PASS: %u checks; synthetic SI inputs, shadow only.\n",checks);return 0;
}
