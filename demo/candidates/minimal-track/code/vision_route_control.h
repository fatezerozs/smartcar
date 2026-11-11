#ifndef VISION_ROUTE_CONTROL_PRIVATE_H
#define VISION_ROUTE_CONTROL_PRIVATE_H
/* Private implementation of the existing vc_* interface. Ordinary preview,
 * and current-road recovery use the source-pinned R13 baseline. Demand keeps
 * the maintained continuous support weights rather than R13's hard classes.
 * No scheduler, metric geometry, shared vision or PI implementation lives here.
 * Included only by vision_control.c after its numeric helpers. */
static int band_error(const vision_result_t *r, const vc_config_t *c, float band_y, float *error, float *center_x)
{
    float sum = 0, center_sum = 0;
    int n = 0, y;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) {
        float yf = (float)y / (VISION_HEIGHT - 1), axis, half_width, center;
        if (absolute(yf - band_y) > c->band_half_height) continue;
        if(r->row_flags[y]==3
#if VISION_ROUTE_SWITCH
           || (r->row_flags[y]==4&&r->junction_rows>=4&&
               (r->junction_anchors>=8||(r->junction_partial&&r->junction_anchors>=4)))
#endif
          ) {
            if(r->left[y]>=r->right[y] || r->right[y]>=VISION_WIDTH ||
               r->right[y]-r->left[y]+1<VISION_MIN_WIDTH)continue;
            center=(r->left[y]+r->right[y])*0.5f;
            half_width=(r->right[y]-r->left[y]+1)*0.5f;
            if(r->center[y]>=VISION_WIDTH || absolute(r->center[y]-center)>1)continue;
        }
#if VISION_ROUTE_SWITCH
        else if((r->row_flags[y]==1 || r->row_flags[y]==2) &&
                r->row_width[y]>=VISION_MIN_WIDTH && r->row_width[y]<=VISION_WIDTH*2U &&
                r->center[y]<VISION_WIDTH) {
            float predicted;
            half_width=r->row_width[y]*0.5f;center=r->center[y];
            if(r->row_flags[y]==1){
                if(r->left[y]<2 || r->left[y]>=VISION_WIDTH)continue;
                predicted=r->left[y]+(r->row_width[y]-1)*0.5f;
            } else {
                if(r->right[y]>=VISION_WIDTH-2)continue;
                predicted=r->right[y]-(r->row_width[y]-1)*0.5f;
            }
            /* center[] is a drawable image coordinate. An actual measured
             * edge may place the modeled lane center outside view: preserve
             * that signed error instead of dropping the only return cue. */
            if(absolute(center-clamp(predicted,0,VISION_WIDTH-1))>1)continue;
            center=predicted;
        }
#endif
        else continue;
#if VISION_ROUTE_SWITCH
        if(r->roundabout_state==1&&r->entry_guide_rows>=12&&r->entry_guide[y]<VISION_WIDTH)
            center=r->entry_guide[y];
#endif
        axis = c->axis_far_x + (c->axis_near_x - c->axis_far_x) *
               (yf - c->far_y) / (c->near_y - c->far_y);
        sum += (center - axis * (VISION_WIDTH - 1)) / half_width;
        center_sum += center / (VISION_WIDTH - 1);
        ++n;
    }
    if (n < c->min_band_rows) return 0;
    *error = sum / n;
    *center_x = center_sum / n;
    return finite_value(*error);
}

static int control_band_error(const vision_result_t *r,const vc_config_t *c,float y,float *error,float *center)
{
    if(band_error(r,c,y,error,center))return 1;
#if VISION_ROUTE_SWITCH
    /* Current-frame BOTH edges only. Expand around the configured sample,
     * bounded so near/far search regions cannot overlap. No history guessing. */
    {
        vc_config_t wide=*c;
        wide.band_half_height=clamp((c->near_y-c->far_y)*0.45f,0,0.075f);
        if(wide.band_half_height>c->band_half_height && band_error(r,&wide,y,error,center))return 1;
        /* Racing_Line can remain visible above the fixed near band in a bend.
         * Sample the closest actual traced strip, never an old frame center. */
        if(y==c->near_y) {
            float closest=2.0f;int row=-1,i;
            for(i=56;i<=VISION_ROI_BOTTOM;++i)if(r->row_flags[i]) {
                float d=absolute((float)i/(VISION_HEIGHT-1)-y);
                if(d<closest){closest=d;row=i;}
            }
            if(row>=0){wide.band_half_height=.06f;
                return band_error(r,&wide,(float)row/(VISION_HEIGHT-1),error,center);}
        }
    }
#endif
    return 0;
}

#if VISION_ROUTE_SWITCH
static int current_partial_direction(const vision_result_t *r,const vc_config_t *c,float *target,int *strong,int *paired)
{
    vc_config_t strip=*c;
    float sum=0,weights=0;
    int n=0,y,first=-1,last=-1;
    *strong=*paired=0;
    if(r->status==VISION_LOW_CONTRAST||(r->status==VISION_TRACK&&!r->valid))return 0;
    strip.band_half_height=.001f;strip.min_band_rows=1;
    for(y=56;y<=100;++y) {
        float e,x,w=y>=80?2.0f:1.0f;
        if(r->junction_partial&&r->row_flags[y]!=4)continue;
        if(!band_error(r,&strip,(float)y/(VISION_HEIGHT-1),&e,&x))continue;
        if(r->row_flags[y]!=3)w*=.5f;
        else ++*paired;
        if(first<0)first=y;last=y;
        sum+=w*e;weights+=w;++n;
    }
    if(n<6||weights<=0)return 0;
    *strong=n>=8&&last-first>=7;
    *target=clamp(c->kp*sum/weights,-1,1);
    return finite_value(*target);
}

/* R13 ordinary-road recovery: a long CURRENT corridor can still guide
 * the chassis when the fixed near band is absent. Ring task rules remain
 * owned by the existing vision implementation, not this legacy policy. */
static int current_near_corridor(const vision_result_t *r,const vc_config_t *c,float *target)
{
    vc_config_t strip=*c;
    int start=(int)(c->near_y*(VISION_HEIGHT-1))-12;
    int boundary=(int)((c->near_y+c->far_y)*.5f*(VISION_HEIGHT-1))+1;
    int n=0,chain=0,longest=0,last=-1,first=-1;float sum=0;
    if(r->status==VISION_LOW_CONTRAST||r->junction_partial||
       (r->status==VISION_TRACK&&!r->valid))return 0;
    if(start<boundary)start=boundary;
    strip.band_half_height=.001f;strip.min_band_rows=1;
    for(int y=start;y<=VISION_ROI_BOTTOM;++y){
        float e,x;
        if(r->row_flags[y]<1||r->row_flags[y]>3||
           !band_error(r,&strip,(float)y/(VISION_HEIGHT-1),&e,&x)||absolute(e)>.75f){chain=0;continue;}
        if(first<0)first=y;last=y;++n;++chain;sum+=e;if(chain>longest)longest=chain;
    }
    if(n<12||longest<10||last-first<12||last<c->near_y*(VISION_HEIGHT-1)-3)return 0;
    *target=clamp(c->kp*sum/n,-1,1);return 1;
}

static int reference_preview_error(const vision_result_t *r,const vc_config_t *c,float *error,float *center)
{
    /* newnew main.c ImgDeal_GetMidError: weighted available middleline rows.
     * Validate each row through the same measured/model consistency checks.
     * Far bends contribute before the near center starts moving. */
    float sum[2]={0,0},centers[2]={0,0},weights[2]={0,0},distant_scale=1.0f;
    vc_config_t strip=*c;
    int y,n=0,close_n=0,restrict_preview=vj_restrict_preview(r);
    /* R13 moves the ordinary preview with the configured camera far band.
     * Do not add/modify shared vision metadata for legacy-only refactoring. */
    int shift=(int)((c->far_y-.62f)*(VISION_HEIGHT-1)+.5f);
    int limit=(int)((c->near_y-c->band_half_height)*(VISION_HEIGHT-1))-4;
    int bottom=88+shift,top=40+shift,split=65+shift;
    if(bottom>limit)bottom=limit;
    if(bottom>VISION_ROI_BOTTOM)bottom=VISION_ROI_BOTTOM;
    if(top<VISION_ROI_TOP)top=VISION_ROI_TOP;
    if(split>bottom-6)split=bottom-6;
    if(top>split-6)top=split-6;
    strip.band_half_height=.001f;strip.min_band_rows=1;
    for(y=top;y<=bottom;++y) {
        float e,x,w=y>=60?14.0f:8.0f;
        int band=y>=split?1:0;
        /* A confirmed crossing follows its current through-white bridge.
         * A remote side branch above it must not steal the forward heading.
         * Ring circulation has a separate committed inner-edge selection. */
        if(restrict_preview&&r->row_flags[y]!=4)continue;
        if(!band_error(r,&strip,(float)y/(VISION_HEIGHT-1),&e,&x))continue;
        if(r->row_flags[y]!=3)w*=.5f;
        sum[band]+=w*e;centers[band]+=w*x;weights[band]+=w;++n;
        if(band)++close_n;
    }
    if(n<6||weights[0]+weights[1]<=0)return 0;
    /* Both directions contain consecutive bends with opposite directions. Finish the
     * nearer visible bend before a distant reverse bend takes over steering.
     * Retain some distant preview, bounded so it cannot cancel a clear nearer
     * curve; the horizon moves automatically when that curve reaches row65. */
    if(close_n>=6&&absolute(sum[1]/weights[1])>=.12f&&sum[0]*sum[1]<0) {
        distant_scale=clamp(.35f*absolute(sum[1])/absolute(sum[0]),0,.20f);
    }
    *error=(sum[1]+distant_scale*sum[0])/(weights[1]+distant_scale*weights[0]);
    *center=(centers[1]+distant_scale*centers[0])/(weights[1]+distant_scale*weights[0]);
    return finite_value(*error);
}

/* One visible boundary in each of many current rows is measured evidence,
 * although the opposite boundary is modeled. Never confuse that with zero
 * current road, or let a few detached rows authorize a fast corner. */
static float current_edge_weight(const vision_result_t *r)
{
    unsigned count[2]={0,0},chain[2]={0,0},longest[2]={0,0};
    int top[2]={VISION_HEIGHT,VISION_HEIGHT},bottom[2]={-1,-1};
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y) {
        int side=r->row_flags[y]==1?0:r->row_flags[y]==2?1:-1;
        int good=side>=0&&r->row_width[y]>=VISION_MIN_WIDTH&&r->row_width[y]<=VISION_WIDTH*2U;
        if(good) {
            float center=side==0?r->left[y]+(r->row_width[y]-1)*.5f:
                                r->right[y]-(r->row_width[y]-1)*.5f;
            good=(side==0?(r->left[y]>=2&&r->left[y]<VISION_WIDTH):r->right[y]<VISION_WIDTH-2)&&
                r->center[y]<VISION_WIDTH&&absolute(r->center[y]-clamp(center,0,VISION_WIDTH-1))<=1;
        }
        for(int i=0;i<2;++i) {
            if(good&&side==i){++count[i];++chain[i];bottom[i]=y;
                if(top[i]==VISION_HEIGHT)top[i]=y;
                if(chain[i]>longest[i])longest[i]=chain[i];}
            else chain[i]=0;
        }
    }
    {float best=0;
     for(int i=0;i<2;++i){
        float a=clamp((count[i]-12.0f)/16.0f,0,1);
        float b=clamp((longest[i]-8.0f)/12.0f,0,1);
        float far=clamp((80.0f-top[i])/12.0f,0,1);
        float near=clamp((bottom[i]-86.0f)/12.0f,0,1);
        float weight=(a<b?a:b)*far*near;if(weight>best)best=weight;
     }
     return best;}
}

static float corner_duty(vc_controller_t *c,const vision_result_t *r,
                         float near_error,float far_error,int have_far,
                         float target,float previous,float dt,float duty)
{
    float turn=absolute(target),actual=absolute(c->output.steering);
    float transition=absolute(target-c->output.steering)*.5f;
    float clear,centered,measured,boost,floor,ceiling;
    if(actual>turn)turn=actual;
    if(transition>turn)turn=transition;
    /* Confidence is traced-row coverage, not a probability of leaving the
     * track. Once a current corridor is established, coverage flicker alone
     * must not impose another independent speed penalty. */
    clear=vj_paired_weight(r);
    {float edge=current_edge_weight(r)*.8f;if(edge>clear)clear=edge;}
    centered=clamp(1.0f-absolute(near_error)/.75f,0,1);
    measured=clamp(r->measured_rows/12.0f,0,1);
    if(vj_current_bridge(r))measured=1.0f;
    /* C22 field logs: 60-80 CURRENT single-edge rows were throttled solely
     * because only 5-8 rows had both edges. Use a bounded fraction of their
     * evidence; retain a lower speed class than paired road. */
    {float edge=current_edge_weight(r)*.80f;if(measured<edge)measured=edge;}
    boost=have_far?clear*centered*measured:0;
    /* Opposite near/far bends and a servo still crossing center are not a
     * straight. In B->A the return bends swap sides: finish the current bend
     * at low speed before accelerating toward the newly visible corridor. */
    if(have_far&&near_error*far_error<0)
        boost*=1.0f-clamp((absolute(near_error)-.08f)/.22f,0,1)*
                         clamp((absolute(far_error)-.10f)/.30f,0,1);
    boost*=1.0f-clamp(transition/.50f,0,1);
    /* Clear centered corners can use .90 of cruise. Weak/single-edge
     * geometry stays at .50-.60; this is demand, not a fixed PWM floor. */
    floor=c->config.cruise_duty*(.50f+.10f*centered+.30f*boost);
    /* Anticipate the requested bend, including while the physical command
     * still points into the preceding bend. No straight-speed burst at zero.
     * Missing far evidence disables the extra clear-lane boost; a large
     * lateral offset returns to the conservative .50 floor. */
    ceiling=c->config.cruise_duty*(1.0f-(.28f-.18f*boost)*clamp((turn-.15f)/.70f,0,1));
    if(duty<floor)duty=floor;
    if(duty>ceiling)duty=ceiling;
    /* Wait for sustained current two-edge geometry before using the extra
     * straight speed. A newly visible bend revokes it in the same frame. */
    if(have_far&&vj_paired_weight(r)>0&&
       absolute(near_error)<.12f&&absolute(far_error)<.16f&&turn<.18f) {
        if(!c->straight_tracking){c->straight_tracking=1;c->straight_since_ms=c->last_frame_ms;}
        if((uint32_t)(c->last_frame_ms-c->straight_since_ms)>=MT_ROUTE_STRAIGHT_CONFIRM_MS) {
            float fast=clamp(c->config.cruise_duty*(1.0f+.17f*vj_paired_weight(r)),0,c->config.duty_cap*.95f);
            if(duty<fast)duty=fast;
        }
    } else c->straight_tracking=0;
    if(duty>previous+c->config.duty_rise_per_s*dt)
        duty=previous+c->config.duty_rise_per_s*dt;
    return duty;
}
#endif


#endif
