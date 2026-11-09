#include "vision_planner.h"
#include <string.h>

static int bounded(float x,float lo,float hi){return x>=lo&&x<=hi;}
static int interval(uint32_t age){return age&&age<UINT32_C(0x80000000);}
static int fresh(uint32_t now,uint32_t then,uint32_t age)
{return interval(age)&&(uint32_t)(now-then)<age;}
static int same_stamp(const vp_stamp_t *a,const vp_stamp_t *b)
{
    return a->output_epoch==b->output_epoch&&a->request_revision==b->request_revision&&
        a->config_revision==b->config_revision&&a->frame_seq==b->frame_seq&&
        a->encoder_seq==b->encoder_seq&&a->capture_ms==b->capture_ms&&a->encoder_ms==b->encoder_ms&&
        a->control_revision==b->control_revision;
}
static vp_status_t reject(vp_candidate_t *out,vp_status_t status)
{
    /* A reused output buffer cannot retain stale drive or trajectory values. */
    memset(out,0,sizeof(*out));out->status=status;return status;
}
int vp_config_valid(const vp_config_t *c)
{
    return c&&c->revision&&sp_limits_valid(&c->limits)&&
        bounded(c->ticks_per_metre_left,1e-6f,1e6f)&&bounded(c->ticks_per_metre_right,1e-6f,1e6f)&&
        c->max_left_ticks_10ms&&c->max_left_ticks_10ms<=1000&&
        c->max_right_ticks_10ms&&c->max_right_ticks_10ms<=1000&&
        interval(c->frame_max_age_ms)&&interval(c->encoder_max_age_ms)&&interval(c->candidate_max_age_ms)&&
        c->candidate_max_age_ms<=c->encoder_max_age_ms&&c->candidate_max_age_ms<=c->frame_max_age_ms&&
        c->compensate<=1&&c->geometry_calibrated<=1&&
        (!c->compensate||(c->geometry_calibrated&&bounded(c->rear_track_m,1e-6f,10)));
}
vp_status_t vp_prepare(vp_mode_t mode,const vp_config_t *c,const vp_input_t *in,vp_candidate_t *out)
{
    sp_plan_t plan;sp_trajectory_t trajectory;vp_candidate_t candidate;
    float target,cap,left,right;
    if(!out)return VP_BAD_INPUT;
    if(!in)return reject(out,VP_BAD_INPUT);
    if(in->action==VP_ZERO_NOW)return reject(out,VP_ZERO);
    if(mode!=VP_SHADOW)return reject(out,VP_OFF);
    if(!vp_config_valid(c)||in->stamp.config_revision!=c->revision)return reject(out,VP_BAD_CONFIG);
    if((in->action!=VP_TRACK&&in->action!=VP_CONTROLLED_STOP)||in->near_only>1||
       in->valid!=VP_VALID_ALL||!bounded(in->measured_speed_m_s,0,c->limits.maximum_speed)||
       !bounded(in->requested_speed_m_s,0,c->limits.maximum_speed)||
       !bounded(in->dt_s,.00001f,c->limits.max_dt_s))return reject(out,VP_BAD_INPUT);
    if(!fresh(in->now_ms,in->stamp.capture_ms,c->frame_max_age_ms)||
       !fresh(in->now_ms,in->stamp.encoder_ms,c->encoder_max_age_ms))return reject(out,VP_EXPIRED);
    if(in->preview.units!=SP_METRES||in->preview.sequence!=in->stamp.frame_seq||
       in->preview.capture_ms!=in->stamp.capture_ms||
       in->preview_scope!=(in->near_only?VP_NEAR_PREVIEW:VP_FULL_PREVIEW))return reject(out,VP_BAD_GEOMETRY);
    if(!sp_plan(&in->preview,&c->limits,in->measured_speed_m_s,in->near_only,0,&plan))
        return reject(out,VP_BAD_GEOMETRY);
    trajectory=in->trajectory;
    cap=plan.current_cap;target=in->requested_speed_m_s;
    if(in->action==VP_CONTROLLED_STOP){
        /* A supervisor stop is a zero target, not hard_stop. Keep physical
         * deceleration/jerk semantics distinct from immediate inhibition. */
        target=0;
    }
    if(!sp_shape(&trajectory,&c->limits,target,cap,in->dt_s,0)||trajectory.infeasible)
        return reject(out,VP_INFEASIBLE);
    if(!sp_wheel_targets(trajectory.speed,in->preview.curvature[0],c->rear_track_m,c->compensate,
        c->geometry_calibrated,c->ticks_per_metre_left,c->ticks_per_metre_right,&left,&right))
        return reject(out,VP_TARGET_RANGE);
    /* Old PI uses counts per 10ms, not counts/s and not permille. Reject
     * overflow instead of saturating one wheel and changing the turn ratio. */
    if(!bounded(left,0,(float)c->max_left_ticks_10ms*100)||
       !bounded(right,0,(float)c->max_right_ticks_10ms*100))return reject(out,VP_TARGET_RANGE);
    memset(&candidate,0,sizeof(candidate));candidate.stamp=in->stamp;
    candidate.status=VP_READY;candidate.action=in->action;candidate.provenance=c->limits.provenance;
    candidate.prepared_ms=in->now_ms;candidate.frame_max_age_ms=c->frame_max_age_ms;
    candidate.encoder_max_age_ms=c->encoder_max_age_ms;candidate.candidate_max_age_ms=c->candidate_max_age_ms;
    candidate.trajectory=trajectory;candidate.cap_m_s=cap;candidate.left_ticks_s=left;candidate.right_ticks_s=right;
    candidate.left_ticks_10ms=(uint16_t)(left/100.0f);candidate.right_ticks_10ms=(uint16_t)(right/100.0f);
    /* Defend the truncation contract even if float division rounds upward
     * exactly onto an integer boundary. Integer counts never exceed request. */
    if((float)candidate.left_ticks_10ms*100>left)--candidate.left_ticks_10ms;
    if((float)candidate.right_ticks_10ms*100>right)--candidate.right_ticks_10ms;
    candidate.quantized=(uint8_t)(left!=(float)candidate.left_ticks_10ms*100||right!=(float)candidate.right_ticks_10ms*100);
    *out=candidate;return VP_READY;
}
int vp_candidate_matches(vp_mode_t mode,const vp_candidate_t *c,const vp_stamp_t *current,uint32_t now)
{
    return mode==VP_SHADOW&&c&&current&&c->status==VP_READY&&c->stamp.config_revision&&
        (c->action==VP_TRACK||c->action==VP_CONTROLLED_STOP)&&same_stamp(&c->stamp,current)&&
        fresh(now,c->prepared_ms,c->candidate_max_age_ms)&&
        fresh(now,c->stamp.capture_ms,c->frame_max_age_ms)&&fresh(now,c->stamp.encoder_ms,c->encoder_max_age_ms);
}
