#include "vision_metric_pi.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"metric:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static vm_odom_config_t odom_config(void)
{vm_odom_config_t c={1,30,20,1000,2000,-1,1};return c;}
static vm_encoder_t encoder(uint32_t seq,uint32_t ms,int32_t left,int32_t right)
{vm_encoder_t e={seq,ms,left,right,-1,1,1};return e;}
static vm_vision_config_t vision_config(void)
{
    vm_vision_config_t c;memset(&c,0,sizeof(c));c.homography[0]=.01f;c.homography[2]=-.93f;
    c.homography[4]=-.01f;c.homography[5]=1.2f;c.homography[8]=1;
    c.min_x_m=-.2f;c.max_x_m=.2f;c.min_y_m=0;c.max_y_m=1;c.max_step_m=.5f;
    c.near_count=c.full_count=5;c.min_confidence=50;c.revision=1;c.max_age_ms=100;
    for(unsigned j=0;j<5;++j){c.near_rows[j]=(uint8_t)(110-j*10);c.full_rows[j]=(uint8_t)(110-j*15);}return c;
}
static vision_result_t lane(void)
{
    vision_result_t r;memset(&r,0,sizeof(r));r.valid=1;r.status=VISION_TRACK;r.confidence=90;
    for(unsigned y=0;y<VISION_HEIGHT;++y){r.left[y]=60;r.right[y]=126;r.center[y]=93;r.row_flags[y]=3;}return r;
}
static vps_config_t planner_config(void)
{
    vps_config_t c;memset(&c,0,sizeof(c));c.planner.revision=1;
    c.planner.limits=(sp_limits_t){SP_SIMULATION,3,1,2,1,2,4,6,.02f,.02f,.0001f,.05f};
    c.planner.ticks_per_metre_left=1000;c.planner.ticks_per_metre_right=2000;
    c.planner.max_left_ticks_10ms=c.planner.max_right_ticks_10ms=1000;
    c.planner.frame_max_age_ms=100;c.planner.encoder_max_age_ms=30;c.planner.candidate_max_age_ms=20;
    c.policy=(sp_policy_t){100,30,300,1000,1000,3,3,3,1,.01f};return c;
}
static void native_and_history(void)
{
    vm_odom_t s;vm_motion_t m;vm_odom_config_t c=odom_config();float x;
    vd_speed_info_t raw;memset(&raw,0,sizeof(raw));raw.signs_confirmed=1;raw.sign3=-1;raw.sign4=1;
    raw.total3=100;raw.total4=-100;raw.sample_ms=100;raw.raw3=123;raw.raw4=456;
    vm_encoder_t e;CHECK(!vm_encoder_from_speed(&raw,0,&e));CHECK(vm_encoder_from_speed(&raw,1,&e));
    CHECK(e.total_left==100&&e.total_right==-100&&e.sequence==0);
    CHECK(vm_odom_init(&s,&c));CHECK(vm_odom_current(&s,100,&m)==VM_WAIT);
    CHECK(vm_odom_push(&s,&e)==VM_OK);CHECK(vm_odom_current(&s,100,&m)==VM_OK&&!m.velocity_valid);
    e=encoder(1,110,90,-80);CHECK(vm_odom_push(&s,&e)==VM_OK);CHECK(vm_odom_current(&s,110,&m)==VM_OK);
    CHECK(m.delta_left==10&&m.delta_right==20&&m.elapsed_ms==10&&m.sequence_span==1);
    CHECK(fabsf(m.left_speed_m_s-1)<1e-6f&&fabsf(m.right_speed_m_s-1)<1e-6f);
    CHECK(fabsf(m.odometer_m-.01f)<1e-6f);
    CHECK(vm_odom_at_capture(&s,105,110,&x)==VM_OK&&fabsf(x-.005f)<1e-6f);
    CHECK(vm_odom_at_capture(&s,110,110,&x)==VM_OK&&fabsf(x-.01f)<1e-6f);
    CHECK(vm_odom_at_capture(&s,115,115,&x)==VM_WAIT);
    CHECK(vm_odom_at_capture(&s,120,115,&x)==VM_BAD_SOURCE);
    CHECK(vm_odom_current(&s,140,&m)==VM_EXPIRED);CHECK(!m.odometer_m);
    uint32_t revision=s.revision;CHECK(vm_odom_push(&s,&e)==VM_DUPLICATE&&s.revision==revision);
    e.total_left--;CHECK(vm_odom_push(&s,&e)==VM_CONFLICT&&s.revision==revision);
    e=encoder(0,100,100,-100);CHECK(vm_odom_push(&s,&e)==VM_OLD);
    e=encoder(2,120,91,-79);CHECK(vm_odom_push(&s,&e)==VM_OK);CHECK(vm_odom_current(&s,120,&m)==VM_OK);
    CHECK(m.left_speed_m_s<0&&m.right_speed_m_s>0&&m.odometer_m>.01f);
    e=encoder(4,140,71,-39);CHECK(vm_odom_push(&s,&e)==VM_OK);
    CHECK(vm_odom_at_capture(&s,130,140,&x)==VM_NO_BRACKET);CHECK(vm_odom_current(&s,140,&m)==VM_OK&&m.sequence_span==2);
    CHECK(vm_odom_at_capture(&s,140,140,&x)==VM_OK);
    e=encoder(5,150,INT32_MIN,0);CHECK(vm_odom_push(&s,&e)==VM_BAD_SOURCE);
    raw.faults=1;CHECK(!vm_encoder_from_speed(&raw,1,&e));
    for(unsigned j=0;j<40;++j){e=encoder(5+j,150+j*10,61-(int32_t)j*10,-19+(int32_t)j*20);CHECK(vm_odom_push(&s,&e)==VM_OK);}
    CHECK(s.count==VM_HISTORY_CAPACITY);CHECK(vm_odom_at_capture(&s,150,540,&x)==VM_NO_BRACKET);
    CHECK(vm_odom_at_capture(&s,535,540,&x)==VM_OK);
    CHECK(vm_odom_init(&s,&c));e=encoder(UINT32_MAX,UINT32_MAX-5,0,0);CHECK(vm_odom_push(&s,&e)==VM_OK);
    e=encoder(0,4,-10,20);CHECK(vm_odom_push(&s,&e)==VM_OK);
    CHECK(vm_odom_at_capture(&s,UINT32_MAX,4,&x)==VM_OK&&fabsf(x-.005f)<1e-6f);
    c.ticks_per_metre_left=NAN;CHECK(!vm_odom_init(&s,&c));CHECK(vm_odom_push(&s,&e)==VM_BAD_CONFIG);
    c=odom_config();CHECK(vm_odom_init(&s,&c));e=encoder(0,0,0,0);CHECK(vm_odom_push(&s,&e)==VM_OK);
    e=encoder(1,UINT32_C(0x70000000),0,0);CHECK(vm_odom_push(&s,&e)==VM_OK);
    e=encoder(2,UINT32_C(0xe0000000),0,0);CHECK(vm_odom_push(&s,&e)==VM_OK);
    CHECK(s.count==2); /* The sample at0 is no longer in the clock half-domain. */
}
static void vision_and_bridge(void)
{
    vm_vision_t s;vm_vision_init(&s);vm_vision_config_t c=vision_config();vision_result_t road=lane();vm_frame_t f;
    CHECK(vm_vision_frame(&s,&c,&road,1,105,110,&f));CHECK(f.near_valid&&f.far_valid&&f.domain_valid);
    CHECK(f.near_preview.units==SP_METRES&&f.near_preview.sequence==1);
    CHECK(fabsf(f.near_preview.distance[4]-.4f)<1e-5f);
    road.row_flags[50]=1;CHECK(vm_vision_frame(&s,&c,&road,2,115,120,&f));CHECK(f.near_valid&&!f.far_valid);
    road.row_flags[110]=1;CHECK(vm_vision_frame(&s,&c,&road,3,125,130,&f));CHECK(!f.near_valid);
    road=lane();c.min_x_m=.1f;CHECK(vm_vision_frame(&s,&c,&road,4,135,140,&f));CHECK(!f.domain_valid);
    c=vision_config();++c.revision;CHECK(!vm_vision_frame(&s,&c,&road,4,135,140,&f));
    vm_vision_init(&s);CHECK(vm_vision_frame(&s,&c,&road,4,135,140,&f)&&f.near_valid);
    vm_odom_t odom;vm_odom_config_t oc=odom_config();vm_encoder_t e=encoder(0,100,0,0);CHECK(vm_odom_init(&odom,&oc));
    CHECK(vm_odom_push(&odom,&e)==VM_OK);e=encoder(1,110,-10,20);CHECK(vm_odom_push(&odom,&e)==VM_OK);
    vm_vision_init(&s);c=vision_config();CHECK(vm_vision_frame(&s,&c,&road,1,105,110,&f));
    vp_stamp_t id;memset(&id,0,sizeof(id));id.config_revision=1;id.output_epoch=9;
    vps_input_t in;CHECK(vm_shadow_input(&odom,&f,1,id,110,1,1,&in)==VM_OK);
    CHECK(in.observation.new_frame&&in.observation.capture_odometer_valid&&in.valid==VP_VALID_ALL);
    CHECK(fabsf(in.observation.odometer_at_capture_m-.005f)<1e-6f&&in.observation.encoder_seq==1);
    CHECK(in.identity.capture_ms==105&&in.identity.encoder_ms==110);
    /* Row100 belongs only to near. Full evidence must survive its rejection,
     * without granting the missing near/domain admission bits. */
    vm_vision_init(&s);road=lane();road.row_flags[100]=1;
    CHECK(vm_vision_frame(&s,&c,&road,1,105,110,&f));
    CHECK(!f.near_valid&&f.far_valid&&!f.domain_valid);
    CHECK(vm_shadow_input(&odom,&f,1,id,110,1,1,&in)==VM_OK);
    CHECK(!in.observation.near_valid&&in.observation.far_valid);
    CHECK(in.observation.capture_odometer_valid&&in.valid==VP_VALID_ENCODER);
    vps_state_t planner;vps_config_t pc=planner_config();
    CHECK(vps_init(&planner,VP_SHADOW,&pc)&&!vps_arm(&planner,&in));
    /* Optional alignment cannot hold back near loss or invent an origin. */
    f.capture_ms=115;CHECK(vm_shadow_input(&odom,&f,1,id,115,1,1,&in)==VM_OK);
    CHECK(!in.observation.near_valid&&in.observation.far_valid&&!in.observation.capture_odometer_valid);
    f.capture_ms=50;CHECK(vm_shadow_input(&odom,&f,1,id,110,1,1,&in)==VM_OK);
    CHECK(!in.observation.capture_odometer_valid);
    /* Shared sample loss invalidates both; full-only rejection preserves near. */
    road=lane();road.row_flags[110]=1;vm_vision_init(&s);
    CHECK(vm_vision_frame(&s,&c,&road,2,106,110,&f)&&!f.near_valid&&!f.far_valid);
    road=lane();road.row_flags[50]=1;vm_vision_init(&s);
    CHECK(vm_vision_frame(&s,&c,&road,3,107,110,&f)&&f.near_valid&&!f.far_valid);
    road=lane();vm_vision_init(&s);
    CHECK(vm_vision_frame(&s,&c,&road,4,108,110,&f)&&f.near_valid&&f.far_valid);
    f.capture_ms=115;CHECK(vm_shadow_input(&odom,&f,1,id,115,1,1,&in)==VM_WAIT);
    CHECK(!in.observation.capture_odometer_valid&&!in.observation.encoder_valid);
    e=encoder(2,120,-20,40);CHECK(vm_odom_push(&odom,&e)==VM_OK);
    CHECK(vm_shadow_input(&odom,&f,1,id,120,1,1,&in)==VM_OK);
}
static void metric_pi_chain(void)
{
    vm_odom_t odom;vm_odom_config_t oc=odom_config();vm_vision_t vision;vm_vision_init(&vision);
    vm_vision_config_t vc=vision_config();vision_result_t road=lane();vm_frame_t frame;vm_motion_t motion;
    vm_encoder_t e=encoder(0,100,0,0);CHECK(vm_odom_init(&odom,&oc));CHECK(vm_odom_push(&odom,&e)==VM_OK);
    e=encoder(1,110,-10,20);CHECK(vm_odom_push(&odom,&e)==VM_OK);
    vp_stamp_t id;memset(&id,0,sizeof(id));id.config_revision=1;id.output_epoch=9;
    CHECK(vm_vision_frame(&vision,&vc,&road,1,105,110,&frame));
    vps_input_t input;CHECK(vm_shadow_input(&odom,&frame,1,id,110,1,1,&input)==VM_OK);
    vps_state_t planner;vps_config_t pc=planner_config();CHECK(vps_init(&planner,VP_SHADOW,&pc));CHECK(vps_arm(&planner,&input));
    CHECK(vm_odom_current(&odom,110,&motion)==VM_OK);
    vm_pi_state_t pi;vm_pi_config_t config={1,256,12,512,24,600,500};
    CHECK(vm_pi_init(&pi,&config,123,&motion));ws_pi_t left={0},right={0};
    for(unsigned j=0;j<200;++j){
        unsigned dt=j%3?10:20;int32_t measured_l=8+(int32_t)(j%5),measured_r=18+(int32_t)(j%3);
        e.sequence++;e.ms+=dt;e.total_left-=measured_l*(int32_t)(dt/10);e.total_right+=measured_r*(int32_t)(dt/10);
        CHECK(vm_odom_push(&odom,&e)==VM_OK);
        CHECK(vm_vision_frame(&vision,&vc,&road,e.sequence,e.ms-5,e.ms,&frame));
        CHECK(vm_shadow_input(&odom,&frame,1,id,e.ms,1,1,&input)==VM_OK);
        vps_result_t result;CHECK(vps_tick(&planner,&input,&result)==VPS_READY);
        CHECK(vm_odom_current(&odom,e.ms,&motion)==VM_OK);
        vm_pi_candidate_t candidate;CHECK(vm_pi_prepare(&pi,&planner,&result,&motion,&input.identity,e.ms,700,80,&candidate)==VM_PI_READY);
        CHECK(candidate.target_left==10&&candidate.target_right==20);
        CHECK(candidate.measured_left==measured_l&&candidate.measured_right==measured_r);
        ws_pi_step_ff_ms(&left,10,measured_l,256,12,600,dt,700);ws_pi_step_ff_ms(&right,20,measured_r,512,24,500,dt,80);
        CHECK(candidate.left.output==left.output&&candidate.right.output==right.output);
        CHECK(candidate.left.integral_q8==left.integral_q8&&candidate.right.integral_q8==right.integral_q8);
        CHECK(vm_pi_commit(&pi,&planner,&result,&motion,&input.identity,e.ms,&candidate));
        CHECK(!vm_pi_commit(&pi,&planner,&result,&motion,&input.identity,e.ms,&candidate));
        vm_pi_candidate_t duplicate;CHECK(vm_pi_prepare(&pi,&planner,&result,&motion,&input.identity,e.ms,0,0,&duplicate)==VM_PI_WAIT);
        CHECK(!duplicate.left.output&&!duplicate.right.output);
    }
    /* Prepare one more result, then exercise publication rejection. */
    e.sequence++;e.ms+=10;e.total_left-=10;e.total_right+=20;CHECK(vm_odom_push(&odom,&e)==VM_OK);
    CHECK(vm_vision_frame(&vision,&vc,&road,e.sequence,e.ms-5,e.ms,&frame));
    CHECK(vm_shadow_input(&odom,&frame,1,id,e.ms,1,1,&input)==VM_OK);
    vps_result_t result;CHECK(vps_tick(&planner,&input,&result)==VPS_READY);CHECK(vm_odom_current(&odom,e.ms,&motion)==VM_OK);
    vm_pi_candidate_t candidate;CHECK(vm_pi_prepare(&pi,&planner,&result,&motion,&input.identity,e.ms,20,20,&candidate)==VM_PI_READY);
    vp_stamp_t changed=input.identity;changed.output_epoch++;
    CHECK(!vm_pi_commit(&pi,&planner,&result,&motion,&changed,e.ms,&candidate));
    CHECK(!vm_pi_commit(&pi,&planner,&result,&motion,&input.identity,e.ms+20,&candidate));
    vm_motion_t changed_feedback=motion;changed_feedback.revision++;
    CHECK(!vm_pi_commit(&pi,&planner,&result,&changed_feedback,&input.identity,e.ms,&candidate));
    changed_feedback=motion;changed_feedback.ms++;changed_feedback.elapsed_ms++;
    CHECK(!vm_pi_commit(&pi,&planner,&result,&changed_feedback,&input.identity,e.ms+1,&candidate));
    vm_pi_stop(&pi,NULL);CHECK(!vm_pi_commit(&pi,&planner,&result,&motion,&input.identity,e.ms,&candidate));
    vm_pi_stop(&pi,&motion);CHECK(!pi.left.output&&!pi.right.output&&!pi.left.integral_q8&&!pi.right.integral_q8);
    CHECK(!vm_pi_commit(&pi,&planner,&result,&motion,&input.identity,e.ms,&candidate));
    CHECK(vm_pi_init(&pi,&config,124,&motion));CHECK(!vm_pi_commit(&pi,&planner,&result,&motion,&input.identity,e.ms,&candidate));
    /* The composed facade must clear PI on a near-invalid result even if
     * there is no new feedback sample to advance a normal control step. */
    pi.left.output=200;pi.right.output=300;pi.left.integral_q8=1234;pi.right.integral_q8=2345;
    e.sequence++;e.ms+=10;e.total_left-=10;e.total_right+=20;CHECK(vm_odom_push(&odom,&e)==VM_OK);
    road.valid=0;CHECK(vm_vision_frame(&vision,&vc,&road,e.sequence,e.ms-5,e.ms,&frame));
    CHECK(vm_shadow_input(&odom,&frame,1,id,e.ms,1,1,&input)==VM_OK);
    CHECK(vps_tick(&planner,&input,&result)==VPS_ZERO);CHECK(vm_odom_current(&odom,e.ms,&motion)==VM_OK);
    CHECK(vm_pi_apply_shadow(&pi,&planner,&result,&motion,&input.identity,e.ms,100,100,&candidate)==VM_PI_ZERO);
    CHECK(!pi.left.output&&!pi.right.output&&!pi.left.integral_q8&&!pi.right.integral_q8);
    road=lane();
    for(unsigned j=0;j<3;++j){
        e.sequence++;e.ms+=10;e.total_left-=10;e.total_right+=20;CHECK(vm_odom_push(&odom,&e)==VM_OK);
        CHECK(vm_vision_frame(&vision,&vc,&road,e.sequence,e.ms-5,e.ms,&frame));
        CHECK(vm_shadow_input(&odom,&frame,1,id,e.ms,1,1,&input)==VM_OK);(void)vps_tick(&planner,&input,&result);
        CHECK(vm_odom_current(&odom,e.ms,&motion)==VM_OK);
        (void)vm_pi_apply_shadow(&pi,&planner,&result,&motion,&input.identity,e.ms,100,100,&candidate);
    }
    CHECK(result.status==VPS_READY&&candidate.status==VM_PI_READY);
    uint16_t held_l=pi.left.output,held_r=pi.right.output;
    CHECK(vps_tick(&planner,&input,&result)==VPS_WAIT);
    CHECK(vm_pi_apply_shadow(&pi,&planner,&result,&motion,&input.identity,e.ms,0,0,&candidate)==VM_PI_WAIT);
    CHECK(pi.left.output==held_l&&pi.right.output==held_r);
}
static void synthetic_closed_loop(void)
{
    /* Three distinct first-order plants exercise the whole adapter chain,
     * including quantized counters, different wheel scales and target drops.
     * These dynamics/gains are fixtures, never vehicle-tuning proposals. */
    for(unsigned plant=0;plant<3;++plant){
        vm_odom_t odom;vm_odom_config_t oc=odom_config();CHECK(vm_odom_init(&odom,&oc));
        vm_vision_t vision;vm_vision_init(&vision);vm_vision_config_t vc=vision_config();
        vision_result_t road=lane();vm_frame_t frame;vm_motion_t motion;
        vm_encoder_t e=encoder(0,100,0,0);CHECK(vm_odom_push(&odom,&e)==VM_OK);
        e.sequence++;e.ms+=10;CHECK(vm_odom_push(&odom,&e)==VM_OK);
        vp_stamp_t id={0};id.config_revision=1;id.output_epoch=9;
        vps_input_t input;CHECK(vm_vision_frame(&vision,&vc,&road,1,105,110,&frame));
        CHECK(vm_shadow_input(&odom,&frame,1,id,110,.4f,1,&input)==VM_OK);
        vps_state_t planner;vps_config_t pc=planner_config();CHECK(vps_init(&planner,VP_SHADOW,&pc));CHECK(vps_arm(&planner,&input));
        CHECK(vm_odom_current(&odom,e.ms,&motion)==VM_OK);
        vm_pi_config_t config={1,5*256,128,5*256,128,600,600};vm_pi_state_t pi;
        CHECK(vm_pi_init(&pi,&config,(uint64_t)plant+1,&motion));
        double velocity_l=0,velocity_r=0,counts_l=0,counts_r=0;
        double gain_l=.003+.001*plant,gain_r=.005-.001*plant;
        for(unsigned stage=0;stage<3;++stage){
            float target=stage==0?.4f:stage==1?.7f:.3f;
            double mean_l=0,mean_r=0;
            for(unsigned j=0;j<600;++j){
                velocity_l+=(gain_l*pi.left.output-velocity_l)*.1;
                velocity_r+=(gain_r*pi.right.output-velocity_r)*.1;
                counts_l+=velocity_l*oc.ticks_per_metre_left*.01;
                counts_r+=velocity_r*oc.ticks_per_metre_right*.01;
                e.sequence++;e.ms+=10;e.total_left=-(int32_t)counts_l;e.total_right=(int32_t)counts_r;
                CHECK(vm_odom_push(&odom,&e)==VM_OK);
                CHECK(vm_vision_frame(&vision,&vc,&road,e.sequence,e.ms-5,e.ms,&frame));
                CHECK(vm_shadow_input(&odom,&frame,1,id,e.ms,target,1,&input)==VM_OK);
                vps_result_t result;CHECK(vps_tick(&planner,&input,&result)==VPS_READY);
                CHECK(vm_odom_current(&odom,e.ms,&motion)==VM_OK);vm_pi_candidate_t c;
                CHECK(vm_pi_apply_shadow(&pi,&planner,&result,&motion,&input.identity,e.ms,0,0,&c)==VM_PI_READY);
                CHECK(pi.left.output<=600&&pi.right.output<=600);
                if(j>=500){mean_l+=velocity_l;mean_r+=velocity_r;}
            }
            mean_l/=100;mean_r/=100;
            printf("SYNTHETIC METRIC PLANT %u target=%.2f left=%.4f right=%.4f\n",plant,(double)target,mean_l,mean_r);
            CHECK(fabs(mean_l-target)<.025&&fabs(mean_r-target)<.025);
        }
    }
}
int main(void)
{
    native_and_history();vision_and_bridge();metric_pi_chain();synthetic_closed_loop();
    printf("METRIC CONTROL PASS: %u checks; native snapshots and shared PI, diagnostic only.\n",checks);return 0;
}
