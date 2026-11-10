/* Host composition probe. Gray -> production vision -> metric adapter -> real
 * odometry bridge -> shadow supervisor/planner; legacy control observed in
 * parallel. No synthesized row flags, hardware IO, or automatic rearm. */
#include "vision_planner_runtime.h"
#include "vision_control.h"
#include "phase3_shadow_profile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];

static vdr_config_t configuration(int measured)
{
    vdr_config_t c=phase3_shadow_configuration;
    if(measured)return c;
    /* Explicit synthetic map and policy. These are NOT camera calibration or
     * production changes; disjoint near/full rows exercise independent loss. */
    memset(&c,0,sizeof(c));
    c.planning.planner.revision=1;
    c.planning.planner.limits=(sp_limits_t){SP_SIMULATION,2,.5f,1,1,2,4,6,.02f,.02f,.0001f,.02f};
    c.planning.planner.ticks_per_metre_left=c.planning.planner.ticks_per_metre_right=1000;
    c.planning.planner.max_left_ticks_10ms=c.planning.planner.max_right_ticks_10ms=100;
    c.planning.planner.frame_max_age_ms=100;c.planning.planner.encoder_max_age_ms=30;
    c.planning.planner.candidate_max_age_ms=20;
    c.planning.policy=(sp_policy_t){100,30,300,600,2000,5,3,3,.24f,.02f};
    c.odometry=(vm_odom_config_t){1,30,20,1000,1000,-1,1};
    c.vision.homography[0]=.01f;c.vision.homography[2]=-.935f;
    c.vision.homography[4]=-.01f;c.vision.homography[5]=1.2f;c.vision.homography[8]=1;
    c.vision.min_x_m=-1;c.vision.max_x_m=1;c.vision.min_y_m=0;c.vision.max_y_m=1;
    c.vision.max_step_m=.5f;c.vision.min_confidence=50;
    c.vision.near_count=c.vision.full_count=5;c.vision.revision=1;c.vision.max_age_ms=100;
    for(unsigned j=0;j<5;++j){c.vision.near_rows[j]=(uint8_t)(110-j*10);c.vision.full_rows[j]=(uint8_t)(110-j*15);}
    c.requested_speed_m_s=.4f;return c;
}

static int missing(const vision_result_t *r,const uint8_t *rows,unsigned n)
{for(unsigned j=0;j<n;++j)if(r->row_flags[rows[j]]!=3)return rows[j];return -1;}

#define TEST(x) do{if(!(x)){fprintf(stderr,"timing:%d %s\n",__LINE__,#x);return 10;}}while(0)
static int timing_contract(void){
    vdr_config_t c=configuration(0);vm_vision_t v;vm_odom_t o;vps_state_t p;
    vm_encoder_t e={0,0,0,0,-1,1,1};vm_frame_t f;vision_result_t r;
    vm_vision_init(&v);TEST(vm_odom_init(&o,&c.odometry));TEST(vm_odom_push(&o,&e)==VM_OK);
    TEST(vps_init(&p,VP_SHADOW,&c.planning));vision_midterm_reset();vision_midterm_route_enable(1);
    for(unsigned y=0;y<120;++y)for(unsigned x=0;x<188;++x)gray[y*188+x]=x>=54&&x<=134?210:30;
    unsigned seq=0;
    for(unsigned k=0;k<80;++k){
        ++e.sequence;e.ms+=10;e.total_left-=3;e.total_right+=3;TEST(vm_odom_push(&o,&e)==VM_OK);
        int fresh=k%2==0;uint32_t now=e.ms+4;
        if(fresh){vision_midterm_process(gray,binary,&r);TEST(vm_vision_frame(&v,&c.vision,&r,++seq,e.ms,now,&f));TEST(f.near_valid&&f.far_valid);}
        vp_stamp_t id={0};id.output_epoch=1;id.config_revision=1;vps_input_t in;vps_result_t result;
        TEST(vm_shadow_input(&o,&f,fresh,id,now,.4f,1,&in)==VM_OK);
        if(!k)TEST(vps_arm(&p,&in));else{TEST(vps_tick(&p,&in,&result)==VPS_READY);TEST(vps_result_matches(&p,&result,&in.identity,now));}
    }
    /* Re-reading the same frame must not renew its geometry history. */
    vm_frame_t rejected;TEST(vm_vision_frame(&v,&c.vision,&r,seq,f.capture_ms,e.ms+4,&rejected));TEST(!rejected.near_valid&&!rejected.far_valid);
    /* Keep publishing encoder samples after the camera stops. Cached image
     * reuse may bridge cadence, but cannot refresh the image's capture time. */
    for(unsigned k=0;k<9;++k){
        ++e.sequence;e.ms+=10;e.total_left-=3;e.total_right+=3;TEST(vm_odom_push(&o,&e)==VM_OK);
        vp_stamp_t id={0};id.output_epoch=1;id.config_revision=1;
        vps_input_t in;vps_result_t result;uint32_t now=e.ms+4;
        TEST(vm_shadow_input(&o,&f,0,id,now,.4f,1,&in)==VM_OK);
        vps_status_t status=vps_tick(&p,&in,&result);
        if(now-f.capture_ms<100)TEST(status==VPS_READY);
        else TEST(status==VPS_FAULT&&(p.supervisor.faults&SP_FAULT_FRAME)&&p.supervisor.motor_inhibit&&
                  !result.candidate.left_ticks_10ms&&!result.candidate.right_ticks_10ms);
    }
    vm_vision_init(&v);TEST(vm_vision_frame(&v,&c.vision,&r,seq+1,1000,1099,&rejected));TEST(rejected.near_valid);
    vm_vision_init(&v);TEST(vm_vision_frame(&v,&c.vision,&r,seq+1,1000,1100,&rejected));TEST(!rejected.near_valid&&!rejected.far_valid);
    puts("PASS camera20ms/encoder10ms 80ticks plus 9 camera-stall ticks; duplicate frame rejection; age99/100ms boundary");return 0;
}

#undef TEST

int main(int argc,char **argv)
{
    if(argc==2&&!strcmp(argv[1],"--timing-contract"))return timing_contract();
    if(argc!=6&&argc!=7)return 2;
    FILE *input=fopen(argv[1],"rb"),*trace=fopen(argv[2],"w"),*masks=fopen(argv[3],"wb"),*geometry=fopen(argv[4],"w");
    if(!input||!trace||!masks||!geometry)return 3;
    int measured=!strcmp(argv[5],"measured");
    if(!measured&&strcmp(argv[5],"synthetic"))return 2;
    vdr_config_t cfg=configuration(measured);
    if(argc==7){
        cfg.requested_speed_m_s=(float)atof(argv[6]);
        if(!(cfg.requested_speed_m_s>0&&cfg.requested_speed_m_s<=cfg.planning.planner.limits.maximum_speed))return 2;
    }
    vm_vision_t vision;vm_vision_init(&vision);
    vm_odom_t odom;vps_state_t planner;
    if(!vm_odom_init(&odom,&cfg.odometry)||!vps_init(&planner,VP_SHADOW,&cfg.planning))return 4;
    vm_encoder_t encoder={0,0,0,0,-1,1,1};
    if(vm_odom_push(&odom,&encoder)!=VM_OK)return 5;
    vc_config_t legacy_cfg;vc_default_config(&legacy_cfg);
    legacy_cfg.near_y=.86f;legacy_cfg.far_y=.62f;legacy_cfg.kp=legacy_cfg.heading_gain=1.20f;
    legacy_cfg.max_run_ms=0;legacy_cfg.min_coverage=VISION_TRACK_MIN_COVERAGE;
    legacy_cfg.axis_far_x=MT_ROUTE_AXIS_FAR_X;
    legacy_cfg.axis_near_x=MT_ROUTE_AXIS_FAR_X+(MT_ROUTE_AXIS_NEAR_X-MT_ROUTE_AXIS_FAR_X)*(.86f-.62f)/(.78f-.62f);
    legacy_cfg.calibration_confirmed=1;legacy_cfg.cruise_duty=.145f;legacy_cfg.duty_cap=.18f;
    vc_controller_t legacy;vc_init(&legacy,&legacy_cfg);
    vision_midterm_route_enable(0);vision_midterm_reset();vision_midterm_route_enable(1);
    unsigned index=0,armed=0,legacy_armed=0;
    fprintf(trace,"index,raw_valid,confidence,ring_state,ring_side,guide_rows,junction_rows,inferred_rows,legacy_near,legacy_far,legacy_vision_state,steering,near,full,near_bad_row,full_bad_row,bridge_status,arm_event,planner_status,supervisor_mode,motor_inhibit,faults,far_active,have_full,cap_m_s,target_speed_m_s,left_ticks_10ms,right_ticks_10ms,near_peak_curvature,full_peak_curvature,candidate_matches,entry_evidence,source_step_m_s,near_cap_m_s,maximum_speed_m_s,guide_delta_px,near_bad_flag,full_bad_flag,candidate_status\n");
    fprintf(geometry,"index,row,left,right,center,flags,guide,binary_at_center,threshold,width,near_ground_x,full_ground_x\n");
    size_t got;
    while((got=fread(gray,1,sizeof(gray),input))==sizeof(gray)){
        vision_result_t road;vm_frame_t frame;vps_input_t in;vps_result_t result;
        memset(&in,0,sizeof(in));memset(&result,0,sizeof(result));
        vision_midterm_process(gray,binary,&road);
        /* One current feedback sample per 20ms planning tick. Publishing two
         * samples but ticking only once would correctly trigger sample-loss
         * protection and would not model this declared cadence. */
        ++encoder.sequence;encoder.ms+=20;
        encoder.total_left-=(int32_t)(cfg.odometry.ticks_per_metre_left*.006f);
        encoder.total_right+=(int32_t)(cfg.odometry.ticks_per_metre_right*.006f);
        if(vm_odom_push(&odom,&encoder)!=VM_OK)return 5;
        uint32_t now=encoder.ms+4,seq=index+1;
        vc_observe(&legacy,&road,seq,encoder.ms,now);
        if(!legacy_armed&&vc_arm_local(&legacy,now,1))legacy_armed=1;
        if(!vm_vision_frame(&vision,&cfg.vision,&road,seq,encoder.ms,now,&frame))return 6;
        vp_stamp_t id;memset(&id,0,sizeof(id));id.output_epoch=1;id.config_revision=cfg.vision.revision;
        vm_status_t bridge=vm_shadow_input(&odom,&frame,1,id,now,cfg.requested_speed_m_s,1,&in);
        unsigned arm_event=0;vps_status_t status=VPS_INACTIVE;
        if(bridge==VM_OK){
            if(!armed){if(vps_arm(&planner,&in)){armed=1;arm_event=1;}}
            else status=vps_tick(&planner,&in,&result);
        }
        float near_curve=0,full_curve=0;
        for(unsigned j=0;j<frame.near_preview.count;++j)if(fabsf(frame.near_preview.curvature[j])>near_curve)near_curve=fabsf(frame.near_preview.curvature[j]);
        for(unsigned j=0;j<frame.full_preview.count;++j)if(fabsf(frame.full_preview.curvature[j])>full_curve)full_curve=fabsf(frame.full_preview.curvature[j]);
        int match=status==VPS_READY?vps_result_matches(&planner,&result,&in.identity,now):0;
        int near_bad=missing(&road,cfg.vision.near_rows,cfg.vision.near_count),full_bad=missing(&road,cfg.vision.full_rows,cfg.vision.full_count);
        fprintf(trace,"%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%.8g,%u,%u,%d,%d,%u,%u,%u,%u,%u,%lu,%u,%u,%.8g,%.8g,%u,%u,%.8g,%.8g,%d,%u,%.8g,%.8g,%.8g,%.8g,%u,%u,%u\n",
            index,road.valid,road.confidence,road.roundabout_state,road.roundabout_side,road.entry_guide_rows,
            road.junction_rows,road.inferred_rows,legacy.output.observation_valid,legacy.output.far_valid,
            legacy.output.vision_state,(double)legacy.output.steering,frame.near_valid,frame.far_valid,
            near_bad,full_bad,
            (unsigned)bridge,arm_event,(unsigned)status,(unsigned)planner.supervisor.mode,planner.supervisor.motor_inhibit,
            (unsigned long)planner.supervisor.faults,planner.supervisor.far_active,planner.have_full,
            (double)result.candidate.cap_m_s,(double)result.candidate.trajectory.speed,
            result.candidate.left_ticks_10ms,result.candidate.right_ticks_10ms,(double)near_curve,(double)full_curve,match,
            road.roundabout_evidence,(double)in.observation.measured_speed,(double)cfg.planning.planner.limits.near_cap,
            (double)cfg.planning.planner.limits.maximum_speed,(double)(road.error_px-road.raw_error_px),
            near_bad>=0?road.row_flags[near_bad]:3,full_bad>=0?road.row_flags[full_bad]:3,(unsigned)result.candidate.status);
        for(unsigned y=0;y<VISION_HEIGHT;++y){
            float nx=NAN,fx=NAN;
            if(frame.near_valid)for(unsigned j=0;j<cfg.vision.near_count;++j)
                if(cfg.vision.near_rows[j]==y)nx=vision.near_history.points[j].x;
            if(frame.far_valid)for(unsigned j=0;j<cfg.vision.full_count;++j)
                if(cfg.vision.full_rows[j]==y)fx=vision.full_history.points[j].x;
            fprintf(geometry,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.8g,%.8g\n",index,y,road.left[y],road.right[y],road.center[y],
                road.row_flags[y],road.entry_guide[y],road.center[y]<VISION_WIDTH?binary[y*VISION_WIDTH+road.center[y]]:0,
                road.row_threshold[y],road.row_width[y],(double)nx,(double)fx);
        }
        if(fwrite(binary,1,sizeof(binary),masks)!=sizeof(binary))return 7;
        ++index;
    }
    if(got||ferror(input)||ferror(trace)||ferror(masks)||ferror(geometry))return 7;
    fclose(input);if(fclose(trace)||fclose(masks)||fclose(geometry))return 7;
    printf("SCENE PIPELINE: %u frames, bundle=%s, one session; no PWM/device IO.\n",index,argv[5]);
    return index?0:8;
}
