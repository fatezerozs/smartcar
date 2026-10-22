#include "vision_control.h"
#include "vision_profile.h"
#include <float.h>
#include <string.h>
#if VISION_ROUTE_SWITCH
#include "vision_junction_policy.h"
#endif

static int finite_value(float x) { return x == x && x >= -FLT_MAX && x <= FLT_MAX; }
static float absolute(float x) { return x < 0 ? -x : x; }
static float clamp(float x, float low, float high) { return x < low ? low : (x > high ? high : x); }
static int range(float x, float low, float high) { return finite_value(x) && x >= low && x <= high; }

void vc_default_config(vc_config_t *c)
{
    memset(c, 0, sizeof(*c));
    mol_default_config(&c->motor);
    c->near_y = VISION_AXIS_NEAR_Y; c->far_y = VISION_AXIS_FAR_Y;
    c->axis_near_x = VISION_AXIS_NEAR_X; c->axis_far_x = VISION_AXIS_FAR_X;
    c->band_half_height = VISION_CONTROL_BAND_HALF_HEIGHT;
    c->kp = VISION_CONTROL_KP; c->kd = VISION_CONTROL_KD;
    c->heading_gain = VISION_CONTROL_HEADING_GAIN;
    c->derivative_filter_s = VISION_CONTROL_D_FILTER_S;
    c->steer_rate = VISION_CONTROL_STEER_RATE;
    c->duty_rise_per_s = VISION_CONTROL_DUTY_RISE_PER_S;
    c->duty_cap = VISION_MOTOR_DUTY_CAP; c->cruise_duty = VISION_MOTOR_CRUISE_DUTY;
    c->max_lateral = VISION_CONTROL_MAX_LATERAL;
    c->frame_timeout_ms = VISION_CONTROL_FRAME_TIMEOUT_MS;
    c->command_timeout_ms = VISION_CONTROL_COMMAND_TIMEOUT_MS;
    c->max_run_ms = VISION_CONTROL_MAX_RUN_MS;
    c->arm_frames = VISION_CONTROL_ARM_FRAMES;
    c->min_coverage = VISION_CONTROL_MIN_COVERAGE;
    c->min_band_rows = VISION_CONTROL_MIN_BAND_ROWS;
    c->calibration_confirmed = VISION_CAMERA_CALIBRATED;
}

int vc_config_valid(const vc_config_t *c)
{
    if (!c) return 0;
    return mol_config_valid(&c->motor) && range(c->near_y, 0, 1) && range(c->far_y, 0, 1) &&
        c->near_y > c->far_y && range(c->band_half_height, 0.01f, 0.15f) &&
        c->near_y - c->far_y > 2 * c->band_half_height &&
        c->near_y + c->band_half_height <= (float)VISION_ROI_BOTTOM / (VISION_HEIGHT - 1) &&
        c->far_y - c->band_half_height >= (float)VISION_ROI_TOP / (VISION_HEIGHT - 1) &&
        range(c->axis_near_x, 0, 1) && range(c->axis_far_x, 0, 1) &&
        range(c->kp, 0, 10) && range(c->kd, 0, 2) && range(c->heading_gain, 0, 10) &&
        range(c->derivative_filter_s, 0.001f, 2) && range(c->steer_rate, 0.01f, 20) &&
        range(c->duty_rise_per_s, 0.001f, 1) && range(c->duty_cap, 0, 1) &&
        range(c->cruise_duty, 0, c->duty_cap) && range(c->max_lateral, 0.1f, 3) &&
        c->frame_timeout_ms >= 20 && c->frame_timeout_ms <= 500 &&
        c->command_timeout_ms >= 20 && c->command_timeout_ms <= 2000 &&
        ((VISION_ROUTE_SWITCH && !c->max_run_ms) ||
         (c->max_run_ms >= c->frame_timeout_ms && c->max_run_ms <= (VISION_ROUTE_SWITCH?120000U:60000U))) &&
        c->arm_frames >= 2 && c->min_coverage >= 1 && c->min_coverage <= 100 &&
        c->min_band_rows >= 1 && c->min_band_rows <= 20 && c->calibration_confirmed <= 1;
}

void vc_trip(vc_controller_t *c, uint32_t reason)
{
    c->local_run = 0;
    c->output.state = VC_FAULT;
    c->output.faults |= reason;
    c->output.duty = c->output.steering = 0;
    c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;
    c->output.recovering=0;c->output.search_source=0;c->recovery_steer=0;
    c->have_partial=c->straight_tracking=0;
    c->stable_frames = 0;
    c->derivative = 0;
}

void vc_init(vc_controller_t *c, const vc_config_t *config)
{
    vc_config_t saved = *config; /* Also safe when clearing with &c->config. */
    memset(c, 0, sizeof(*c));
    c->config = saved;
    c->configured = (uint8_t)vc_config_valid(&saved);
    if (!c->configured) vc_trip(c, VC_FAULT_CONFIG);
}

void vc_stop(vc_controller_t *c)
{
    c->local_run = 0;
    if (c->output.state != VC_FAULT) c->output.state = VC_DISARMED;
    c->output.duty = c->output.steering = 0;
    c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;
    c->output.recovering=0;c->output.search_source=0;c->recovery_steer=0;
    c->have_partial=c->straight_tracking=0;
    c->stable_frames = 0;
    c->derivative = 0;
}

int vc_reconfigure(vc_controller_t *c, const vc_config_t *config)
{
    if (c->output.state != VC_DISARMED || !config || !vc_config_valid(config)) return 0;
    vc_init(c, config);
    return 1;
}

void vc_clear_fault(vc_controller_t *c)
{
    if (c->output.state == VC_RUNNING) return;
    vc_init(c, &c->config);
}

void vc_poll(vc_controller_t *c, uint32_t now)
{
    int remote=!c->local_run;
    if (c->output.state != VC_RUNNING) return;
#if VISION_ROUTE_SWITCH
    if(c->output.recovering&&(uint32_t)(now-c->last_valid_ms)>300U&&
       (!c->have_partial||(uint32_t)(now-c->last_partial_ms)>100U||
        (uint32_t)(now-c->last_valid_ms)>1200U))vc_trip(c,VC_FAULT_VISION);
#endif
    if ((uint32_t)(now - c->last_frame_ms) >= c->config.frame_timeout_ms) vc_trip(c, VC_FAULT_FRAME);
    if (remote && (uint32_t)(now - c->last_command_ms) >= c->config.command_timeout_ms) vc_trip(c, VC_FAULT_COMMAND);
    if (c->config.max_run_ms && (uint32_t)(now - c->armed_ms) >= c->config.max_run_ms) vc_trip(c, VC_FAULT_RUNTIME);
}

int vc_arm(vc_controller_t *c, uint32_t now, int hardware_confirmed)
{
    if (!hardware_confirmed || !c->configured || !c->config.calibration_confirmed ||
        c->config.cruise_duty <= 0 || c->config.duty_cap <= 0 ||
        c->output.state != VC_DISARMED || !c->output.observation_valid ||
        c->stable_frames < c->config.arm_frames ||
        (uint32_t)(now - c->last_frame_ms) >= c->config.frame_timeout_ms) return 0;
    c->output.state = VC_RUNNING;
    c->local_run = 0;
    c->last_command_ms = c->armed_ms = now;
    c->output.duty = c->output.steering = 0;
    c->have_partial=c->straight_tracking=0;
    c->derivative = 0;
    return 1;
}

int vc_arm_local(vc_controller_t *c, uint32_t now, int hardware_confirmed)
{
    if (!vc_arm(c, now, hardware_confirmed)) return 0;
    c->local_run = 1;
    return 1;
}

void vc_heartbeat(vc_controller_t *c, uint32_t now)
{
    /* An expired heartbeat must never be resurrected by a late packet. */
    vc_poll(c, now);
    if (c->output.state == VC_RUNNING) c->last_command_ms = now;
}

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

static int reference_preview_error(const vision_result_t *r,const vc_config_t *c,float *error,float *center)
{
    /* newnew main.c ImgDeal_GetMidError: weighted available middleline rows.
     * Validate each row through the same measured/model consistency checks.
     * Far bends contribute before the near center starts moving. */
    float sum[2]={0,0},centers[2]={0,0},weights[2]={0,0},distant_scale=1.0f;
    vc_config_t strip=*c;
    int y,n=0,close_n=0,restrict_preview=vj_restrict_preview(r);
    strip.band_half_height=.001f;strip.min_band_rows=1;
    for(y=40;y<=88;++y) {
        float e,x,w=y>=60?14.0f:8.0f;
        int band=y>=65?1:0;
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

void vc_observe(vc_controller_t *c, const vision_result_t *r,
                uint32_t sequence, uint32_t frame_ms, uint32_t now)
{
    float near_error = 0, far_error = 0, dt = 0, heading = 0;
    int have_far;
    vc_poll(c, now); /* Check old deadlines before accepting a delayed frame. */
    if (!c->configured) return;
    if (c->have_frame && sequence == c->last_sequence) return; /* Not a new observation. */
    if (c->have_frame && (uint32_t)(frame_ms - c->last_frame_ms) >= c->config.frame_timeout_ms) {
        c->stable_frames = 0;
        c->derivative = 0;
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_FRAME);
    }
    if (c->have_frame) {
        dt = (uint32_t)(frame_ms - c->last_frame_ms) * 0.001f;
        if (dt <= 0) {
            c->stable_frames = 0;
            c->output.observation_valid = 0;
            if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_FRAME);
            return;
        }
    }
    c->have_frame = 1;
    c->last_sequence = sequence;
    c->last_frame_ms = frame_ms;
    c->output.observation_valid = c->output.far_valid = 0;
    c->output.speed_boost=0;
    c->output.paired_support=c->output.edge_support=0;
    c->output.lateral = c->output.heading = 0;
    c->output.near_center_x = c->output.far_center_x = 0;
    if ((uint32_t)(now - frame_ms) >= c->config.frame_timeout_ms || r->confidence > 100) {
        c->stable_frames=0;
        if(c->output.state==VC_RUNNING)vc_trip(c,VC_FAULT_VISION);
        return;
    }
    if (!r->valid || r->status != VISION_TRACK || r->confidence < c->config.min_coverage ||
        !control_band_error(r, &c->config, c->config.near_y, &near_error, &c->output.near_center_x)) {
        c->stable_frames = 0;
        c->straight_tracking=0;
#if VISION_ROUTE_SWITCH
        /* Partial CURRENT road can guide a bounded low-speed recovery.
         * No current boundary evidence means zero motor demand; never power
         * through a junction or outside view using only an old turn. */
        {float current_target=0;int strong=0,paired=0,carry=0;
        int current=current_partial_direction(r,&c->config,&current_target,&strong,&paired);
        if(c->output.state==VC_RUNNING && current &&
           ((uint32_t)(now-c->last_valid_ms)<=300U||
            (current&&strong&&(uint32_t)(now-c->last_valid_ms)<=1200U))) {
            float step=c->config.steer_rate*dt;
            float chosen=current_target;
            float search=clamp(chosen*1.25f,-1,1);
            /* Current white-path steering is continuous across zero and the
             * old .12 threshold. Do not jump from .119 to a .60 command. */
            /* A short modeled one-edge strip near center cannot establish a
             * new straight. Retain some recent curvature until that CURRENT
             * strip gives a clear new direction. Paired edges can unwind it. */
            if(current&&strong&&!paired&&!r->junction_partial&&absolute(chosen)<.20f&&absolute(c->recovery_steer)>.35f){
                search=clamp(search+c->recovery_steer*.85f*(1.0f-absolute(chosen)/.20f),-1,1);carry=1;}
            if(current&&strong){c->have_partial=1;c->last_partial_ms=now;}
            if(current&&search*c->output.steering<0)step*=2.0f;
            else if(current&&absolute(search)<absolute(c->output.steering))step*=1.5f;
            c->output.steering+=clamp(search-c->output.steering,-step,step);
            /* A current four-row entry plus broad white forward corridor is
             * stronger than a detached partial edge. Keep enough low-speed
             * demand to pass that transition, below clear-road/corner speed. */
            c->output.duty=clamp(c->output.duty+c->config.duty_rise_per_s*dt,0,
                c->config.cruise_duty*(r->junction_partial?.60f:.45f));
            c->output.recovering=1;c->invalid_frames=1;c->derivative=0;
            c->output.search_source=(uint8_t)(carry?3:2);
            return;
        }}
        if(c->output.state==VC_RUNNING && c->invalid_frames<2 &&
           (uint32_t)(now-c->last_valid_ms)<=40U) {
            c->output.recovering=0;c->output.search_source=0;
            ++c->invalid_frames;c->output.duty=0;c->derivative=0;
            return; /* No motor drive on missing evidence, no guessed steering. */
        }
#endif
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_VISION);
        return;
    }
#if !VISION_ROUTE_SWITCH
    if (absolute(near_error) > c->config.max_lateral) {
        c->stable_frames = 0;
        if (c->output.state == VC_RUNNING) vc_trip(c, VC_FAULT_OFFSET);
        return;
    }
#endif
    have_far = control_band_error(r, &c->config, c->config.far_y, &far_error, &c->output.far_center_x);
#if VISION_ROUTE_SWITCH
    have_far=reference_preview_error(r,&c->config,&far_error,&c->output.far_center_x)||have_far;
#endif
    if (have_far) heading = far_error - near_error;
    if (!finite_value(heading)) { vc_trip(c, VC_FAULT_NUMERIC); return; }
    if(c->invalid_frames){c->previous_lateral=near_error;c->derivative=0;}
    c->invalid_frames=0;c->last_valid_ms=frame_ms;
    c->output.recovering=0;
    c->output.search_source=0;
    c->output.observation_valid = 1;
    c->output.far_valid = (uint8_t)have_far;
    c->output.lateral = near_error;
    c->output.heading = heading;
    if (c->stable_frames < 255) ++c->stable_frames;
    if (c->output.state == VC_RUNNING) {
        float target, target_duty, derivative;
        /* Same-millisecond/reordered timestamps are not valid control timing. */
        if (dt <= 0 || dt >= c->config.frame_timeout_ms * 0.001f) { vc_trip(c, VC_FAULT_FRAME); return; }
        derivative = (near_error - c->previous_lateral) / dt;
        c->derivative += dt / (c->config.derivative_filter_s + dt) * (derivative - c->derivative);
#if VISION_ROUTE_SWITCH
        /* Preview the bend before the near point has moved. Keep the calibrated
         * far offset itself; subtracting near can cancel a useful turn command. */
        /* Keep raw lateral telemetry, but saturate its steering contribution.
         * A measured/clipped-edge lane is not discarded at an arbitrary offset
         * before the vehicle has had time to turn. Missing evidence still zeros. */
        {float preview_scale=1.0f;
        /* When the near road center has crossed to the other side, prioritize
         * centering instead of holding the previous bend from far preview.
         * Continuous taper avoids switching abruptly at a tiny center error. */
        if(have_far&&near_error*far_error<0)
            preview_scale=clamp((.22f-absolute(near_error))/.18f,.05f,1.0f);
        target=c->config.kp*clamp(near_error,-c->config.max_lateral,c->config.max_lateral)+
            c->config.kd*c->derivative+
            (have_far?preview_scale*c->config.heading_gain*clamp(far_error,-c->config.max_lateral,c->config.max_lateral):0);}
#else
        target = c->config.kp * near_error + c->config.kd * c->derivative + c->config.heading_gain * heading;
#endif
        if (!finite_value(target)) { vc_trip(c, VC_FAULT_NUMERIC); return; }
        target = clamp(target, -1, 1);
#if VISION_ROUTE_SWITCH
        c->recovery_steer=target;
#endif
        {float rate=c->config.steer_rate;
#if VISION_ROUTE_SWITCH
        /* Release an excessive bend command faster than building it up.
         * A reversal still passes smoothly through center with bounded rate. */
        if(absolute(target)<absolute(c->output.steering)||target*c->output.steering<0)rate*=1.5f;
        else if(absolute(near_error)>.35f&&target*near_error>0)rate*=1.5f;
#endif
        c->output.steering += clamp(target - c->output.steering,-rate*dt,rate*dt);}
        if (!mol_step(&c->config.motor, near_error, heading, c->output.steering,
                      r->confidence * .01f, have_far, c->config.cruise_duty,
                      c->config.duty_cap, c->config.duty_rise_per_s,
                      c->output.duty, dt, &target_duty)) {
            vc_trip(c, VC_FAULT_NUMERIC); return;
        }
#if VISION_ROUTE_SWITCH
        target_duty=corner_duty(c,r,near_error,far_error,have_far,target,
                               c->output.duty,dt,target_duty);
#endif
        c->output.duty = target_duty;
#if VISION_ROUTE_SWITCH
        c->output.speed_boost=(uint8_t)(have_far&&vj_paired_support(r));
        c->output.paired_support=have_far?vj_paired_weight(r):0;
        c->output.edge_support=have_far?current_edge_weight(r):0;
        /* Labels describe CURRENT support; they do not veto ring/cross speed.
         * The output layer blends paired/curve scales continuously using
         * actual/requested steering and lateral margin. Missing support still
         * falls back; coverage metadata alone cannot force a scale jump. */
        if(c->output.speed_boost&&absolute(c->output.steering)>=.45f)
            c->output.speed_boost=2;
        else if(!c->output.speed_boost&&have_far&&current_edge_weight(r)>=1.0f)
            c->output.speed_boost=3;
#endif
    } else c->output.duty = c->output.steering = 0;
    c->previous_lateral = near_error;
}
