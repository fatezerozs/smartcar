/* A tangent inner edge can curve while the outside MAIN LINE stays straight.
 * It must not authorize an EXIT before a genuine paired island arc. */
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
static vision_result_t lane;
static unsigned checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("COMMIT FAIL %d ring=%u guide=%u\n",__LINE__,lane.roundabout_state,lane.entry_guide_rows);}}while(0)
static void process(int n){while(n--)vision_midterm_process(gray,binary,&lane);}
static void road(int side)
{
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x){int center=94+side*(110-y)/2;
        gray[y*VISION_WIDTH+x]=(uint8_t)(x>=center-40&&x<=center+40?210:30);}
}
static void island(int side)
{
    road(0);
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x){
        int px=side<0?x:VISION_WIDTH-1-x,dx=px-55,dy=y-65;
        if(dx*dx*52*52+dy*dy*64*64<=64*64*52*52)gray[y*VISION_WIDTH+x]=210;
        if(dx*dx*20*20+dy*dy*18*18<=18*18*20*20)gray[y*VISION_WIDTH+x]=30;
    }
}
static void tangent(int side)
{
    for(int y=0;y<VISION_HEIGHT;++y){int bend=y<=82?8:(y>=90?0:(90-y));
        for(int x=0;x<VISION_WIDTH;++x){int px=side<0?x:VISION_WIDTH-1-x;
            gray[y*VISION_WIDTH+x]=(uint8_t)(px>=54-bend&&px<=134?210:30);}
    }
}
static void opening(int side)
{
    road(0);
    for(int y=84;y<=110;++y)for(int x=0;x<VISION_WIDTH;++x)
        if(side<0?x>=94:x<=94)gray[y*VISION_WIDTH+x]=210;
}
static void wide_road(int side)
{
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x){int center=94+side*(110-y);
        gray[y*VISION_WIDTH+x]=(uint8_t)(x>=center-72&&x<=center+72?210:30);}
}
int main(void)
{
    for(int side=-1;side<=1;side+=2){
        vision_midterm_route_enable(0);vision_midterm_reset();road(0);process(10);
        vision_midterm_route_enable(1);island(side);process(4);CHECK(lane.roundabout_state==1);
        tangent(side);process(6);CHECK(lane.roundabout_state==2);
        opening(side);process(8);CHECK(lane.roundabout_state==2); /* Entry is not exit. */
        island(side);process(1);CHECK(lane.roundabout_state==2&&!lane.entry_guide_rows);
        road(side);process(6);CHECK(lane.roundabout_state==2);
        opening(side);process(6);CHECK(lane.roundabout_state==3&&lane.valid);
        /* Wide view: the inner boundary is cropped, but a measured outside
         * arc with CURRENT white clearance still proves circulation. */
        vision_midterm_route_enable(0);vision_midterm_reset();wide_road(0);process(10);
        vision_midterm_route_enable(1);
        for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x){
            int px=side<0?x:VISION_WIDTH-1-x;
            int white=px>=22&&px<=166;
            if(y>=35&&y<=95&&px<=100)white=1;
            int dx=px+10,dy=y-65;
            if(dx*dx*20*20+dy*dy*60*60<=60*60*20*20)white=0;
            gray[y*VISION_WIDTH+x]=(uint8_t)(white?210:30);
        }
        process(6);CHECK(lane.roundabout_state==1);
        wide_road(side);process(8);
        CHECK(lane.roundabout_state==2&&lane.roundabout_committed&&lane.inferred_rows>0);
        wide_road(0);
        for(int y=84;y<=110;++y)for(int x=0;x<VISION_WIDTH;++x)
            if(side<0?x>=94:x<=94)gray[y*VISION_WIDTH+x]=210;
        process(6);CHECK(lane.roundabout_state==3&&lane.valid);
    }
    printf("RING COMMIT: %u checks, %u failures; measured outer arc required before EXIT\n",checks,failures);
    return failures?1:0;
}
