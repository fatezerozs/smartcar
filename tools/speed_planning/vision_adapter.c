#include "vision_adapter.h"
#include <float.h>
/* Select a current path coordinate without changing evidence provenance.
 * Modeled rows require route evidence, visible centers and consistent width;
 * full junction anchors and bounded entry guides retain their own semantics. */
int sp_vision_path_sample(const vision_result_t *r,unsigned y,float *out)
{
    float x;
    if(!r||!out)return 0;
    int route=((r->roundabout_state>=1&&r->roundabout_state<=4&&
                   (r->roundabout_side==-1||r->roundabout_side==1))||
                  ((r->roundabout_candidate==-1||r->roundabout_candidate==1)&&r->roundabout_votes));
    if(y>=VISION_HEIGHT||r->center[y]>=VISION_WIDTH)return 0;
    if(r->row_flags[y]==3||(route&&r->row_flags[y]==4&&
        r->junction_rows>=4&&r->junction_anchors>=8&&!r->junction_partial)){
        if(r->left[y]>=r->right[y]||r->right[y]>=VISION_WIDTH||
           r->right[y]-r->left[y]+1<VISION_MIN_WIDTH)return 0;
        x=(r->left[y]+r->right[y])*.5f;
    }else if(route&&(r->row_flags[y]==1||r->row_flags[y]==2)){
        unsigned width=r->row_width[y];
        if(width<VISION_MIN_WIDTH||width>VISION_WIDTH*2U)return 0;
        if(r->row_flags[y]==1){
            if(r->left[y]<2||r->left[y]>=VISION_WIDTH)return 0;
            x=r->left[y]+(width-1)*.5f;
        }else{
            if(r->right[y]>=VISION_WIDTH-2)return 0;
            x=r->right[y]-(width-1)*.5f;
        }
        /* A modeled center outside the image is a return cue for legacy
         * control, not a ground path with visible clearance. */
        if(x<0||x>VISION_WIDTH-1)return 0;
    }else return 0;
    if(r->center[y]-x>1||x-r->center[y]>1)return 0;
    if(route&&r->roundabout_state==1&&r->entry_guide_rows>=12&&
       r->entry_guide[y]!=VISION_INVALID_X){
        float guide=r->entry_guide[y],width=r->row_width[y];
        if(guide>=VISION_WIDTH||width<VISION_MIN_WIDTH||width>VISION_WIDTH*2U||
           guide-x>width*.5f||x-guide>width*.5f||
               (guide-x)*r->roundabout_side< -1)return 0;
        x=guide;
    }
    *out=x;return 1;
}
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
        if((i&&y>=rows[i-1])||!sp_vision_path_sample(r,y,&x))return 0;
        if(map){float z=map[6]*x+map[7]*y+map[8];
            if(!(z>1e-6f||z< -1e-6f))return 0;
            points[i].x=(map[0]*x+map[1]*(float)y+map[2])/z;
            points[i].y=(map[3]*x+map[4]*(float)y+map[5])/z;
        }else{points[i].x=x/(VISION_WIDTH-1);points[i].y=1.0f-(float)y/(VISION_HEIGHT-1);}
    }
    return sp_geometry(h,points,n,map?SP_METRES:SP_NORMALIZED,seq,capture,now,age,step,out);
}
