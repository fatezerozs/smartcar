/* C26 A<->B adaptation of newnew/IMG_DEAL.c:
 * LeftStartFind/RightStartFind, left_jump/right_jump, Racing_Line.
 * C26_11: choose a connected current white corridor before pairing boundaries;
 * retain single-boundary half-width centers from the reference approach.
 * Replace its TrackWild[] with this camera's measured widths. Roundabout
 * selection below uses current island arcs, white connectivity and
 * edge/opening events, not a sequence of timed steering commands.
 * Included privately by vision_midterm.c, after thresholding. */
#ifndef VISION_REFERENCE_ROUTE_H
#define VISION_REFERENCE_ROUTE_H

typedef struct {
    uint8_t col[VISION_HEIGHT];
} reference_edge_t;

typedef struct {
    uint8_t enabled,state,votes,clear_votes,exit_votes,cancel_votes;
    int8_t side,candidate_side;
    uint8_t island_bottom,rows,path_rejects,island_visible,island_lost_frames;
    uint8_t exit_armed,arc_votes;
    uint8_t mouth_passed,closed_votes;
} reference_ring_t;
static reference_ring_t reference_ring;
/* Only the new far-clipped candidate uses these CURRENT transition anchors.
 * Rebuilt every observation; never a saved path or an inferred boundary. */
static uint8_t reference_ring_top_edge[VISION_HEIGHT];
static void reference_roundabout_reset(void)
{
    uint8_t enabled=reference_ring.enabled;
    memset(&reference_ring,0,sizeof(reference_ring));
    reference_ring.enabled=enabled;
}

/* Confirm area over adjacent rows, rather than rewarding a one-row bright
 * patch. This only ranks initial seeds; the normal connected edge walk still
 * determines every published boundary. */
static float reference_seed_area(const uint8_t *binary,int y,int l,int r)
{
    int count=0,total=0,last_l=l,last_r=r,cap=r-l+1;
    for(int row=y;row>=VISION_ROI_TOP&&y-row<8;--row){
        int best_l=-1,best_r=-1,best=0;
        for(int x=0;x<VISION_WIDTH;){
            int a,b;while(x<VISION_WIDTH&&!binary[row*VISION_WIDTH+x])++x;a=x;
            while(x<VISION_WIDTH&&binary[row*VISION_WIDTH+x])++x;b=x-1;
            if(b-a+1<best||b<last_l||a>last_r)continue;
            best=b-a+1;best_l=a;best_r=b;
        }
        if(best<VISION_MIN_WIDTH)break;
        total+=best>cap?cap:best;++count;last_l=best_l;last_r=best_r;
    }
    return count>=6?(float)total/count:0;
}

/* Select one CURRENT white corridor before pairing its two boundaries.
 * Adjacent rows must overlap the same visible white run. During a confirmed
 * roundabout only, its current inner boundary supplies the learned half width.
 * No map order, replayed pixels or steering schedule enters this choice. */
static void reference_white_corridor(const uint8_t *binary,reference_edge_t *left,reference_edge_t *right)
{
    int y,x,last_y=VISION_ROI_BOTTOM+1,have=0,last_l=0,last_r=VISION_WIDTH-1;
    float position=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X,slope=0;
    memset(left,0,sizeof(*left));memset(right,0,sizeof(*right));
    memset(left->col,VISION_INVALID_X,sizeof(left->col));
    memset(right->col,VISION_INVALID_X,sizeof(right->col));
    for(y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP;--y) {
        const uint8_t *row=binary+y*VISION_WIDTH;
        int gap=last_y-y,best_l=-1,best_r=-1,best_flags=0,tied=0;
        float prediction=position+slope*(have?gap:0),best=1.0e9f,best_center=0;
        if(!have&&y<40)break;
        if(have&&gap>18)break;
        if(prediction<0)prediction=0;
        if(prediction>VISION_WIDTH-1)prediction=VISION_WIDTH-1;
        for(x=0;x<VISION_WIDTH;) {
            int l,r,flags;
            float center,tracking_center,score;
            while(x<VISION_WIDTH&&!row[x])++x;
            if(x==VISION_WIDTH)break;
            l=x;while(x<VISION_WIDTH&&row[x])++x;r=x-1;
            if(r-l+1<VISION_MIN_WIDTH)continue;
            flags=(l>=2&&!row[l-1]&&!row[l-2])|
                ((r<VISION_WIDTH-2&&!row[r+1]&&!row[r+2])<<1);
            if(!flags)continue; /* All-white view supplies no corridor boundary. */
            /* Tiny interior highlights/stripes are not a full road. Apply
             * this before candidate selection so they cannot steal the seed.
             * A clipped single-edge corridor is handled separately below. */
            if(flags==3) {
                if(y>=56&&r-l+1<24)continue;
                if(width_samples[y]>=5&&(r-l+1)*2<learned_width[y])continue;
            }
            if(reference_ring.state==1&&reference_ring.island_visible&&
               reference_ring_top_edge[y]<VISION_WIDTH) {
                int inner=reference_ring.side<0?1:2;
                int at=reference_ring.side<0?l:r;
                if(!(flags&inner)||vabs((float)(at-reference_ring_top_edge[y]))>4)continue;
            }
            center=(l+r)*.5f;
            if(flags!=3&&width_samples[y]>=5) {
                center=flags==1?l+(learned_width[y]-1)*.5f:r-(learned_width[y]-1)*.5f;
            }
            if(reference_ring.enabled&&(reference_ring.state==1||reference_ring.state==2)&&
               width_samples[y]>=5&&(reference_ring.state==2||y<=reference_ring.island_bottom+2)&&
               (reference_ring.state==2||reference_ring_top_edge[VISION_ROI_TOP]>=VISION_WIDTH||
                reference_ring_top_edge[y]<VISION_WIDTH)) {
                int inner=reference_ring.side<0?1:2;
                float ring_center=reference_ring.side<0?l+(learned_width[y]-1)*.5f:r-(learned_width[y]-1)*.5f;
                /* Keep the island on the SAME side throughout circulation.
                 * A wide mouth's opposite edge belongs to the main line; it
                 * must not pull the center out of the island prematurely.
                 * The entire modeled half-width must still be current white. */
                if((flags&inner)&&r-l+1>=learned_width[y]*.8f&&ring_center>=l&&ring_center<=r) {
                    center=ring_center;
                    if(flags!=3||(r-l+1)*10>learned_width[y]*12||
                       (r-l+1)*10<learned_width[y]*8)flags=inner;
                    else center=(l+r)*.5f;
                }
            }
            tracking_center=center;
            if(tracking_center<0)tracking_center=0;
            if(tracking_center>VISION_WIDTH-1)tracking_center=VISION_WIDTH-1;
            if(have) {
                /* Pair only a run connected to the just-observed road strip.
                 * A bounded gap can reconnect real pixels, never fill them. */
                if(l>last_r+2*gap||r<last_l-2*gap)continue;
                if(vabs(tracking_center-prediction)>40.0f)continue;
            }
            score=4.0f*vabs(tracking_center-prediction)-.10f*(r-l+1);
            if(!have&&flags==3){
                float distance=prediction<l?l-prediction:(prediction>r?prediction-r:0);
                float area=reference_seed_area(binary,y,l,r);
                if(distance>40||area<=0)continue;
                /* Prefer substantial nearby white road to a thin centered
                 * reflection/stripe. Distant white areas cannot win by size. */
                score=distance-.8f*area;
            }
            if(score<best){best=score;best_l=l;best_r=r;best_flags=flags;best_center=tracking_center;tied=0;}
            else if(score==best)tied=1;
        }
        if(best_l<0||tied)continue;
        if(have)slope=.5f*slope+.5f*(best_center-position)/gap;
        if(best_flags&1)left->col[y]=(uint8_t)best_l;
        if(best_flags&2)right->col[y]=(uint8_t)best_r;
        position=best_center;last_l=best_l;last_r=best_r;last_y=y;have=1;
    }
    /* Detached white fragments need six supporting rows before either edge
     * can become part of the path. Both boundaries refer to the SAME run. */
    {int top=-1,bottom=-1,n=0,i;
     for(y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP;--y)if(left->col[y]<VISION_WIDTH||right->col[y]<VISION_WIDTH) {
        if(n&&bottom-y>5) {
            if(n<6)for(i=bottom;i<=top;++i)left->col[i]=right->col[i]=VISION_INVALID_X;
            n=0;
        }
        if(!n)top=y;
        bottom=y;++n;
     }
     if(n&&n<6)for(i=bottom;i<=top;++i)left->col[i]=right->col[i]=VISION_INVALID_X;}
}

/* A stopped boot may see a narrow stripe before the car is put on track.
 * Recover a badly mismatched mature model only from a sustained straight run
 * of RAW two-sided measurements, before junction fill can alter those edges.
 * Curves, rings, white fields and inferred/joined rows cannot train it. */
/* Binarizing a smooth white-surface shadow can invent two stable edges.
 * Learn camera width only when both RAW gray boundaries have local contrast;
 * this does not alter current path selection or reuse a previous frame. */
static int reference_width_gray_edges(const uint8_t *gray,int y,int l,int r)
{
    int left=0,right=0;
    if(l<3||r>=VISION_WIDTH-3||r-l+1<VISION_MIN_WIDTH)return 0;
    for(int k=0;k<3;++k){
        left+=(int)gray[y*VISION_WIDTH+l+k]-gray[y*VISION_WIDTH+l-1-k];
        right+=(int)gray[y*VISION_WIDTH+r-k]-gray[y*VISION_WIDTH+r+1+k];
    }
    return left>=3*VISION_MIN_CONTRAST&&right>=3*VISION_MIN_CONTRAST;
}
static uint8_t reference_width_votes;
static float reference_width_last90,reference_width_last_slope,reference_width_last_center;
static void reference_width_refresh(const uint8_t *gray,const reference_edge_t *left,const reference_edge_t *right)
{
    float sy=0,syy=0,sw=0,syw=0,sc=0,syc=0;
    float width_slope,width_base,center_slope,center_base,den,at90;
    int n=0,first=-1,last=-1;
    if(reference_ring.state||width_samples[90]<5){reference_width_votes=0;return;}
    for(int y=VISION_ROI_BOTTOM-40;y<=VISION_ROI_BOTTOM;++y){
        int l=left->col[y],r=right->col[y];
        if(l>=VISION_WIDTH||r>=VISION_WIDTH||r-l+1<VISION_MIN_WIDTH||!reference_width_gray_edges(gray,y,l,r))continue;
        float w=(float)(r-l+1),c=(l+r)*.5f;
        if(first<0)first=y;last=y;++n;sy+=y;syy+=(float)y*y;
        sw+=w;syw+=y*w;sc+=c;syc+=y*c;
    }
    den=n*syy-sy*sy;
    if(n<28||last-first<30||den<=0){reference_width_votes=0;return;}
    width_slope=(n*syw-sy*sw)/den;width_base=(sw-width_slope*sy)/n;
    center_slope=(n*syc-sy*sc)/den;center_base=(sc-center_slope*sy)/n;
    at90=width_base+90*width_slope;
    if(width_slope<.2f||width_slope>3||vabs(center_slope)>.35f||
       vabs(center_base+90*center_slope-(VISION_WIDTH-1)*VISION_AXIS_NEAR_X)>at90*.35f||
       vabs(at90-learned_width[90])<=learned_width[90]*.25f){reference_width_votes=0;return;}
    for(int y=VISION_ROI_BOTTOM-40;y<=VISION_ROI_BOTTOM;++y){
        int l=left->col[y],r=right->col[y];
        if(l>=VISION_WIDTH||r>=VISION_WIDTH||!reference_width_gray_edges(gray,y,l,r))continue;
        if(vabs((r-l+1)-(width_base+y*width_slope))>4||
           vabs((l+r)*.5f-(center_base+y*center_slope))>3){reference_width_votes=0;return;}
    }
    if(!reference_width_votes||vabs(at90-reference_width_last90)>at90*.05f||
       vabs(width_slope-reference_width_last_slope)>.2f||
       vabs(center_base+90*center_slope-reference_width_last_center)>4)reference_width_votes=1;
    else if(reference_width_votes<8)++reference_width_votes;
    reference_width_last90=at90;reference_width_last_slope=width_slope;
    reference_width_last_center=center_base+90*center_slope;
    if(reference_width_votes<8)return;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        int l=left->col[y],r=right->col[y],w=r-l+1;
        float expected=width_base+y*width_slope;
        if(l>=VISION_WIDTH||r>=VISION_WIDTH||w<VISION_MIN_WIDTH||!reference_width_gray_edges(gray,y,l,r)||
           vabs(w-expected)>(expected*.08f>6?expected*.08f:6)||
           vabs((l+r)*.5f-(center_base+y*center_slope))>4)continue;
        learned_width[y]=(uint16_t)w;width_samples[y]=5;width_age[y]=0;
    }
    reference_width_votes=0;
}

#include "vision_route_path.h"
#include "vision_route_junction.h"

/* Coarse dark components. A laterally clipped island needs visible top/bottom
 * caps, a bulging inner edge, a straight outside main-line edge and connection
 * to the current corridor. A far/top-clipped arc instead needs a current lower
 * mouth plus paired near road (newnew Roundabout_Deal READY/GET_IN_READY).
 * A side opening alone is inadequate.
 * Bounded scratch storage is static, never on the camera task's stack. */
#define REFERENCE_RING_W ((VISION_WIDTH+1)/2)
#define REFERENCE_RING_H ((VISION_ROI_BOTTOM-VISION_ROI_TOP)/2+1)
#define REFERENCE_RING_N (REFERENCE_RING_W*REFERENCE_RING_H)
static uint8_t reference_ring_seen[(REFERENCE_RING_N+7)/8];
static uint16_t reference_ring_queue[REFERENCE_RING_N];
static uint8_t reference_ring_lo[REFERENCE_RING_H],reference_ring_hi[REFERENCE_RING_H];
static int reference_ring_dark(const uint8_t *binary,int x,int y)
{
    return !binary[(VISION_ROI_TOP+2*y)*VISION_WIDTH+2*x];
}
static int reference_roundabout_island(const uint8_t *binary,int *bottom,int *clipped,
                                      const reference_edge_t *left,const reference_edge_t *right)
{
    int start,best_side=0,best_support=0;
    const float axis=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X;
    memset(reference_ring_seen,0,sizeof(reference_ring_seen));
    for(start=0;start<REFERENCE_RING_N;++start) {
        unsigned head=0,tail=0;
        int minx=REFERENCE_RING_W,maxx=0,miny=REFERENCE_RING_H,maxy=0,border=0;
        int side,support=0,connected=0,y,first=-1,last=-1;
        uint8_t outer[REFERENCE_RING_H];
        if(reference_ring_seen[start>>3]&(1U<<(start&7)))continue;
        reference_ring_seen[start>>3]|=(uint8_t)(1U<<(start&7));
        if(!reference_ring_dark(binary,start%REFERENCE_RING_W,start/REFERENCE_RING_W))continue;
        memset(reference_ring_lo,255,sizeof(reference_ring_lo));
        memset(reference_ring_hi,0,sizeof(reference_ring_hi));
        reference_ring_queue[tail++]=(uint16_t)start;
        while(head<tail) {
            int item=reference_ring_queue[head++],x=item%REFERENCE_RING_W;
            int row=item/REFERENCE_RING_W,dx,dy;
            if(x<minx)minx=x;if(x>maxx)maxx=x;if(row<miny)miny=row;if(row>maxy)maxy=row;
            if(x<reference_ring_lo[row])reference_ring_lo[row]=(uint8_t)x;
            if(x>reference_ring_hi[row])reference_ring_hi[row]=(uint8_t)x;
            if(!x)border|=1;if(x==REFERENCE_RING_W-1)border|=2;
            if(!row)border|=4;if(row==REFERENCE_RING_H-1)border|=8;
            for(dy=-1;dy<=1;++dy)for(dx=-1;dx<=1;++dx) {
                int nx=x+dx,ny=row+dy,n;
                if(nx<0||nx>=REFERENCE_RING_W||ny<0||ny>=REFERENCE_RING_H)continue;
                n=ny*REFERENCE_RING_W+nx;
                if(reference_ring_seen[n>>3]&(1U<<(n&7)))continue;
                reference_ring_seen[n>>3]|=(uint8_t)(1U<<(n&7));
                if(reference_ring_dark(binary,nx,ny))reference_ring_queue[tail++]=(uint16_t)n;
            }
        }
        if(((border&4)&&(border&8))||((border&8)&&!(reference_ring.state==1&&
           reference_ring.island_lost_frames<=4&&
           reference_ring.island_bottom>=VISION_ROI_BOTTOM-18))||tail<24||maxx-minx<5||maxy-miny<5||
           maxx-minx>3*(maxy-miny)||maxy-miny>3*(maxx-minx)||
           tail*5U<(unsigned)((maxx-minx+1)*(maxy-miny+1)*2))continue;
        side=(minx+maxx<axis-8)?-1:((minx+maxx>axis+8)?1:0);
        if(!side)continue;
        if((border&8)&&side!=reference_ring.side)continue;
        if((border&3)&&(border&3)!=(side<0?1:2))continue;
        memset(outer,255,sizeof(outer));
        /* A closed spot alone is insufficient: the strip on its near side
         * must fit the learned road, with a real opposite dark boundary.
         * This explicitly chooses the island's RIGHT strip for a left island,
         * preserving the direction of the northbound tangent main line. */
        for(y=miny;y<=maxy;++y)if(reference_ring_lo[y]!=255) {
            int row=VISION_ROI_TOP+2*y;
            int edge=side<0?2*reference_ring_hi[y]:2*reference_ring_lo[y];
            int other=side<0?2*reference_ring_lo[y]:2*reference_ring_hi[y];
            int step=side<0?1:-1,x=edge,n=0,other_n=0,inside,outside;
            if(width_samples[row]<5)continue;
            while(x>=0&&x<VISION_WIDTH&&!binary[row*VISION_WIDTH+x])x+=step;
            if(x<0||x>=VISION_WIDTH||vabs((float)(x-edge))>4)continue;
            inside=x;
            while(x>=0&&x<VISION_WIDTH&&binary[row*VISION_WIDTH+x]){++n;x+=step;}
            if(x>=2&&x<VISION_WIDTH-2&&n*10>=learned_width[row]*6&&
               n*10<=learned_width[row]*18) {
                outside=x;
                if(border) {
                    int current=side<0?left->col[row]:right->col[row];
                    if(current<VISION_WIDTH&&vabs((float)(current-inside))<=3)++connected;
                    outer[y]=(uint8_t)outside;if(first<0)first=y;last=y;++support;
                    continue;
                }
                x=other;
                while(x>=0&&x<VISION_WIDTH&&!binary[row*VISION_WIDTH+x])x-=step;
                if(x<0||x>=VISION_WIDTH||vabs((float)(x-other))>4)continue;
                while(x>=0&&x<VISION_WIDTH&&binary[row*VISION_WIDTH+x]){++other_n;x-=step;}
                if(other_n*10>=learned_width[row]*4)++support;
            }
        }
        if(border) {
            float bulge=0,a,b;
            int side_rows=0;
            int upper=side<0?reference_ring_hi[miny]:reference_ring_lo[miny];
            int lower=side<0?reference_ring_hi[maxy]:reference_ring_lo[maxy];
            if(support<8||connected<6||first<0||last-first<7)continue;
            for(y=miny;y<=maxy;++y)if(side<0?reference_ring_lo[y]==0:
                reference_ring_hi[y]==REFERENCE_RING_W-1)++side_rows;
            if((border&3)&&side_rows<6)continue; /* Thin connector is not a clipped island. */
            for(y=miny+1;y<maxy;++y)if(reference_ring_lo[y]!=255) {
                float edge=(float)(side<0?reference_ring_hi[y]:reference_ring_lo[y]);
                float line=upper+(lower-upper)*(float)(y-miny)/(maxy-miny);
                float bend=-side*2*(edge-line);if(bend>bulge)bulge=bend;
            }
            if(bulge<8)continue;
            /* Fit every measured outer-edge row; quantized endpoints alone
             * can reject an otherwise straight tangent main-line boundary. */
            {float sy=0,sx=0,syy=0,syx=0,n=0,den;
             for(y=first;y<=last;++y)if(outer[y]!=255){
                float at=(float)(y-first);
                sy+=at;sx+=outer[y];syy+=at*at;syx+=at*outer[y];++n;}
             den=n*syy-sy*sy;
             if(n<6||den<=0)continue;
             b=(n*syx-sy*sx)/den;a=(sx-b*sy)/n;}
            if(vabs(b)>3)continue;
            for(y=first;y<=last;++y)if(outer[y]!=255&&vabs(outer[y]-(a+b*(y-first)))>4)break;
            if(y<=last)continue;
            if(border&4) {
                int mouth=0,near=0,cap=VISION_ROI_TOP+2*maxy;
                /* Port the reference's one-sided opening/opposite-edge and
                 * current transition-anchor evidence, not its fixed rows or
                 * TrackWild table. An uncapped top component alone is also
                 * possible in an ordinary bend. Below the CURRENT lower tip
                 * require a wide island-side white mouth and the same real
                 * outside edge; the car's near road must still be paired. */
                if(cap>VISION_ROI_BOTTOM-18)continue;
                for(int row=cap+2;row<=cap+9;++row) {
                    int edge=side<0?right->col[row]:left->col[row],x,n=0;
                    float outside=a+b*((row-VISION_ROI_TOP)*.5f-first);
                    if(width_samples[row]<5||edge>=VISION_WIDTH||
                       vabs(edge+(-side)-outside)>4)continue;
                    x=edge;
                    while(x>=0&&x<VISION_WIDTH&&binary[row*VISION_WIDTH+x]){++n;x+=side;}
                    if(n*10>=learned_width[row]*13)++mouth;
                }
                for(int row=VISION_ROI_BOTTOM-7;row<=VISION_ROI_BOTTOM;++row) {
                    int l=left->col[row],r=right->col[row],n=r-l+1;
                    if(width_samples[row]>=5&&l<VISION_WIDTH&&r<VISION_WIDTH&&
                       n*10>=learned_width[row]*8&&n*10<=learned_width[row]*13)++near;
                }
                if(mouth<6||near<6)continue;
            }
        }
        if(support>=6&&support>best_support){best_support=support;best_side=side;
            *bottom=VISION_ROI_TOP+2*maxy;*clipped=border!=0;
            memset(reference_ring_top_edge,VISION_INVALID_X,sizeof(reference_ring_top_edge));
            if(border&4)for(y=miny;y<=maxy;++y)if(reference_ring_lo[y]!=255) {
                int row=VISION_ROI_TOP+2*y;
                int edge=side<0?2*reference_ring_hi[y]+1:2*reference_ring_lo[y]-1;
                if(edge>=0&&edge<VISION_WIDTH) {
                    reference_ring_top_edge[row]=(uint8_t)edge;
                    if(row+1<=VISION_ROI_BOTTOM)reference_ring_top_edge[row+1]=(uint8_t)edge;
                }
            }
        }
    }
    return best_side;
}

/* Unlike a cross, a roundabout exit may open only the OUTER side. Eight
 * current narrow exit rows and white pixels along every intervening modeled
 * road row can establish the straight exit without a two-sided opening. */
#include "vision_ring_bridge.h"
static int reference_roundabout_exit_path(const uint8_t *binary,reference_edge_t *left,
                                         reference_edge_t *right,uint8_t *joined,
                                         uint8_t *anchors,int apply)
{
    int y,near_count=0,curve_ready=0;
    const float axis=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X;
    float near_center=0,near_prediction=axis,near_footprint=axis;
    /* Preserve the CURRENT approach footprint while aiming at an exit which
     * may still be oblique or off-axis. Do not extrapolate its far tangent
     * blindly all the way through the inner island to the chassis. */
    for(y=VISION_ROI_BOTTOM;y>VISION_ROI_BOTTOM-8;--y){
        int a,b;float p;
        if(reference_exit_edge(binary,y,near_prediction,&a,&b,&p)){
            if(!near_count)near_footprint=p;
            near_center+=p;++near_count;near_prediction=p;}
    }
    near_center=near_count>=6?near_center/near_count:axis;
    /* Keep proven far exits first. A raised/downward camera may reveal the
     * real outlet only below row82: try that CURRENT near anchor band only
     * after the old search fails, leaving >=16 rows to connect to the car. */
    for(int region=0;region<2;++region)
    for(y=region?VISION_ROI_BOTTOM-16:82;y>=(region?83:VISION_ROI_TOP+7);--y)
    for(int seed=0;seed<3;++seed) {
        int l[8],r[8],k,row,opened,good=1,plan=-1;
        float pos[8],center,slope;
        /* Restore the proven forward-first candidate order. Island-side
         * tracing owns the ring; a gap bridge must not introduce a second,
         * competing turn preference while its exit is still unqualified. */
        float prediction=axis+(seed==0?0:(seed==1?-.5f:.5f))*learned_width[y];
        if(!reference_exit_edge(binary,y,prediction,&l[0],&r[0],&pos[0]))continue;
        if(vabs(pos[0]-axis)>learned_width[y]*.9f)continue;
        for(k=1;k<8;++k) {
            if(!reference_exit_edge(binary,y-k,pos[k-1],&l[k],&r[k],&pos[k])||
               vabs(pos[k]-pos[k-1])>3){good=0;break;}
        }
        for(k=0;k<8&&good;++k)if(l[k]<VISION_WIDTH&&r[k]<VISION_WIDTH){
            int width=r[k]-l[k]+1;
            if(width*10<learned_width[y-k]*7||width*10>learned_width[y-k]*13)good=0;
        }
        if(!good)continue;
        center=pos[0];slope=(pos[7]-center)/7;
        if(vabs(slope)>1.2f)continue;
        for(int attempt=0;attempt<2;++attempt){
            good=1;
            for(row=y+1;row<=VISION_ROI_BOTTOM;++row){
                float p=attempt?center-slope*(row-y):
                    center+(near_center-center)*(row-y)/(VISION_ROI_BOTTOM-y);
                if(!reference_white_span(binary,row,p,&opened)){good=0;break;}
            }
            if(good){plan=attempt;break;}
        }
        if(plan<0&&near_count>=6){
            if(!curve_ready){reference_ring_bridge_prepare(binary,near_footprint,0,0);curve_ready=1;}
            if(reference_ring_bridge(y,center))plan=2;
        }
        if(plan<0)continue;
        if(!apply)return 1;
        for(k=0;k<8;++k){left->col[y-k]=(uint8_t)l[k];right->col[y-k]=(uint8_t)r[k];}
        for(row=y+1;row<=VISION_ROI_BOTTOM;++row) {
            float p=plan==2?ring_bridge_points[row]:plan?center-slope*(row-y):
                center+(near_center-center)*(row-y)/(VISION_ROI_BOTTOM-y);
            int a=(int)(p-(learned_width[row]-1)*.5f+.5f);
            if(a>=0&&a+learned_width[row]<=VISION_WIDTH){left->col[row]=(uint8_t)a;
                right->col[row]=(uint8_t)(a+learned_width[row]-1);joined[row]=1;}
        }
        /* Only extend the newly selected current exit above its anchors.
         * A residual inward ring branch must not cancel the exit preview. */
        for(row=VISION_ROI_TOP;row<y-7;++row){left->col[row]=right->col[row]=VISION_INVALID_X;joined[row]=0;}
        prediction=pos[7];
        for(row=y-8;row>=VISION_ROI_TOP;--row){
            int a,b;float p;
            if(!reference_exit_edge(binary,row,prediction+slope,&a,&b,&p)||vabs(p-prediction)>4)break;
            left->col[row]=(uint8_t)a;right->col[row]=(uint8_t)b;prediction=p;
        }
        *anchors=8;return 1;
    }
    return 0;
}

static void reference_roundabout_observe(const uint8_t *binary,const reference_edge_t *left,
                                        const reference_edge_t *right,const uint8_t *joined,
                                        uint8_t anchors)
{
    int y,side,clipped=0,bottom=VISION_ROI_BOTTOM,normal=0,inner=0,opening=0,joined_n=0,inner_white=0;
    float near=0,far=0;int near_n=0,far_n=0;
    float arc_near=0,arc_far=0;int arc_near_n=0,arc_far_n=0;
    (void)anchors;
    if(!reference_ring.enabled){reference_roundabout_reset();return;}
    if(reference_ring.island_visible)reference_ring.island_lost_frames=0;
    else if(reference_ring.island_lost_frames<255)++reference_ring.island_lost_frames;
    reference_ring.island_visible=0;
    memset(reference_ring_top_edge,VISION_INVALID_X,sizeof(reference_ring_top_edge));
    side=(reference_ring.state==0||reference_ring.state==1||reference_ring.state==4)?
        reference_roundabout_island(binary,&bottom,&clipped,left,right):0;
    if(reference_ring.state==0) {
        if(side&&side==reference_ring.candidate_side)++reference_ring.votes;
        else {reference_ring.candidate_side=(int8_t)side;reference_ring.votes=side?1:0;}
        if(reference_ring.votes>=(clipped?5:3)){reference_ring.state=1;reference_ring.side=(int8_t)side;
            reference_ring.island_bottom=(uint8_t)bottom;reference_ring.clear_votes=0;reference_ring.island_visible=1;}
        return;
    }
    if(side==reference_ring.side){reference_ring.island_bottom=(uint8_t)bottom;reference_ring.island_visible=1;}
    for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y) {
        int l=left->col[y],r=right->col[y],width=r-l+1;
        int edge=reference_ring.side<0?l:r;
        if(joined[y])++joined_n;
        if(width_samples[y]<5)continue;
        if(l<VISION_WIDTH&&r<VISION_WIDTH&&width*10>=learned_width[y]*7&&
           width*10<=learned_width[y]*13)++normal;
        {int outer_edge=reference_ring.side<0?r:l,opened;
         float outer_center=outer_edge+reference_ring.side*(learned_width[y]-1)*.5f;
         int paired=l<VISION_WIDTH&&r<VISION_WIDTH;
         int plausible=paired?width*10>=learned_width[y]*7&&width*10<=learned_width[y]*13:
             outer_edge<VISION_WIDTH&&reference_white_span(binary,y,outer_center,&opened);
         if(outer_edge<VISION_WIDTH&&plausible){
            float outer=(float)(reference_ring.side<0?r:l);
            float expected=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X-
                reference_ring.side*(learned_width[y]-1)*.5f;
            float bend=reference_ring.side*(outer-expected)/(learned_width[y]*.5f);
            if(y>=90){arc_near+=bend;++arc_near_n;}
            if(y>=60&&y<=82){arc_far+=bend;++arc_far_n;}
         }}
        if(edge>=VISION_WIDTH)continue;
        if(y>=48&&y<=105) {
            int step=reference_ring.side<0?1:-1;
            int a=edge+step*(learned_width[y]+2),x,n=0,white=0;
            float center=edge+step*(learned_width[y]-1)*.5f;
            /* Opening on the OUTER side, with the inner edge still visible. */
            for(x=0;x<8;++x){int at=a+step*x;
                if(at>=0&&at<VISION_WIDTH){++n;white+=binary[y*VISION_WIDTH+at]!=0;}}
            if(n>=6&&white==n)++opening;
            ++inner;
            /* Current body corridor beside the measured island edge. The
             * outside near edge may already be cropped by the real mouth. */
            {int body_l=(int)(center-learned_width[y]*.25f+.5f);
             int body_r=(int)(center+learned_width[y]*.25f+.5f),clear=body_l>=0&&body_r<VISION_WIDTH;
             if(clear)for(int px=body_l;px<=body_r;++px)if(!binary[y*VISION_WIDTH+px]){clear=0;break;}
             if(clear)++inner_white;}
            if(y>=85){near+=center;++near_n;}
            if(y>=48&&y<=70){far+=center;++far_n;}
        }
    }
    reference_ring.rows=(uint8_t)inner;
    /* Translation must not hide a real paired arc: near/far change proves
     * the outer edge bends, and the inner edge corroborates off-center cases.
     * A straight tangent outer edge still cannot commit the ring. */
    int inner_arc=near_n>=5&&far_n>=5&&
        reference_ring.side*(far/far_n-near/near_n)>6;
    int outer_arc=arc_near_n>=8&&arc_far_n>=8&&
        arc_far/arc_far_n-arc_near/arc_near_n>.12f&&
        ((arc_far/arc_far_n>.18f&&arc_near/arc_near_n>-.15f)||inner_arc);
    /* A confirmed island plus a long CURRENT inner arc may establish
     * circulation when the outside NEAR edge is no longer in view. A straight
     * measured tangent outside edge still blocks this alternative. This does
     * NOT arm an exit: paired outside-arc evidence is still required below. */
    int cropped_arc=inner>=24&&inner_white>=16&&
        near_n>=5&&far_n>=5&&opening>=6&&(arc_near_n<8||arc_far_n<8)&&
        reference_ring.side*(far/far_n-near/near_n)>
            (learned_width[90]*.08f>8?learned_width[90]*.08f:8);
    if(reference_ring.state==1) {
        /* A closed mouth with a paired arc, or a currently supported cropped
         * inner arc, establishes FOLLOW. Neither shortcut arms an exit: the
         * separate outside-arc qualification below still rejects a tangent. */
        int curved=outer_arc||cropped_arc;
        if(cropped_arc)reference_ring.island_visible=1;
        if(!reference_ring.island_visible&&reference_ring.island_lost_frames<=4&&normal>=20&&inner>=12&&
           near_n>=5&&far_n>=5&&reference_ring.side*(far/far_n-near/near_n)>6){
            /* Confirmed side, CURRENT inner arc and plausible white corridor. */
            reference_ring.island_visible=1;
        }
        if((normal>=20||cropped_arc)&&inner>=12&&(opening<4||cropped_arc)&&curved)++reference_ring.clear_votes;
        else reference_ring.clear_votes=0;
        if(reference_ring.clear_votes>=4){reference_ring.state=2;reference_ring.exit_votes=0;
            reference_ring.exit_armed=reference_ring.arc_votes=0;
            /* Four paired closed-arc frames have already passed the mouth.
             * A cropped/open inner arc may reach FOLLOW but is still entering. */
            reference_ring.mouth_passed=(uint8_t)(outer_arc&&opening<4&&normal>=20);
            reference_ring.closed_votes=0;}
        /* The user-selected single-circle task stays committed to its side.
         * A competing straight road cannot cancel confirmed ENTER. Current
         * pixels still decide path availability; OFF clears the task. */
    } else if(reference_ring.state==2) {
        /* Inner-edge curvature alone can be the tangent island seen from the
         * straight main line. Require the CURRENT OUTSIDE edge of a plausible
         * paired road to bend islandward too. Only then may an outer opening
         * become an exit. This is geometry, not a fixed driving-time gate. */
        /* Single-mouth circle: first leave the entry opening into a stable
         * closed arc, then accept a NEW opening. The still-open entrance
         * cannot become EXIT just because the outside edge briefly curves. */
        if(!reference_ring.mouth_passed){
            if(outer_arc&&opening<4&&normal>=20&&inner>=12)++reference_ring.closed_votes;
            else reference_ring.closed_votes=0;
            if(reference_ring.closed_votes>=4)reference_ring.mouth_passed=1;
        }
        if(!reference_ring.exit_armed){
            if(outer_arc&&reference_ring.mouth_passed)++reference_ring.arc_votes;else reference_ring.arc_votes=0;
            if(reference_ring.arc_votes>=2)reference_ring.exit_armed=1;
        }
        /* A new outer opening AND a currently visible forward exit across
         * white are required. newnew Roundabout_Deal likewise uses separate
         * edge-loss/reappearance phases; its fixed columns are not copied. */
        if(reference_ring.exit_armed&&opening>=6&&inner>=8&&
           reference_roundabout_exit_path(binary,0,0,0,0,0))++reference_ring.exit_votes;
        else reference_ring.exit_votes=0;
        if(reference_ring.exit_votes>=5){reference_ring.state=3;reference_ring.clear_votes=0;}
        /* Opposite-looking branch geometry is not a completed circle. Keep
         * FOLLOW until the qualified exit/recovery sequence or physical OFF. */
    } else if(reference_ring.state==3) {
        if(normal>=28&&joined_n==0&&opening<4)++reference_ring.clear_votes;
        else reference_ring.clear_votes=0;
        if(reference_ring.clear_votes>=4){reference_ring.state=4;reference_ring.clear_votes=0;}
    } else {
        if(!side&&normal>=28&&joined_n==0)++reference_ring.clear_votes;
        else reference_ring.clear_votes=0;
        if(reference_ring.clear_votes>=8)reference_roundabout_reset();
    }
}

#include "vision_ring_entry.h"

static void reference_route(const uint8_t *gray,const uint8_t *binary,vision_result_t *out)
{
    reference_edge_t left,right,raw_left,raw_right;
    uint8_t joined[VISION_HEIGHT];
    int y,first=-1,last=-1,total=0,seed_y=-1;
    float error=0,weight_sum=0,near_sum=0,far_sum=0;
    int near_n=0,far_n=0;
    /* Observe the ordinary corridor first. Active circulation then reselects
     * a connected current white strip from the island-side measured boundary. */
    {uint8_t state=reference_ring.state;reference_ring.state=0;
     reference_white_corridor(binary,&left,&right);reference_ring.state=state;}
    reference_width_refresh(gray,&left,&right);
    /* Direction memory ranks current white exits; it cannot supply pixels
     * or bypass anchor/white-span checks. Ring arcs do not update this hint. */
    if(!reference_ring.state)reference_course_observe(&left,&right);
    raw_left=left;raw_right=right;
    reference_junction(binary,&left,&right,joined,&out->junction_anchors);
    reference_junction_exit_only(binary,&left,&right,joined,&out->junction_anchors);
    reference_junction_entry_only(binary,&left,&right,joined,&out->junction_anchors);
    reference_junction_partial(binary,&left,&right,joined,&out->junction_anchors,&out->junction_partial);
    reference_roundabout_observe(binary,&raw_left,&raw_right,joined,out->junction_anchors);
    if(reference_ring.state==1||reference_ring.state==2) {
        /* A closed island can be visible BEYOND a nearer cross (the central
         * 270-degree loop). Retain its already validated near white bridge
         * during ENTER. Otherwise a long all-white cross would discard the
         * very current anchors that let us reach the island in the first place.
         * FOLLOW retains this entry bridge until the mouth has passed, then
         * suppresses ordinary bridges to avoid leaving the circle early. */
        uint8_t saved_joined[VISION_HEIGHT],saved_anchors=out->junction_anchors;
        memcpy(saved_joined,joined,sizeof(saved_joined));
        raw_left=left;raw_right=right;
        reference_white_corridor(binary,&left,&right);
        {int kept=0;
         for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y) {
            if((reference_ring.state==1||(reference_ring.state==2&&!reference_ring.mouth_passed))&&
               y>reference_ring.island_bottom+2&&joined[y]) {
                left.col[y]=raw_left.col[y];right.col[y]=raw_right.col[y];++kept;
            } else joined[y]=0;
         }
         if(!kept)out->junction_anchors=0;}
        /* An open ring mouth can remove the near edges before exit votes
         * exist. Connect the current near white footprint to eight real far
         * boundary anchors, retaining FOLLOW and its exit prerequisites.
         * This is a current path, not an implicit EXIT or stored turn. */
        if(reference_ring.state==2&&
           !reference_path_viable(binary,&left,&right,joined,out->junction_anchors)&&
           reference_roundabout_exit_path(binary,&left,&right,joined,&out->junction_anchors,1))
            out->path_fallback=1;
        /* Classifier intent cannot destroy a still-visible ordinary road.
         * C22 field losses included ENTER with no selected rows. If a current
         * ordinary corridor survives, defer a bad entry, or reject a stale
         * FOLLOW after three consecutive invalid path proposals. */
        if(!reference_path_viable(binary,&left,&right,joined,out->junction_anchors)&&
            reference_path_viable(binary,&raw_left,&raw_right,saved_joined,saved_anchors)) {
            left=raw_left;right=raw_right;memcpy(joined,saved_joined,sizeof(saved_joined));
            out->junction_anchors=saved_anchors;out->path_fallback=1;
            if(reference_ring.path_rejects<255)++reference_ring.path_rejects;
            /* Current-road fallback is a path-availability event, not a
             * completed/cancelled ring task. Once FOLLOW is established, keep
             * the side and exit qualification through a white mouth. Every
             * published path still comes from this frame; opposite-curve
             * rejection and OFF/reset continue to cancel the task normally. */
            /* Do not cancel ENTER on a single cropped/hidden island frame.
             * The observer above already requires 12 ordinary non-curving
             * frames to reject ENTER. Fallback publishes only CURRENT road;
             * entry guidance remains gated by CURRENT island evidence. */
        } else reference_ring.path_rejects=0;
    }
    if(reference_ring.state==3)
        reference_roundabout_exit_path(binary,&left,&right,joined,&out->junction_anchors,1);
    out->roundabout_state=reference_ring.state;out->roundabout_side=reference_ring.side;
    out->roundabout_committed=reference_ring.exit_armed;
    out->roundabout_entry_active=(uint8_t)(reference_ring.state==1||
        (reference_ring.state==2&&!reference_ring.mouth_passed));
    out->roundabout_rows=reference_ring.rows;
    for(y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP;--y) {
        int l=left.col[y],r=right.col[y],flags=0,width;
        float center,weight;
        if(l<VISION_WIDTH&&r<VISION_WIDTH) {
            int x,white=0;
            width=r-l+1;
            if(width<VISION_MIN_WIDTH)continue;
            /* fixpoint analogue: both walks must enclose the same white road,
             * rather than unrelated boundaries separated by a dark island. */
            for(x=l;x<=r;++x)white+=binary[y*VISION_WIDTH+x]!=0;
            if(white*10<width*9)continue;
            flags=joined[y]?4:3;
        } else if(learned_width[y]>=VISION_MIN_WIDTH && width_samples[y]>=5) {
            width=learned_width[y];
            if(l<VISION_WIDTH){r=l+width-1;flags=1;}
            else if(r<VISION_WIDTH){l=r-width+1;flags=2;}
            else continue;
            /* Reference Racing_Line uses either measured boundary +/- half
             * road width, including an edge temporarily missing inside view.
             * Require its modeled interior to remain predominantly white. */
            {int x,white=0,n=0;
             for(x=l<0?0:l;x<=r&&x<VISION_WIDTH;++x){++n;white+=binary[y*VISION_WIDTH+x]!=0;}
             if(n<VISION_MIN_WIDTH||white*4<n*3)continue;}
        } else continue;
        center=(l+r)*.5f;
        /* A visible strip at the edge can prove the road is further outside
         * view. Keep its current boundary and modeled width for correction;
         * only the preview coordinate is clipped. Do not discard that turn. */
        out->left[y]=(l>=0&&l<VISION_WIDTH)?(uint8_t)l:VISION_INVALID_X;
        out->right[y]=(r>=0&&r<VISION_WIDTH)?(uint8_t)r:VISION_INVALID_X;
        out->center[y]=(uint8_t)(center<0?0:(center>VISION_WIDTH-1?VISION_WIDTH-1:center+.5f));
        out->row_flags[y]=(uint8_t)flags;
        out->row_width[y]=(uint16_t)width;
        if(flags==3)++out->measured_rows;
        else if(flags==4)++out->junction_rows;
        else ++out->inferred_rows;
        if(first<0)first=y;
        last=y;++total;
        if(seed_y<0)seed_y=y;
        /* Adapt newnew main.c weighted middleline accumulation; emphasize
         * visible bend approach instead of requiring one fixed camera band. */
        weight=y>=60&&y<=90?14.0f:(y>=40?8.0f:2.0f);
        if(flags!=3)weight*=flags==4?.25f:.5f;
        error+=weight*(center-(VISION_WIDTH-1)*.5f);weight_sum+=weight;
        if(y>=80){near_sum+=center;++near_n;}
        if(y>=40&&y<80){far_sum+=center;++far_n;}
    }
    /* This score is current traced-row coverage, not probability. A six-row
     * edge seed alone cannot start the car; require >=25% of the ROI and a
     * vertically extended path reaching at least the middle camera region. */
    out->confidence=(uint8_t)(total*100/(VISION_ROI_BOTTOM-VISION_ROI_TOP+1));
    out->valid=total>=12&&first>=56&&first-last>=16&&weight_sum>0&&
        out->confidence>=VISION_TRACK_MIN_COVERAGE&&(!out->junction_rows||out->junction_anchors>=8);
    out->status=out->valid?VISION_TRACK:VISION_NO_LANE;
    if(width_samples[90]>=5)out->camera_width90=learned_width[90];
    if(out->valid) {
        for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(out->row_flags[y]==3&&
            reference_width_gray_edges(gray,y,out->left[y],out->right[y])) {
            unsigned w=out->row_width[y];
            /* Mature geometry is this camera's straight-road half-width
             * model. Repeated oblique rows in a U bend must not overwrite it
             * before that same bend needs a missing-edge center. */
            if(width_samples[y]>=5)continue;
            if(!learned_width[y]||w*5U<learned_width[y]*4U||w*4U>learned_width[y]*5U)width_samples[y]=1;
            else if(width_samples[y]<5)++width_samples[y];
            learned_width[y]=(uint16_t)w;width_age[y]=0;
        }
        out->raw_error_px=out->error_px=error/weight_sum;
        reference_ring_entry_guide(binary,out,weight_sum);
        if(near_n>=5&&far_n>=5){out->bend_px=far_sum/far_n-near_sum/near_n;out->bend_valid=1;}
        previous_seed=out->center[seed_y];previous_error=out->raw_error_px;
    } else previous_error=0;
    previous_valid=out->valid;
}
#endif
