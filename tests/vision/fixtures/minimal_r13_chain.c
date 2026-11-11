/* Same current road and encoder samples through both complete public drive
 * chains. Hardware is simulated; this is no vehicle-dynamics acceptance. */
#include "../..//support/fixtures/switch_io.h"
#include "vision_drive_internal.h"
#include <math.h>
static void road(unsigned scenario,unsigned frame)
{
    int side=scenario&1?1:-1;
    set_lane(0);memset(route_lane.entry_guide,255,sizeof(route_lane.entry_guide));
    route_lane.measured_rows=VISION_ROI_BOTTOM-VISION_ROI_TOP+1;
    route_lane.camera_width90=scenario>=16?(scenario<20?60:144):0;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        int offset=scenario<2?0:scenario<4?side*(110-y)/3:
            scenario<6?side*(frame<100?(110-y)/2:-(110-y)/2):side*3;
        int center=93+offset,width=scenario>=20?140:80;
        route_lane.left[y]=(uint8_t)(center-width/2);
        route_lane.right[y]=(uint8_t)(center+width/2);
        route_lane.center[y]=(uint8_t)center;route_lane.row_width[y]=(uint16_t)(width+1);
    }
    if(scenario==6||scenario==7){
        unsigned paired=frame<80?28:frame<120?24:frame<160?23:16;
        route_lane.measured_rows=(uint8_t)paired;
        route_lane.inferred_rows=(uint8_t)(VISION_ROI_BOTTOM-VISION_ROI_TOP+1-paired);
        for(int y=VISION_ROI_TOP+paired;y<=VISION_ROI_BOTTOM;++y){
            route_lane.row_flags[y]=(uint8_t)(side<0?1:2);
            if(side<0)route_lane.right[y]=255;else route_lane.left[y]=255;
        }
    }
    if(scenario>=8&&scenario<12&&frame>=100){
        route_lane.valid=0;route_lane.status=VISION_NO_LANE;route_lane.confidence=15;
        if(scenario<10){
            for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)
                if(y<70||y>79)route_lane.row_flags[y]=0;
        }else memset(route_lane.row_flags,0,sizeof(route_lane.row_flags));
    }
    if(scenario>=12&&scenario<16){
        route_lane.roundabout_state=(uint8_t)(scenario-11);
        route_lane.roundabout_side=(int8_t)side;
        route_lane.roundabout_committed=(uint8_t)(scenario==13||scenario==14);
        route_lane.roundabout_entry_active=(uint8_t)(scenario==12);
        route_lane.entry_path_valid=(uint8_t)(scenario==12);
        if(scenario==12){
            for(int y=55;y<=100;++y)route_lane.entry_guide[y]=(uint8_t)(route_lane.center[y]+8);
            route_lane.entry_guide_rows=46;
        }
    }
    if(scenario==16||scenario==17)route_lane.confidence=frame<100?85:55;
    if(scenario>=26){
        set_lane(0);route_lane.confidence=30; /* R13 categorical base mode. */
        for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)route_lane.row_width[y]=81;
    }
}
int main(void)
{
    for(unsigned scenario=0;scenario<28;++scenario){
        switch_boot(0);
        for(unsigned frame=0;frame<(scenario==24||scenario==25?1600U:280U);++frame){
            road(scenario==24||scenario==25?scenario-24:scenario,frame);
            switch_delta_left=scenario==24||scenario==25?-1:-(int)(1+(frame*7+scenario)%30);
            switch_delta_right=scenario==24||scenario==25?1:(int)(1+(frame*11+scenario)%30);
            switch_step(1);
#ifdef SUBJECT
            /* Drain the foreground preparation for this exact sample. R13
             * performs this preparation inside its output call. */
            vd_speed_control_poll(now);
#endif
            vd_output_apply();
            vd_speed_info_t s=info();const vc_output_t *c=vision_drive_output();
            printf("%u %u %u %u %u %u %u %u %u %.7f %.7f %.7f %.7f %d %d %u %u %u %u %u %u\n",
                scenario,frame,c->state,c->faults,c->observation_valid,c->far_valid,
                c->recovering,c->search_source,c->speed_boost,
                (double)c->lateral,(double)c->heading,(double)c->steering,(double)c->duty,
                s.target_right,s.target_left,s.pwm_right,s.pwm_left,s.faults,
                (unsigned)pwm[0],(unsigned)pwm[1],vision_drive_servo_command_us());
        }
        vision_drive_stop();CHECK(!pwm[0]&&!pwm[1]);
    }
    return 0;
}
