/* Current grayscale, public route API; no private ring-state injection. */
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
static vision_result_t lane;
static unsigned checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("EXIT FAIL %d ring=%u valid=%u anchors=%u\n",__LINE__,lane.roundabout_state,lane.valid,lane.junction_anchors);}}while(0)
static void process(int n){while(n--)vision_midterm_process(gray,binary,&lane);}
static void road(int side)
{
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x){
        int center=94+side*(110-y)/2;
        gray[y*VISION_WIDTH+x]=(uint8_t)(x>=center-40&&x<=center+40?210:30);
    }
}
static void boot(void){vision_midterm_route_enable(0);vision_midterm_reset();road(0);process(10);vision_midterm_route_enable(1);}
static void island(int side)
{
    road(0);
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=0;x<VISION_WIDTH;++x){
        int px=side<0?x:VISION_WIDTH-1-x,dx=px-55,dy=y-65;
        if(dx*dx*52*52+dy*dy*64*64<=64*64*52*52)gray[y*VISION_WIDTH+x]=210;
        if(dx*dx*20*20+dy*dy*18*18<=18*18*20*20)gray[y*VISION_WIDTH+x]=30;
    }
}
static void exit_road(int side,int shift,int gap)
{
    for(int y=0;y<VISION_HEIGHT;++y){
        int offset=y<=82?shift+82-y:shift*(110-y)/28;
        int center=94-side*offset;
        for(int x=0;x<VISION_WIDTH;++x){
            int white=x>=center-40&&x<=center+40;
            if(y>=84&&(side<0?x>=center:x<=center))white=1;
            gray[y*VISION_WIDTH+x]=(uint8_t)(white&&(!gap||y!=96)?210:30);
        }
    }
}
static void curved_exit(int side,int gap)
{
    for(int y=0;y<VISION_HEIGHT;++y){
        float t=(float)(y-82)/28;
        int offset=y<=82?32+82-y:(int)(32*(1-t)+60*t*(1-t));
        int center=94-side*offset;
        for(int x=0;x<VISION_WIDTH;++x){
            int white=x>=center-40&&x<=center+40;
            if(y>=84&&y<=94&&(side<0?x>=center:x<=center))white=1;
            gray[y*VISION_WIDTH+x]=(uint8_t)(white&&(!gap||y!=100)?210:30);
        }
    }
}
int main(void)
{
    /* The exit is connected by a bend, not either of the two straight
     * chords used by WC5. A dark cut must still reject that connection. */
    for(int side=-1;side<=1;side+=2){
        boot();island(side);process(4);road(side);process(6);
        CHECK(lane.roundabout_state==2&&lane.roundabout_committed);
        curved_exit(side,1);process(6);CHECK(lane.roundabout_state!=3);
        boot();island(side);process(4);road(side);process(6);
        curved_exit(side,0);process(6);
        CHECK(lane.roundabout_state==3&&lane.valid&&lane.junction_anchors>=8);
        for(int y=84;y<=110;++y)if(lane.row_flags[y]==4)
            CHECK(binary[y*VISION_WIDTH+lane.center[y]]);
    }
    /* Wide mouth: only distant CURRENT inner/exit edges remain. The exit
     * bridge must survive while five EXIT votes accumulate; three invalid
     * inner-only proposals must not reset the ring before that can happen. */
    for(int side=-1;side<=1;side+=2){
        boot();island(side);process(4);road(side);process(6);
        CHECK(lane.roundabout_state==2);
        road(0);
        for(int y=48;y<=59;++y)for(int x=0;x<VISION_WIDTH;++x)
            if(side<0?x>=94:x<=94)gray[y*VISION_WIDTH+x]=210;
        for(int y=60;y<=110;++y)memset(gray+y*VISION_WIDTH,210,VISION_WIDTH);
        process(4);CHECK(lane.roundabout_state==2&&lane.valid&&lane.junction_anchors>=8);
        process(2);CHECK(lane.roundabout_state==3&&lane.valid);
    }
    for(int side=-1;side<=1;side+=2)for(int shift=32;shift<=60;shift+=28){
        boot();exit_road(side,shift,0);process(8);CHECK(!lane.roundabout_state);
        boot();island(side);process(4);CHECK(lane.roundabout_state==1);
        road(side);process(6);CHECK(lane.roundabout_state==2);
        exit_road(side,shift,1);process(8);CHECK(lane.roundabout_state==2);
        exit_road(side,shift,0);process(4);CHECK(lane.roundabout_state==2);
        process(2);CHECK(lane.roundabout_state==3&&lane.valid&&lane.junction_anchors>=8);
        CHECK(-side*((int)lane.center[82]-94)>20);
        if(shift==60)CHECK(lane.row_flags[75]==(side<0?1:2));
        for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)
            if(lane.row_flags[y]==4)CHECK(binary[y*VISION_WIDTH+lane.center[y]]);
        road(0);process(14);CHECK(lane.roundabout_state==0&&lane.valid);
    }
    printf("RING EXIT: 8 curved/wide-mouth/oblique/clipped mirror cases; %u checks, %u failures; no physical orbit claim\n",checks,failures);
    return failures?1:0;
}
