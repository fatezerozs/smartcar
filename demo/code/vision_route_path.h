#ifndef VISION_ROUTE_PATH_H
#define VISION_ROUTE_PATH_H
/* Private geometric viability check for a CURRENT path proposal. It mirrors
 * publication's white-pixel checks; no previous-frame pixels or time lease. */
static int reference_path_viable(const uint8_t *binary,const reference_edge_t *left,
                                 const reference_edge_t *right,const uint8_t *joined,
                                 unsigned anchors)
{
    int count=0,near=0,first=-1,last=-1;
    for(int y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP;--y) {
        int l=left->col[y],r=right->col[y],single=0;
        if(l<VISION_WIDTH&&r<VISION_WIDTH) {
            if(r-l+1<VISION_MIN_WIDTH)continue;
            if(joined[y]&&anchors<8)return 0;
        } else {
            if(width_samples[y]<5||learned_width[y]<VISION_MIN_WIDTH)continue;
            if(l<VISION_WIDTH)r=l+learned_width[y]-1;
            else if(r<VISION_WIDTH)l=r-learned_width[y]+1;
            else continue;
            single=1;
        }
        int white=0,n=0;
        for(int x=l<0?0:l;x<=r&&x<VISION_WIDTH;++x){++n;white+=binary[y*VISION_WIDTH+x]!=0;}
        if(n<VISION_MIN_WIDTH||(single?white*4<n*3:white*10<n*9))continue;
        if(first<0)first=y;last=y;++count;if(y>=80)++near;
    }
    return first>=56&&first-last>=16&&near>=3&&
        count*100/(VISION_ROI_BOTTOM-VISION_ROI_TOP+1)>=VISION_TRACK_MIN_COVERAGE;
}
#endif
