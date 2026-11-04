#ifndef VISION_RING_BRIDGE_H
#define VISION_RING_BRIDGE_H
/* Current-image connectivity from a measured near footprint to exit anchors.
 * Rebuilt on every call; no stored trajectory is reused on a later frame.
 * Integer centers, <=3px/row, full modeled width >=90% white, central half
 * entirely white. Parent links always lead toward the measured near road. */
static int8_t ring_bridge_parent[VISION_HEIGHT][VISION_WIDTH];
static uint16_t ring_bridge_cost[2][VISION_WIDTH];
static uint8_t ring_bridge_points[VISION_HEIGHT];
static void reference_ring_bridge_prepare(const uint8_t *binary,float near_center)
{
    memset(ring_bridge_parent,127,sizeof(ring_bridge_parent));
    memset(ring_bridge_cost,255,sizeof(ring_bridge_cost));
    for(int y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP+7;--y){
        uint16_t dark[VISION_WIDTH+1];
        unsigned current=(unsigned)y&1U,next=current^1U;
        dark[0]=0;
        for(int x=0;x<VISION_WIDTH;++x)dark[x+1]=(uint16_t)(dark[x]+!binary[y*VISION_WIDTH+x]);
        for(int x=0;x<VISION_WIDTH;++x){
            int width=learned_width[y],l=x-(width-1)/2,r=l+width-1;
            int half=(width+3)/4,best_dx=0;
            unsigned best=65535,penalty;
            ring_bridge_cost[current][x]=65535;
            if(width_samples[y]<5||width<VISION_MIN_WIDTH||l<0||r>=VISION_WIDTH||
               x-half<0||x+half>=VISION_WIDTH||
               (dark[r+1]-dark[l])*10>width||dark[x+half+1]!=dark[x-half])continue;
            penalty=(unsigned)(dark[r+1]-dark[l])*4;
            if(y==VISION_ROI_BOTTOM){
                float offset=vabs(x-near_center);
                if(offset>6)continue;
                best=(unsigned)(offset*2);
            } else for(int dx=-3;dx<=3;++dx){
                int nx=x+dx;
                unsigned cost;
                if(nx<0||nx>=VISION_WIDTH||ring_bridge_cost[next][nx]==65535)continue;
                cost=ring_bridge_cost[next][nx]+(unsigned)(dx*dx);
                if(cost<best){best=cost;best_dx=dx;}
            }
            if(best==65535)continue;
            ring_bridge_cost[current][x]=(uint16_t)(best+penalty);
            ring_bridge_parent[y][x]=(int8_t)best_dx;
        }
    }
}
static int reference_ring_bridge(int y,float center)
{
    int x=(int)(center+.5f);
    if(x<0||x>=VISION_WIDTH||ring_bridge_parent[y][x]==127)return 0;
    for(int row=y;row<=VISION_ROI_BOTTOM;++row){
        ring_bridge_points[row]=(uint8_t)x;
        x+=ring_bridge_parent[row][x];
    }
    return 1;
}
#endif
