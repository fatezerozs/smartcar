/* Map geometry diagnostic, not a vehicle dynamics / camera calibration test.
 * Northbound tangent road x906, island center839/237, R67, road halfwidth30.
 * The fixed pose deliberately ignores steering: only entry and direction are
 * asserted. Completing a real orbit cannot be inferred from this replay. */
#include "vision_midterm.h"
#include <math.h>
#include <stdio.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
static vision_result_t lane;
static unsigned checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("projection FAIL line %d\n",__LINE__);}}while(0)
static void render(double cy,double depth,double fov,int ring,int mirror)
{
    for(int y=0;y<VISION_HEIGHT;++y){
        double scale=fov*(40.0+.75*y)/60.0,z=depth/(y+53.333333);
        for(int x=0;x<VISION_WIDTH;++x){
            int pixel=mirror?VISION_WIDTH-1-x:x;
            double wx=906.0+(pixel-94.0)/scale,wy=cy-z;
            double d=hypot(wx-839.0,wy-237.0);
            int white=fabs(wx-906.0)<=30.0||(ring&&fabs(d-67.0)<=30.0);
            gray[y*VISION_WIDTH+x]=(uint8_t)(white?210:30);
        }
    }
}
int main(void)
{
    for(int mirror=0;mirror<2;++mirror)for(int sf=7;sf<=12;++sf)
    for(int depth=20000;depth<=30000;depth+=10000){
        int entered=0,side=mirror?1:-1;float aligned=0;
        vision_midterm_route_enable(0);vision_midterm_reset();
        render(1000,depth,sf*.1,0,mirror);
        for(int i=0;i<12;++i)vision_midterm_process(gray,binary,&lane);
        CHECK(lane.valid);vision_midterm_route_enable(1);
        for(int cy=650;cy>=150;cy-=4){
            render(cy,depth,sf*.1,1,mirror);
            for(int i=0;i<4;++i)vision_midterm_process(gray,binary,&lane);
            if(lane.roundabout_state){entered=1;CHECK(lane.roundabout_side==side);
                if(side*lane.error_px>aligned)aligned=side*lane.error_px;}
        }
        CHECK(entered);CHECK(aligned>3);
    }
    printf("MAP ENTRY: 24 perspective/mirror cases; %u checks, %u failures; no orbit claim\n",checks,failures);
    return failures?1:0;
}
