#define _CRT_SECURE_NO_WARNINGS
/* Actual 94x60 samples; nearest 2x2 replication cannot restore missing pixels.
 * Each pose starts independently. Repeats exercise static acquisition only;
 * 20ms here is simulated controller time, not capture or vehicle timing. */
#include "vision_midterm.h"
#include "vision_control.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static uint8_t preview[94*60],gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
static vision_result_t lane;
static vc_controller_t control;
static unsigned checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("FAIL line=%d frame=%d mirror=%d: %s\n",__LINE__,frame,mirror,#c);}}while(0)
static int load(int frame,int mirror){
    char name[128];FILE *f;
    sprintf(name,"fixtures/tangent-preview/frame-%06d.gray",frame);f=fopen(name,"rb");
    if(!f){perror(name);return 0;}
    if(fread(preview,1,sizeof(preview),f)!=sizeof(preview)||fgetc(f)!=EOF){fclose(f);return 0;}
    fclose(f);
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x)
        gray[y*VISION_WIDTH+x]=preview[y/2*94+(mirror?93-x/2:x/2)];
    return 1;
}
static void run(int enabled){
    vc_config_t cfg;
    vision_midterm_route_enable(0);vision_midterm_reset();vision_midterm_route_enable((uint8_t)enabled);
    vc_default_config(&cfg);cfg.near_y=.86f;cfg.far_y=.62f;cfg.kp=cfg.heading_gain=1.20f;cfg.max_run_ms=0;
    cfg.min_coverage=VISION_TRACK_MIN_COVERAGE;cfg.duty_rise_per_s=.20f;
    cfg.axis_far_x=MT_ROUTE_AXIS_FAR_X;
    cfg.axis_near_x=MT_ROUTE_AXIS_FAR_X+(MT_ROUTE_AXIS_NEAR_X-MT_ROUTE_AXIS_FAR_X)*(.86f-.62f)/(.78f-.62f);cfg.calibration_confirmed=1;cfg.cruise_duty=.145f;cfg.duty_cap=.18f;
    vc_init(&control,&cfg);
    for(unsigned i=1;i<=24;++i){
        vision_midterm_process(gray,binary,&lane);
        vc_observe(&control,&lane,i,i*20,i*20);
        if(enabled&&control.output.state==VC_DISARMED)(void)vc_arm_local(&control,i*20,1);
    }
}
int main(void){
    const int frames[]={40,41,43,44,45};
    int frame,mirror;
    for(unsigned i=0;i<sizeof(frames)/sizeof(frames[0]);++i)for(mirror=0;mirror<=1;++mirror){
        int side=mirror?1:-1;frame=frames[i];if(!load(frame,mirror))return 2;run(1);
        CHECK(lane.valid&&lane.roundabout_state==1&&lane.roundabout_side==side);
        CHECK(lane.entry_guide_rows>=12);
        CHECK(side*(lane.error_px-lane.raw_error_px)>10);
        CHECK(control.output.observation_valid&&control.output.far_valid);
        CHECK(side*control.output.steering>.15f);
        for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(lane.entry_guide[y]<VISION_WIDTH){
            int point=lane.entry_guide[y],margin=(lane.row_width[y]+3)/4;
            CHECK(point-margin>=0&&point+margin<VISION_WIDTH);
            for(int x=point-margin;x<=point+margin;++x)CHECK(binary[y*VISION_WIDTH+x]);
        }
        printf("FIELD STATIC frame=%d mirror=%d state=%u side=%d guide=%u steer=%.3f\n",frame,mirror,lane.roundabout_state,lane.roundabout_side,lane.entry_guide_rows,(double)control.output.steering);
        /* OFF must revoke ring intent, independent of the visible island. */
        run(0);CHECK(!lane.roundabout_state&&!lane.entry_guide_rows);
        /* Bend only the outside boundary: an open curved road is not proof
         * of a tangent island. Both fit residual limits must still reject it. */
        run(1);
        for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
            int edge=mirror?lane.left[y]:lane.right[y];
            float d=((float)y-67)/43;
            int amount=(int)(24*(1-d*d));
            if(edge>=VISION_WIDTH||amount<=0)continue;
            for(int k=0;k<amount;++k){int x=edge+(mirror?k:-k);if(x>=0&&x<VISION_WIDTH)gray[y*VISION_WIDTH+x]=0;}
        }
        run(1);CHECK(!lane.roundabout_state&&!lane.entry_guide_rows);
    }
    for(frame=20;frame<=51;++frame)if(frame==20||frame==24||frame==30||frame==51)for(mirror=0;mirror<=1;++mirror){
        if(!load(frame,mirror))return 2;run(1);
        CHECK(!lane.roundabout_state&&!lane.entry_guide_rows);
        if(frame!=30)CHECK(!control.output.observation_valid);
    }
    frame=65;mirror=0;if(!load(frame,mirror))return 2;run(1);
    CHECK(lane.valid&&lane.roundabout_state==1&&lane.roundabout_side==1&&lane.entry_guide_rows>=12);
    CHECK(control.output.steering>0);
    printf("FIELD PREVIEW %s: %u checks, %u failures; independent poses, no physical timing claim\n",failures?"FAIL":"PASS",checks,failures);
    return failures?1:0;
}
