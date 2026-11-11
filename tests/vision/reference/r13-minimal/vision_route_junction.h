/* Private current-pixel junction geometry, included by vision_reference_route.h. */
#ifndef VISION_ROUTE_JUNCTION_H
#define VISION_ROUTE_JUNCTION_H

typedef struct {float position,slope;uint8_t valid,age;} reference_course_t;
static reference_course_t reference_course;
static void reference_course_reset(void){memset(&reference_course,0,sizeof(reference_course));}
static float reference_course_at(int y)
{
    return reference_course.valid?reference_course.position+
        reference_course.slope*(VISION_ROI_BOTTOM-y):(VISION_WIDTH-1)*VISION_AXIS_NEAR_X;
}
static void reference_course_observe(const reference_edge_t *left,const reference_edge_t *right)
{
    int y,n=0,first=-1,last=-1;float a=0,b=0;
    if(reference_course.age<255)++reference_course.age;
    if(reference_course.age>20)reference_course.valid=0;
    for(y=VISION_ROI_BOTTOM;y>=VISION_ROI_BOTTOM-15;--y) {
        int l=left->col[y],r=right->col[y],w=r-l+1;
        if(l>=VISION_WIDTH||r>=VISION_WIDTH||width_samples[y]<5||
           w*10<learned_width[y]*8||w*10>learned_width[y]*12)break;
        if(first<0){first=y;a=(l+r)*.5f;}
        last=y;b=(l+r)*.5f;++n;
    }
    if(n>=8) {
        float slope=(b-a)/(first-last);
        if(vabs(slope)<=.75f){reference_course.position=a+slope*(first-VISION_ROI_BOTTOM);
            reference_course.slope=slope;reference_course.valid=1;reference_course.age=0;}
    }
}

/* Prefer the current entry tangent at an ordinary crossing. A confirmed
 * island overrides this preference in its own module. No map turn sequence. */
static int reference_exit_pair(const uint8_t *binary,int y,float prediction,int *left,int *right)
{
    int x,best_l=-1,best_r=-1,tied=0;
    float best=1.0e9f;
    const uint8_t *row=binary+y*VISION_WIDTH;
    if(width_samples[y]<5)return 0;
    for(x=0;x<VISION_WIDTH;) {
        int l,r,width;float distance;
        while(x<VISION_WIDTH&&!row[x])++x;l=x;
        while(x<VISION_WIDTH&&row[x])++x;r=x-1;width=r-l+1;
        if(l<2||r>=VISION_WIDTH-2||width<VISION_MIN_WIDTH)continue;
        if(row[l-1]||row[l-2]||row[r+1]||row[r+2])continue;
        if(width*10<learned_width[y]*7||width*10>learned_width[y]*13)continue;
        distance=vabs((l+r)*.5f-prediction);
        if(distance>learned_width[y]*.25f)continue;
        if(distance<best){best=distance;best_l=l;best_r=r;tied=0;}
        else if(distance==best)tied=1;
    }
    if(best_l<0||tied)return 0;
    *left=best_l;*right=best_r;return 1;
}

static int reference_white_span(const uint8_t *binary,int y,float center,int *opened)
{
    const uint8_t *row=binary+y*VISION_WIDTH;
    int l,r,x,n=0,white=0,wing=0,wing_n=0,left_n=0,right_n=0;
    if(width_samples[y]<5)return 0;
    l=(int)(center-(learned_width[y]-1)*.5f+.5f);r=l+learned_width[y]-1;
    for(x=l<0?0:l;x<=r&&x<VISION_WIDTH;++x){++n;white+=row[x]!=0;}
    if(n<VISION_MIN_WIDTH||n*2<learned_width[y]||white*10<n*9)return 0;
    for(x=1;x<=8;++x){
        if(l-x>=0&&l-x<VISION_WIDTH){++wing_n;++left_n;wing+=row[l-x]!=0;}
        if(r+x>=0&&r+x<VISION_WIDTH){++wing_n;++right_n;wing+=row[r+x]!=0;}
    }
    *opened=left_n>=2&&right_n>=2&&wing*10>=wing_n*9;
    /* With the field's wide near road, a modest approach offset can put
     * one modeled margin outside the camera. Requiring BOTH opening wings
     * then loses a still-visible white through path. One visible expanded
     * margin is sufficient only when the central half-width strip is fully
     * in view and CURRENT white. Entry/exit anchors remain mandatory in the
     * callers; this cannot turn an all-white frame into a road by itself. */
    if(!*opened&&(!left_n||!right_n)&&(left_n>=6||right_n>=6)&&wing*10>=wing_n*9) {
        int a=(int)(center-learned_width[y]*.25f+.5f);
        int b=(int)(center+learned_width[y]*.25f+.5f),good=a>=0&&b<VISION_WIDTH;
        if(good)for(x=a;x<=b;++x)if(!row[x]){good=0;break;}
        if(good)*opened=1;
    }
    return 1;
}

static void reference_junction(const uint8_t *binary,reference_edge_t *left,
                               reference_edge_t *right,uint8_t *joined,uint8_t *anchors)
{
    int y,near=-1,support=0,opened=0,open_rows=0;
    float entry=0,slope=0;
    /* Six real, stable entry rows; a current white opening on both sides;
     * six real exit rows in the forward corridor. Missing evidence on either
     * side cannot invent a junction, and a dark gap is never painted over.
     * newnew Cross_col/CrossRow inspire the white-through test; thresholds
     * and learned widths here belong to this camera, not its fixed columns. */
    memset(joined,0,VISION_HEIGHT);
    for(y=VISION_ROI_BOTTOM;y>=56;--y) {
        int l=left->col[y],r=right->col[y],width=r-l+1;
        if(l<VISION_WIDTH&&r<VISION_WIDTH&&width_samples[y]>=5&&
           width*10>=learned_width[y]*7&&width*10<=learned_width[y]*13) {
            float center=(l+r)*.5f;
            if(support&&vabs(center-entry)>4)support=0;
            entry=center;++support;continue;
        }
        if(support>=6&&y+6<=VISION_ROI_BOTTOM) {
            near=y+1;
            slope=(entry-(left->col[y+6]+right->col[y+6])*.5f)/5.0f;
            if(vabs(slope)>.75f||!reference_white_span(binary,y,entry+slope,&opened)||!opened)near=-1;
            break;
        }
        support=0;
    }
    if(near<0)return;
    for(y=near-1;y>=VISION_ROI_TOP+5&&near-y<=56;--y) {
        int l[6],r[6],k,good=1;
        float prediction=entry+slope*(near-y);
        if(open_rows>=4&&reference_exit_pair(binary,y,prediction,&l[0],&r[0])) {
            for(k=1;k<6;++k) {
                float next=(l[k-1]+r[k-1])*.5f+slope;
                if(!reference_exit_pair(binary,y-k,next,&l[k],&r[k])||
                   vabs((l[k]+r[k]-l[k-1]-r[k-1])*.5f)>4){good=0;break;}
            }
            if(good) {
                float exit=(l[0]+r[0])*.5f;
                int row;
                for(row=y+1;row<near;++row) {
                    float center=exit+(entry-exit)*(row-y)/(near-y);
                    if(!reference_white_span(binary,row,center,&opened)){good=0;break;}
                }
                if(!good)return;
                for(k=0;k<6;++k){left->col[y-k]=(uint8_t)l[k];right->col[y-k]=(uint8_t)r[k];}
                for(row=y+1;row<near;++row) {
                    float center=exit+(entry-exit)*(row-y)/(near-y);
                    int a=(int)(center-(learned_width[row]-1)*.5f+.5f);
                    if(a>=0&&a+learned_width[row]<=VISION_WIDTH){
                        left->col[row]=(uint8_t)a;right->col[row]=(uint8_t)(a+learned_width[row]-1);joined[row]=1;}
                }
                /* Keep reading the current real forward corridor above the
                 * exit. No time-based steering or previous-frame replay. */
                for(row=VISION_ROI_TOP;row<=y-6;++row)left->col[row]=right->col[row]=VISION_INVALID_X;
                prediction=(l[5]+r[5])*.5f;
                for(row=y-6;row>=VISION_ROI_TOP;--row) {
                    int a,b;
                    if(!reference_exit_pair(binary,row,prediction,&a,&b))break;
                    left->col[row]=(uint8_t)a;right->col[row]=(uint8_t)b;prediction=(a+b)*.5f;
                }
                *anchors=12;return;
            }
        }
        if(!reference_white_span(binary,y,prediction,&opened))return;
        if(opened)++open_rows;
    }
}

/* Single current edges are legitimate exit evidence once camera width is
 * learned. Require a contiguous white run, never two unrelated edge walks. */
static int reference_exit_edge(const uint8_t *binary,int y,float prediction,
                               int *left,int *right,float *position)
{
    const uint8_t *row=binary+y*VISION_WIDTH;
    int x,best_l=-1,best_r=-1,tied=0;float best=1.0e9f,best_center=0;
    if(width_samples[y]<5)return 0;
    for(x=0;x<VISION_WIDTH;) {
        int l,r,flags,w;float center,distance;
        while(x<VISION_WIDTH&&!row[x])++x;l=x;
        while(x<VISION_WIDTH&&row[x])++x;r=x-1;w=r-l+1;
        if(w<VISION_MIN_WIDTH)continue;
        flags=(l>=2&&!row[l-1]&&!row[l-2])|((r<VISION_WIDTH-2&&!row[r+1]&&!row[r+2])<<1);
        if(!flags)continue;
        if(flags==3){if(w*2<learned_width[y]||w*5>learned_width[y]*8)continue;center=(l+r)*.5f;}
        else center=flags==1?l+(learned_width[y]-1)*.5f:r-(learned_width[y]-1)*.5f;
        distance=vabs(center-prediction);
        if(distance>learned_width[y]*.45f)continue;
        if(distance<best){best=distance;best_l=flags&1?l:VISION_INVALID_X;
            best_r=flags&2?r:VISION_INVALID_X;best_center=center;tied=0;}
        else if(distance==best)tied=1;
    }
    if(best_l<0||tied)return 0;
    *left=best_l;*right=best_r;*position=best_center;return 1;
}

static void reference_junction_exit_only(const uint8_t *binary,reference_edge_t *left,
                                         reference_edge_t *right,uint8_t *joined,uint8_t *anchors)
{
    int y,k,opening_rows=0;
    for(y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP;--y)if(joined[y])return;
    /* Keep ordinary-road frames cheap. A short-entry crossing must not be
     * skipped merely because six bottom rows are still narrow, but without
     * any current expanded white area there is no exit bridge to search. */
    for(y=VISION_ROI_BOTTOM;y>=VISION_ROI_TOP;--y) {
        int opened;
        if(reference_white_span(binary,y,reference_course_at(y),&opened)&&opened&&++opening_rows>=4)break;
    }
    if(opening_rows<4)return;
    /* Once inside a crossing, its entry can leave the image. Eight CURRENT
     * real boundary rows (paired or single) can define a through-white corridor.
     * Require learned geometry and visible white pixels at every intervening
     * row; this is not a timed continuation from an old turn command. */
    for(y=VISION_ROI_BOTTOM-4;y>=VISION_ROI_TOP+7;--y) {
        int l[8],r[8],ys[8],good=1,open_rows=0,row,opened,scan=1,holes=0;
        float pos[8];
        float exit,slope;
        if(!reference_exit_edge(binary,y,reference_course_at(y),&l[0],&r[0],&pos[0]))continue;
        ys[0]=y;
        /* Eight real anchors in at most ten rows. Edge threshold flicker may
         * remove two rows, but every missing row must still be white road. */
        for(k=1;k<8;) {
            row=y-scan++;
            if(row<VISION_ROI_TOP||scan>11){good=0;break;}
            if(!reference_exit_edge(binary,row,pos[k-1],&l[k],&r[k],&pos[k])) {
                if(++holes>2||!reference_white_span(binary,row,pos[k-1],&opened)){good=0;break;}
                continue;
            }
            if(vabs(pos[k]-pos[k-1])>3*(ys[k-1]-row)){good=0;break;}
            ys[k]=row;++k;
        }
        if(!good)continue;
        exit=pos[0];slope=(pos[7]-exit)/(y-ys[7]);
        if(vabs(slope)>1.0f||vabs(exit-reference_course_at(y))>learned_width[y]*.40f)continue;
        /* The remembered tangent ranks exits, it cannot veto a new current
         * corridor after the chassis has changed heading near the junction. */
        for(row=y+1;row<=VISION_ROI_BOTTOM;++row) {
            float center=exit-slope*(row-y);
            if(!reference_white_span(binary,row,center,&opened)){good=0;break;}
            if(opened)++open_rows;
        }
        if(!good||open_rows<4)continue;
        for(k=0;k<8;++k){left->col[ys[k]]=(uint8_t)l[k];right->col[ys[k]]=(uint8_t)r[k];}
        for(row=y+1;row<=VISION_ROI_BOTTOM;++row) {
            float center=exit-slope*(row-y);
            int a=(int)(center-(learned_width[row]-1)*.5f+.5f);
            if(a>=0&&a+learned_width[row]<=VISION_WIDTH){
                left->col[row]=(uint8_t)a;right->col[row]=(uint8_t)(a+learned_width[row]-1);joined[row]=1;}
        }
        for(row=VISION_ROI_TOP;row<ys[7];++row)left->col[row]=right->col[row]=VISION_INVALID_X;
        exit=pos[7];
        for(row=ys[7]-1;row>=VISION_ROI_TOP;--row) {
            int a,b;float center;
            if(!reference_exit_edge(binary,row,exit,&a,&b,&center))break;
            left->col[row]=(uint8_t)a;right->col[row]=(uint8_t)b;exit=center;
        }
        /* A verified current exit refreshes direction while the old entry
         * has left view. History alone never refreshes this lease. */
        reference_course.position=pos[0]-slope*(VISION_ROI_BOTTOM-y);
        reference_course.slope=slope;reference_course.valid=1;reference_course.age=0;
        *anchors=8;return;
    }
}

static void reference_junction_entry_only(const uint8_t *binary,reference_edge_t *left,
                                          reference_edge_t *right,uint8_t *joined,uint8_t *anchors)
{
    int y,support=0,near=-1,top=-1,open_rows=0,opened;
    float center=0,below=0,slope=0;
    for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(joined[y])return;
    /* Field crossing approaches can retain only ONE edge before the far
     * exit is visible. Follow its CURRENT tangent across a wide white opening,
     * rather than demanding twelve two-edge rows that do not exist there.
     * Eight real entry rows and twelve expanded white rows are mandatory. */
    for(y=VISION_ROI_BOTTOM;y>=56;--y) {
        int l=left->col[y],r=right->col[y],width=r-l+1,usable=0;
        float at=0;
        if(width_samples[y]>=5) {
            if(l<VISION_WIDTH&&r<VISION_WIDTH&&width*10>=learned_width[y]*6&&width*10<=learned_width[y]*14)
                {at=(l+r)*.5f;usable=1;}
            else if(l<VISION_WIDTH&&r==VISION_INVALID_X)
                {at=l+(learned_width[y]-1)*.5f;usable=1;}
            else if(r<VISION_WIDTH&&l==VISION_INVALID_X)
                {at=r-(learned_width[y]-1)*.5f;usable=1;}
        }
        if(usable) {
            if(support&&vabs(at-center)>3)support=0;
            if(!support)below=at;
            center=at;++support;continue;
        }
        if(support>=8) {
            near=y+1;slope=(center-below)/(support-1);
            if(vabs(slope)>1.0f)near=-1;
            if(slope>.5f)slope=.5f;if(slope<-.5f)slope=-.5f;
            break;
        }
        support=0;
    }
    if(near<0)return;
    for(y=near-1;y>=VISION_ROI_TOP;--y) {
        float at=center+slope*(near-y);
        if(!reference_white_span(binary,y,at,&opened)){if(y>=48)return;break;}
        if(opened)++open_rows;
        top=y;
    }
    if(open_rows<12||top<0||near-top<16)return;
    for(y=top;y<near;++y) {
        float at=center+slope*(near-y);
        int l=(int)(at-(learned_width[y]-1)*.5f+.5f);
        if(l>=0&&l+learned_width[y]<=VISION_WIDTH){
            left->col[y]=(uint8_t)l;right->col[y]=(uint8_t)(l+learned_width[y]-1);joined[y]=1;}
    }
    *anchors=(uint8_t)(support>255?255:support);
}

/* A widening can leave fewer than eight entry rows before its far exit is
 * visible. Four CURRENT narrow entry rows plus a recently established course
 * and twelve expanded white rows may guide an ALREADY running car. Report
 * four anchors honestly: this result cannot arm or refresh full-valid time. */
static void reference_junction_partial(const uint8_t *binary,reference_edge_t *left,
                                       reference_edge_t *right,uint8_t *joined,
                                       uint8_t *anchors,uint8_t *partial)
{
    int y,k,row,opened,top=-1,open_rows=0;
    if(!reference_course.valid)return;
    for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(joined[y])return;
    for(y=VISION_ROI_BOTTOM;y>=80;--y) {
        int l[4],r[4],good=1;float pos[4],slope;
        if(!reference_exit_edge(binary,y,reference_course_at(y),&l[0],&r[0],&pos[0]))continue;
        for(k=1;k<4;++k)if(!reference_exit_edge(binary,y-k,pos[k-1],&l[k],&r[k],&pos[k])||
            vabs(pos[k]-pos[k-1])>2){good=0;break;}
        if(!good)continue;
        slope=(pos[3]-pos[0])/3.0f;
        if(vabs(slope)>.75f||vabs(slope-reference_course.slope)>.35f)continue;
        open_rows=0;top=-1;
        for(row=y-4;row>=VISION_ROI_TOP&&y-row<=36;--row) {
            float at=pos[3]+slope*(y-3-row);
            if(!reference_white_span(binary,row,at,&opened)){good=0;break;}
            if(opened)++open_rows;
            top=row;
        }
        if(!good||open_rows<12||top<0)continue;
        for(row=top;row<=y-4;++row) {
            float at=pos[3]+slope*(y-3-row);
            int a=(int)(at-(learned_width[row]-1)*.5f+.5f);
            if(a<0||a+learned_width[row]>VISION_WIDTH){good=0;break;}
        }
        if(!good)continue;
        for(k=0;k<4;++k){left->col[y-k]=(uint8_t)l[k];right->col[y-k]=(uint8_t)r[k];}
        for(row=top;row<=y-4;++row) {
            float at=pos[3]+slope*(y-3-row);
            int a=(int)(at-(learned_width[row]-1)*.5f+.5f);
            left->col[row]=(uint8_t)a;right->col[row]=(uint8_t)(a+learned_width[row]-1);joined[row]=1;
        }
        *anchors=4;*partial=1;return;
    }
}

#endif
