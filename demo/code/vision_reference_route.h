/* C26 A<->B adaptation of newnew/IMG_DEAL.c:
 * LeftStartFind/RightStartFind, left_jump/right_jump, Racing_Line.
 * C26_11: choose a connected current white corridor before pairing boundaries;
 * retain single-boundary half-width centers from the reference approach.
 * Replace its TrackWild[] with this camera's measured widths. Roundabout
 * selection below uses a closed dark island, current white connectivity and
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
    uint8_t island_bottom,rows,path_rejects,island_visible;
} reference_ring_t;
static reference_ring_t reference_ring;
static void reference_roundabout_reset(void)
{
    uint8_t enabled=reference_ring.enabled;
    memset(&reference_ring,0,sizeof(reference_ring));
    reference_ring.enabled=enabled;
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
            center=(l+r)*.5f;
            if(flags!=3&&width_samples[y]>=5) {
                center=flags==1?l+(learned_width[y]-1)*.5f:r-(learned_width[y]-1)*.5f;
            }
            if(reference_ring.enabled&&(reference_ring.state==1||reference_ring.state==2)&&
               width_samples[y]>=5&&(reference_ring.state==2||y<=reference_ring.island_bottom+2)) {
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

#include "vision_route_path.h"
#include "vision_route_junction.h"

/* Coarse dark components. A laterally clipped island also needs visible top
 * and bottom caps, a bulging inner edge, a straight outside main-line edge
 * and connection to the current corridor. A side opening alone is inadequate.
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
        if((border&12)||tail<24||maxx-minx<5||maxy-miny<5||
           maxx-minx>3*(maxy-miny)||maxy-miny>3*(maxx-minx)||
           tail*5U<(unsigned)((maxx-minx+1)*(maxy-miny+1)*2))continue;
        side=(minx+maxx<axis-8)?-1:((minx+maxx>axis+8)?1:0);
        if(!side)continue;
        if(border&&border!=(side<0?1:2))continue;
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
            if(side_rows<6)continue; /* Thin dark connector is not a clipped island. */
            for(y=miny+1;y<maxy;++y)if(reference_ring_lo[y]!=255) {
                float edge=(float)(side<0?reference_ring_hi[y]:reference_ring_lo[y]);
                float line=upper+(lower-upper)*(float)(y-miny)/(maxy-miny);
                float bend=-side*2*(edge-line);if(bend>bulge)bulge=bend;
            }
            if(bulge<8)continue;
            a=outer[first];b=((float)outer[last]-a)/(last-first);
            if(vabs(b)>3)continue;
            for(y=first;y<=last;++y)if(outer[y]!=255&&vabs(outer[y]-(a+b*(y-first)))>4)break;
            if(y<=last)continue;
        }
        if(support>=6&&support>best_support){best_support=support;best_side=side;
            *bottom=VISION_ROI_TOP+2*maxy;*clipped=border!=0;}
    }
    return best_side;
}

/* Unlike a cross, a roundabout exit may open only the OUTER side. Eight
 * current narrow exit rows and white pixels along every intervening modeled
 * road row can establish the straight exit without a two-sided opening. */
static int reference_roundabout_exit_path(const uint8_t *binary,reference_edge_t *left,
                                         reference_edge_t *right,uint8_t *joined,
                                         uint8_t *anchors,int apply)
{
    int y;
    const float axis=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X;
    for(y=82;y>=VISION_ROI_TOP+7;--y) {
        int l[8],r[8],k,row,opened,good=1;
        float center,slope;
        if(!reference_exit_pair(binary,y,axis,&l[0],&r[0]))continue;
        for(k=1;k<8;++k) {
            float at=(l[k-1]+r[k-1])*.5f;
            if(!reference_exit_pair(binary,y-k,at,&l[k],&r[k])||
               vabs((l[k]+r[k]-l[k-1]-r[k-1])*.5f)>3){good=0;break;}
        }
        if(!good)continue;
        center=(l[0]+r[0])*.5f;slope=((l[7]+r[7])*.5f-center)/7;
        if(vabs(slope)>.65f)continue;
        for(row=y+1;row<=VISION_ROI_BOTTOM;++row)
            if(!reference_white_span(binary,row,center-slope*(row-y),&opened)){good=0;break;}
        if(!good)continue;
        if(!apply)return 1;
        for(k=0;k<8;++k){left->col[y-k]=(uint8_t)l[k];right->col[y-k]=(uint8_t)r[k];}
        for(row=y+1;row<=VISION_ROI_BOTTOM;++row) {
            int a=(int)(center-slope*(row-y)-(learned_width[row]-1)*.5f+.5f);
            if(a>=0&&a+learned_width[row]<=VISION_WIDTH){left->col[row]=(uint8_t)a;
                right->col[row]=(uint8_t)(a+learned_width[row]-1);joined[row]=1;}
        }
        *anchors=8;return 1;
    }
    return 0;
}

static void reference_roundabout_observe(const uint8_t *binary,const reference_edge_t *left,
                                        const reference_edge_t *right,const uint8_t *joined,
                                        uint8_t anchors)
{
    int y,side,clipped=0,bottom=VISION_ROI_BOTTOM,normal=0,inner=0,opening=0,joined_n=0;
    float near=0,far=0;int near_n=0,far_n=0;
    if(!reference_ring.enabled){reference_roundabout_reset();return;}
    reference_ring.island_visible=0;
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
        if(width_samples[y]<5||edge>=VISION_WIDTH)continue;
        if(l<VISION_WIDTH&&r<VISION_WIDTH&&width*10>=learned_width[y]*7&&
           width*10<=learned_width[y]*13)++normal;
        if(y>=48&&y<=105) {
            int step=reference_ring.side<0?1:-1;
            int a=edge+step*(learned_width[y]+2),x,n=0,white=0;
            float center=edge+step*(learned_width[y]-1)*.5f;
            /* Opening on the OUTER side, with the inner edge still visible. */
            for(x=0;x<8;++x){int at=a+step*x;
                if(at>=0&&at<VISION_WIDTH){++n;white+=binary[y*VISION_WIDTH+at]!=0;}}
            if(n>=6&&white==n)++opening;
            ++inner;
            if(y>=85){near+=center;++near_n;}
            if(y>=48&&y<=70){far+=center;++far_n;}
        }
    }
    reference_ring.rows=(uint8_t)inner;
    if(reference_ring.state==1) {
        /* The entry mouth must disappear and a sustained real curved corridor
         * replace it before an exit can be considered. This prevents the same
         * tangent opening from being mistaken for an immediate exit. */
        int curved=near_n>=5&&far_n>=5&&
            reference_ring.side*(far/far_n-near/near_n)>6;
        if(normal>=20&&inner>=12&&opening<4&&curved)++reference_ring.clear_votes;
        else reference_ring.clear_votes=0;
        if(reference_ring.clear_votes>=4){reference_ring.state=2;reference_ring.exit_votes=0;}
        /* A false closed mark must not leave permanent inner-edge bias after
         * it passes. A real island entry continues to show same-side curvature;
         * only a sustained ordinary non-curving road cancels an unentered island. */
        if(!side&&normal>=28&&opening<4&&near_n>=5&&far_n>=5&&
           vabs(far/far_n-near/near_n)<3)++reference_ring.cancel_votes;
        else reference_ring.cancel_votes=0;
        if(reference_ring.cancel_votes>=12)reference_roundabout_reset();
    } else if(reference_ring.state==2) {
        /* A new outer opening AND a currently visible forward exit across
         * white are required. newnew Roundabout_Deal likewise uses separate
         * edge-loss/reappearance phases; its fixed columns are not copied. */
        int aligned=near_n>=5&&far_n>=5&&vabs(far/far_n-near/near_n)<4;
        if(aligned&&opening>=6&&inner>=8&&((joined_n>=4&&anchors>=8)||
           reference_roundabout_exit_path(binary,0,0,0,0,0)))++reference_ring.exit_votes;
        else reference_ring.exit_votes=0;
        if(reference_ring.exit_votes>=5){reference_ring.state=3;reference_ring.clear_votes=0;}
        /* Straight tangent views do not cancel a committed ring: doing so
         * skips it at the tangent. Only sustained opposite curvature can
         * reject the island hypothesis without a verified exit event. */
        if(normal>=28&&opening<4&&near_n>=5&&far_n>=5&&
           reference_ring.side*(far/far_n-near/near_n)<-6)++reference_ring.cancel_votes;
        else reference_ring.cancel_votes=0;
        if(reference_ring.cancel_votes>=12)reference_roundabout_reset();
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

static void reference_route(const uint8_t *binary,vision_result_t *out)
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
         * FOLLOW suppresses all such bridges to avoid leaving the circle early. */
        uint8_t saved_joined[VISION_HEIGHT],saved_anchors=out->junction_anchors;
        memcpy(saved_joined,joined,sizeof(saved_joined));
        raw_left=left;raw_right=right;
        reference_white_corridor(binary,&left,&right);
        {int kept=0;
         for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y) {
            if(reference_ring.state==1&&y>reference_ring.island_bottom+2&&joined[y]) {
                left.col[y]=raw_left.col[y];right.col[y]=raw_right.col[y];++kept;
            } else joined[y]=0;
         }
         if(!kept)out->junction_anchors=0;}
        /* Classifier intent cannot destroy a still-visible ordinary road.
         * C22 field losses included ENTER with no selected rows. If a current
         * ordinary corridor survives, defer a bad entry, or reject a stale
         * FOLLOW after three consecutive invalid path proposals. */
        if(!reference_path_viable(binary,&left,&right,joined,out->junction_anchors)&&
            reference_path_viable(binary,&raw_left,&raw_right,saved_joined,saved_anchors)) {
            left=raw_left;right=raw_right;memcpy(joined,saved_joined,sizeof(saved_joined));
            out->junction_anchors=saved_anchors;out->path_fallback=1;
            if(reference_ring.path_rejects<255)++reference_ring.path_rejects;
            if(reference_ring.state==1||reference_ring.path_rejects>=3)reference_roundabout_reset();
        } else reference_ring.path_rejects=0;
    }
    if(reference_ring.state==3)
        reference_roundabout_exit_path(binary,&left,&right,joined,&out->junction_anchors,1);
    out->roundabout_state=reference_ring.state;out->roundabout_side=reference_ring.side;
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
    if(out->valid) {
        for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(out->row_flags[y]==3) {
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
