/* Map geometry diagnostic, not a vehicle dynamics / camera calibration test.
 * Northbound tangent road x906, island center839/237, R67, road halfwidth30.
 * The fixed pose deliberately ignores steering: only entry and direction are
 * asserted. Completing a real orbit cannot be inferred from this replay. */
#include "vision_midterm.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
static vision_result_t lane;
static unsigned checks,failures,fallbacks,guided;
static double camera_offset,camera_yaw;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("projection FAIL line %d\n",__LINE__);}}while(0)
static void render(double cy,double depth,double fov,int ring,int mirror)
{
    for(int y=0;y<VISION_HEIGHT;++y){
        double scale=fov*(40.0+.75*y)/60.0,z=depth/(y+53.333333);
        for(int x=0;x<VISION_WIDTH;++x){
            int pixel=mirror?VISION_WIDTH-1-x:x;
            double u=(pixel-94.0)/scale;
            double wx=906.0+camera_offset+u*cos(camera_yaw)+z*sin(camera_yaw);
            double wy=cy-z*cos(camera_yaw)+u*sin(camera_yaw);
            double d=hypot(wx-839.0,wy-237.0);
            int white=fabs(wx-906.0)<=30.0||(ring&&fabs(d-67.0)<=30.0);
            gray[y*VISION_WIDTH+x]=(uint8_t)(white?210:30);
        }
    }
}
int main(void)
{
    for(int mirror=0;mirror<2;++mirror)for(int sf=7;sf<=16;++sf)
    for(int depth=20000;depth<=30000;depth+=10000){
        int entered=0,side=mirror?1:-1;float aligned=0;
        vision_midterm_route_enable(0);vision_midterm_reset();
        render(1000,depth,sf*.1,0,mirror);
        for(int i=0;i<12;++i)vision_midterm_process(gray,binary,&lane);
        CHECK(lane.valid);vision_midterm_route_enable(1);
        for(int cy=650;cy>=150;cy-=4){
            render(cy,depth,sf*.1,1,mirror);
            for(int i=0;i<4;++i){
                vision_midterm_process(gray,binary,&lane);
                if(lane.entry_guide_rows){
                    ++guided;CHECK(lane.valid&&lane.roundabout_state==1);
                    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(lane.entry_guide[y]<VISION_WIDTH){
                        int x=lane.entry_guide[y],margin=(lane.row_width[y]+3)/4;
                        CHECK(lane.row_flags[y]>=1&&lane.row_flags[y]<=3);
                        CHECK(x-margin>=0&&x+margin<VISION_WIDTH);
                        {int white=1;for(int at=x-margin;at<=x+margin;++at)
                            if(!binary[y*VISION_WIDTH+at])white=0;
                         CHECK(white);}
                    }
                }
                if(lane.path_fallback){
                    ++fallbacks;CHECK(lane.valid);
                    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(lane.row_flags[y])
                        CHECK(binary[y*VISION_WIDTH+lane.center[y]]);
                }
            }
            if(lane.roundabout_state){entered=1;CHECK(lane.roundabout_side==side);
                if(side*lane.error_px>aligned)aligned=side*lane.error_px;}
        }
        if(!entered||aligned<=3)printf("TANGENT mirror=%d sf=%d depth=%d entered=%d aligned=%.2f width90=%u\n",
            mirror,sf,depth,entered,(double)aligned,lane.row_width[90]);
        CHECK(entered);CHECK(aligned>3);
    }
    CHECK(fallbacks>0);
    CHECK(guided>0);
    /* The original centered replay misses mild installation/approach errors.
     * Learn centered width, then approach at an independent offset and yaw. */
    for(int mirror=0;mirror<2;++mirror)for(int pose=0;pose<9;++pose){
        int entered=0,guide_frames=0,side=mirror?1:-1;
        camera_offset=camera_yaw=0;
        vision_midterm_route_enable(0);vision_midterm_reset();
        render(1000,25000,1.4,0,mirror);
        for(int i=0;i<12;++i)vision_midterm_process(gray,binary,&lane);
        CHECK(lane.valid);vision_midterm_route_enable(1);
        camera_offset=(pose/3-1)*8.0;camera_yaw=(pose%3-1)*.04;
        for(int cy=650;cy>=250;cy-=4){
            render(cy,25000,1.4,1,mirror);
            for(int i=0;i<4;++i){
                vision_midterm_process(gray,binary,&lane);
                if(lane.roundabout_state){entered=1;CHECK(lane.roundabout_side==side);}
                if(lane.entry_guide_rows)++guide_frames;
            }
        }
        printf("TANGENT POSE mirror=%d pose=%d entered=%d guides=%d\n",mirror,pose,entered,guide_frames);
        CHECK(entered&&guide_frames>=4);
    }
    /* Follow a confirmed near cap into bottom clipping. This is an evidence
     * continuity test, with fixed poses; no successful physical turn claim. */
    for(int mirror=0;mirror<2;++mirror){
        camera_offset=camera_yaw=0;
        vision_midterm_route_enable(0);vision_midterm_reset();render(1000,25000,1.4,0,mirror);
        for(int i=0;i<12;++i)vision_midterm_process(gray,binary,&lane);
        vision_midterm_route_enable(1);
        for(int cy=650;cy>=410;cy-=2){render(cy,25000,1.4,1,mirror);
            for(int i=0;i<2;++i)vision_midterm_process(gray,binary,&lane);}
        CHECK(lane.valid&&lane.roundabout_state==1&&lane.entry_guide_rows>=12);
        for(int cy=408;cy>=390;cy-=2){render(cy,25000,1.4,1,mirror);
            for(int i=0;i<2;++i)vision_midterm_process(gray,binary,&lane);
            CHECK(lane.valid&&lane.roundabout_state==1&&lane.entry_guide_rows>=12);
            if(cy<=400){
                if(lane.roundabout_evidence!=2)printf("CONTINUATION mirror=%d cy=%d ring=%u evidence=%u guides=%u valid=%u\n",mirror,cy,lane.roundabout_state,lane.roundabout_evidence,lane.entry_guide_rows,lane.valid);
                CHECK(lane.roundabout_evidence==2);
            }
        }
        /* The exact same clipped view cannot establish a NEW island.
         * Route reset retains learned camera widths, not permission to enter. */
        vision_midterm_route_reset();render(400,25000,1.4,1,mirror);
        for(int i=0;i<20;++i)vision_midterm_process(gray,binary,&lane);
        CHECK(!lane.roundabout_state&&!lane.entry_guide_rows&&!lane.roundabout_evidence);
        /* A far confirmed island cannot jump to a separate near dark blob. */
        render(510,25000,1.4,1,mirror);
        for(int i=0;i<8;++i)vision_midterm_process(gray,binary,&lane);
        CHECK(lane.roundabout_state==1&&lane.roundabout_evidence==1);
        render(400,25000,1.4,1,mirror);vision_midterm_process(gray,binary,&lane);
        CHECK(!lane.roundabout_evidence&&!lane.entry_guide_rows);
        memset(gray,210,sizeof(gray));vision_midterm_process(gray,binary,&lane);
        CHECK(!lane.valid&&!lane.entry_guide_rows&&!lane.roundabout_evidence);
        vision_midterm_route_enable(0);CHECK(!lane.entry_guide_rows);
    }
    printf("MAP ENTRY: 40 perspective/mirror + 18 approach poses; %u checks, %u failures, %u original current-road fallbacks, %u original guided frames; no orbit claim\n",checks,failures,fallbacks,guided);
    return failures?1:0;
}
