#include "vision_adapter.h"
#include <float.h>
int sp_vision_preview(sp_geometry_history_t *h,const vision_result_t *r,
    const uint8_t *rows,unsigned n,unsigned min_confidence,const float *map,
    uint32_t seq,uint32_t capture,uint32_t now,uint32_t age,float step,sp_preview_t *out)
{
    sp_point_t points[SP_CAPACITY];unsigned i;
    if(!r||!rows||n<5||n>SP_CAPACITY||min_confidence>100||!r->valid||r->status!=VISION_TRACK||
       r->confidence>100||r->confidence<min_confidence)return 0;
    if(map)for(i=0;i<9;++i)if(!(map[i]>=-FLT_MAX&&map[i]<=FLT_MAX))return 0;
    for(i=0;i<n;++i){
        unsigned y=rows[i];float x;
        if(y>=VISION_HEIGHT||(i&&y>=rows[i-1])||r->row_flags[y]!=3||
           r->left[y]>=r->right[y]||r->right[y]>=VISION_WIDTH||
           r->right[y]-r->left[y]+1<VISION_MIN_WIDTH||r->center[y]>=VISION_WIDTH)return 0;
        x=(r->left[y]+r->right[y])*.5f;
        if(r->center[y]-x>1||x-r->center[y]>1)return 0;
        if(map){float z=map[6]*x+map[7]*y+map[8];
            if(!(z>1e-6f||z< -1e-6f))return 0;
            points[i].x=(map[0]*x+map[1]*(float)y+map[2])/z;
            points[i].y=(map[3]*x+map[4]*(float)y+map[5])/z;
        }else{points[i].x=x/(VISION_WIDTH-1);points[i].y=1.0f-(float)y/(VISION_HEIGHT-1);}
    }
    return sp_geometry(h,points,n,map?SP_METRES:SP_NORMALIZED,seq,capture,now,age,step,out);
}
