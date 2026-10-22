/* Grayscale -> threshold -> public route API. No private state injection. */
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
static vision_result_t lane;
static unsigned checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("FAIL line %d: %s (ring=%u side=%d rows=%u valid=%u)\n",__LINE__,#c,lane.roundabout_state,lane.roundabout_side,lane.roundabout_rows,lane.valid);}}while(0)
static void process(unsigned frames)
{
    while(frames--)vision_midterm_process(gray,binary,&lane);
}
static void road(int side)
{
    int x,y;
    for(y=0;y<VISION_HEIGHT;++y){int center=94+side*(110-y)/2;
        for(x=0;x<VISION_WIDTH;++x)gray[y*VISION_WIDTH+x]=(uint8_t)(x>=center-40&&x<=center+40?210:30);}
}
static void mirror(void)
{
    int x,y;
    for(y=0;y<VISION_HEIGHT;++y)for(x=0;x<VISION_WIDTH/2;++x){
        uint8_t a=gray[y*VISION_WIDTH+x];gray[y*VISION_WIDTH+x]=gray[y*VISION_WIDTH+VISION_WIDTH-1-x];
        gray[y*VISION_WIDTH+VISION_WIDTH-1-x]=a;}
}
static void boot(int enable)
{
    vision_midterm_route_enable(0);vision_midterm_reset();road(0);process(10);
    vision_midterm_route_enable((uint8_t)enable);CHECK(lane.valid);
}
static void island(int side)
{
    int x,y;
    road(0);
    for(y=0;y<VISION_HEIGHT;++y)for(x=0;x<VISION_WIDTH;++x){
        int dx=x-55,dy=y-65;
        if(dx*dx*52*52+dy*dy*64*64<=64*64*52*52)gray[y*VISION_WIDTH+x]=210;
        if(dx*dx*20*20+dy*dy*18*18<=18*18*20*20)gray[y*VISION_WIDTH+x]=30;
    }
    if(side>0)mirror();
}
static void exit_opening(int side)
{
    int x,y;road(0);
    for(y=84;y<=110;++y)for(x=side<0?94:0;x<=(side<0?VISION_WIDTH-1:94);++x)
        gray[y*VISION_WIDTH+x]=210;
}
static void clipped_island(int side)
{
    road(0);
    for(int y=35;y<=95;++y)for(int x=0;x<=100;++x)gray[y*VISION_WIDTH+x]=210;
    for(int y=45;y<=85;++y)for(int x=0;x<=50;++x){int dx=x-10,dy=y-65;
        if(dx*dx*20*20+dy*dy*40*40<=40*40*20*20)gray[y*VISION_WIDTH+x]=30;}
    if(side>0)mirror();
}
int main(void)
{
    int side,x,y;
    /* Neither a two-sided crossing nor an open hairpin dark interior is a
     * closed island; all require C26/route enable before any state advances. */
    boot(1);road(0);
    for(y=40;y<=95;++y)memset(gray+y*VISION_WIDTH,210,VISION_WIDTH);
    process(12);CHECK(lane.roundabout_state==0);
    for(side=-1;side<=1;side+=2){
        boot(1);island(side);
        for(y=62;y<=68;++y)for(x=0;x<=55;++x)
            gray[y*VISION_WIDTH+(side<0?x:VISION_WIDTH-1-x)]=30;
        process(12);CHECK(lane.roundabout_state==0);
    }
    boot(0);island(-1);process(12);CHECK(lane.roundabout_state==0);
    /* Side-clipped dark core, with upper/lower white caps and straight
     * outside main edge. Both mirror directions must still enter. */
    for(side=-1;side<=1;side+=2){
        boot(1);clipped_island(side);
        process(3);CHECK(lane.roundabout_state==0); /* Clipped case needs five votes. */
        process(3);CHECK(lane.roundabout_state==1&&lane.roundabout_side==side&&lane.valid);
        road(side);process(6);CHECK(lane.roundabout_state==2);
        road(-side);process(14);CHECK(lane.roundabout_state==0); /* No stale inner side. */
    }
    for(side=-1;side<=1;side+=2){
        /* Bayesian ENTRY agreement shortens only the clipped confirmation.
         * Opposite hints, absent islands, OFF and consumed hints add no votes. */
        boot(1);clipped_island(side);
        for(int i=0;i<3;++i){vision_midterm_route_hint(side);process(1);}
        CHECK(lane.valid&&lane.roundabout_state==1&&lane.roundabout_side==side);
        boot(1);clipped_island(side);
        for(int i=0;i<3;++i){vision_midterm_route_hint(-side);process(1);}
        CHECK(lane.roundabout_state==0);
        boot(1);clipped_island(side);vision_midterm_route_hint(side);process(3);
        CHECK(lane.roundabout_state==0); /* One hint cannot be replayed. */
        boot(1);road(0);
        for(int i=0;i<8;++i){vision_midterm_route_hint(side);process(1);}
        CHECK(lane.roundabout_state==0);
        boot(0);clipped_island(side);
        for(int i=0;i<8;++i){vision_midterm_route_hint(side);process(1);}
        CHECK(lane.roundabout_state==0);
    }
    vision_midterm_reset();vision_midterm_route_enable(1);island(-1);process(4);
    CHECK(lane.roundabout_state==0); /* No learned geometry, no island arm. */
    /* Central loop: a nearer full-white cross lies BELOW an enclosed dark
     * core. ENTER must retain the currently proven near cross bridge, while
     * its island-side branch bias applies only beyond the core's lower edge. */
    boot(1);road(0);
    for(y=25;y<=60;++y)for(x=0;x<=134;++x)gray[y*VISION_WIDTH+x]=210;
    for(y=25;y<=60;++y)for(x=20;x<=80;++x){int dx=x-55,dy=y-43;
        if(dx*dx*12*12+dy*dy*18*18<=18*18*12*12)gray[y*VISION_WIDTH+x]=30;}
    for(y=70;y<=96;++y)memset(gray+y*VISION_WIDTH,210,VISION_WIDTH);
    process(4);CHECK(lane.roundabout_state==1&&lane.roundabout_side==-1);
    CHECK(lane.valid&&lane.junction_rows>=20&&lane.junction_anchors>=8);
    for(y=70;y<=96;++y)CHECK(lane.row_flags[y]==4&&binary[y*VISION_WIDTH+lane.center[y]]);
    process(20);CHECK(lane.valid&&lane.roundabout_state==1&&lane.junction_rows>=20);
    for(side=-1;side<=1;side+=2){
        boot(1);island(side);process(2);CHECK(lane.roundabout_state==0);
        process(2);CHECK(lane.roundabout_state==1&&lane.roundabout_side==side);
        /* The selected strip is the near-side strip: island stays on left
         * for a left island. Never jump to the opposite white branch. */
        if(side<0)CHECK(lane.left[65]>=72&&lane.left[65]<VISION_WIDTH);
        else CHECK(lane.right[65]<=VISION_WIDTH-1-72);
        for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(lane.row_flags[y])
            CHECK(binary[y*VISION_WIDTH+lane.center[y]]);
        /* Seeing the same tangent opening cannot immediately issue EXIT. */
        exit_opening(side);process(4);CHECK(lane.roundabout_state==1);
        road(side);process(6);CHECK(lane.roundabout_state==2);
        CHECK(lane.roundabout_rows>=12);
        /* A tangent / nearly straight view alone must not abandon the ring. */
        road(0);process(18);CHECK(lane.roundabout_state==2);
        road(side);process(6);CHECK(lane.roundabout_state==2);
        /* One-sided outer mouth with a CURRENT straight narrow exit.
         * No two-sided cross-junction model is needed here. */
        exit_opening(side);process(2);CHECK(lane.roundabout_state==2);
        road(side);process(1);CHECK(lane.roundabout_state==2); /* Flicker resets votes. */
        exit_opening(side);process(4);CHECK(lane.roundabout_state==2);
        process(2);CHECK(lane.roundabout_state==3&&lane.junction_anchors>=8&&lane.valid);
        road(0);process(4);CHECK(lane.roundabout_state==4);
        process(8);CHECK(lane.roundabout_state==0);
        /* OFF clears only route state; camera widths remain usable. */
        island(side);process(4);CHECK(lane.roundabout_state==1);
        vision_midterm_route_enable(0);process(1);CHECK(lane.roundabout_state==0);
        road(0);for(y=0;y<VISION_HEIGHT;++y)for(x=94;x<VISION_WIDTH;++x)
            gray[y*VISION_WIDTH+x]=210;
        process(1);CHECK(lane.valid&&lane.inferred_rows>=20);
        /* A closed false mark that passes onto an ordinary straight cannot
         * hold the island inner-edge bias forever. */
        boot(1);island(side);process(4);CHECK(lane.roundabout_state==1);
        road(0);process(14);CHECK(lane.roundabout_state==0);
    }
    /* Entry preference requires a currently detected island, not retained
     * ENTER alone. Learned boundaries/widths remain ordinary observations. */
    for(side=-1;side<=1;side+=2){
        boot(1);island(side);process(4);
        CHECK(lane.entry_guide_rows>=12&&lane.roundabout_state==1);
        road(0);process(1);CHECK(!lane.entry_guide_rows);
        memset(gray,210,sizeof(gray));process(1);CHECK(!lane.entry_guide_rows&&!lane.valid);
    }
    printf("roundabout: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
