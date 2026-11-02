#ifndef SMARTCAR_ROUTE_LANE_H
#define SMARTCAR_ROUTE_LANE_H
#include "speed_helpers.h"
static vision_result_t route_lane;
static uint32_t route_sequence;
static inline void set_lane(int shift)
{
    memset(&route_lane,0,sizeof(route_lane));
    route_lane.valid=1;route_lane.status=VISION_TRACK;route_lane.confidence=85;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        int center=93+shift;
        route_lane.left[y]=(uint8_t)(center-40);route_lane.right[y]=(uint8_t)(center+40);
        route_lane.center[y]=(uint8_t)center;route_lane.row_flags[y]=3;
    }
}

#endif
