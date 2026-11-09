#include "pipeline.h"
#include <math.h>
#include <string.h>
static int positive(float value){return isfinite(value)&&value>0;}
static void zero_motors(sp_pipeline_t *p)
{
    ws_pi_reset(&p->pi[0]);ws_pi_reset(&p->pi[1]);
    p->left_target=p->right_target=0;p->left_pwm=p->right_pwm=0;
    memset(&p->trajectory,0,sizeof(p->trajectory));
    p->current_cap=0;
}
static void fail(sp_pipeline_t *p,sp_input_t *i)
{i->severe_fault=1;sp_supervise(&p->supervisor,&p->config.policy,i);zero_motors(p);}
static int preview_valid(const sp_preview_t *v,const sp_input_t *i,const sp_limits_t *limits)
{
    sp_plan_t ignored;
    return v&&v->sequence==i->frame_seq&&v->capture_ms==i->capture_ms&&
        sp_plan(v,limits,0,0,0,&ignored);
}
static float anticipate_curve(const sp_pipeline_t *p,const sp_preview_t *v,float cap)
{
    float speed=p->trajectory.speed,a=fmaxf(0,p->trajectory.accel),brake=p->config.planning_brake;
    float peak=speed+a*a/(2*p->config.limits.jerk_brake);
    float ramp_time=(a+brake)/p->config.limits.jerk_brake+brake/p->config.limits.jerk_accel;
    for(unsigned j=1;j<v->count;++j){
        float k=fabsf(v->curvature[j]),curve,distance;
        if(k<=p->config.limits.curvature_epsilon)continue;
        curve=sqrtf(p->config.limits.lateral_accel/(k+p->config.limits.curvature_epsilon));
        if(curve>=speed)continue;
        /* Explicit reference design allowance for jerk ramp and tracking delay.
         * This is not a certified bound for an unidentified physical plant. */
        distance=peak*(ramp_time+p->config.limits.latency_s)+fmaxf(0,peak*peak-curve*curve)/(2*brake);
        if(v->distance[j]<=distance&&curve<cap)cap=curve;
    }
    return cap;
}
int sp_pipeline_init(sp_pipeline_t *p,const sp_pipeline_config_t *c,uint64_t session)
{
    if(!p||!c)return 0;memset(p,0,sizeof(*p));sp_supervisor_init(&p->supervisor);
    if(!session||!sp_limits_valid(&c->limits)||c->limits.provenance!=SP_SIMULATION||
       !positive(c->ticks_per_metre_left)||!positive(c->ticks_per_metre_right)||
       c->ticks_per_metre_left>1000000||c->ticks_per_metre_right>1000000||
       !positive(c->feedforward_full_speed)||c->feedforward_full_speed>100||!c->kp_q8||
       !positive(c->planning_brake)||c->planning_brake>c->limits.brake||
       c->limits.maximum_speed*fmaxf(c->ticks_per_metre_left,c->ticks_per_metre_right)*.01f>1000)return 0;
    p->config=*c;p->session=session;p->configured=1;return 1;
}
int sp_pipeline_arm(sp_pipeline_t *p,const sp_pipeline_input_t *in,const sp_preview_t *near_p,const sp_preview_t *full_p)
{
    if(!p||!in||!p->configured||!preview_valid(near_p,&in->observation,&p->config.limits)||
       !in->observation.capture_odometer_valid||!isfinite(in->observation.odometer_at_capture_m)||
       in->observation.odometer_at_capture_m<0||in->observation.odometer_at_capture_m>in->observation.odometer_m||
       !preview_valid(full_p,&in->observation,&p->config.limits)||
       !sp_supervisor_arm(&p->supervisor,&p->config.policy,&in->observation))return 0;
    p->near_preview=*near_p;p->full_preview=*full_p;p->have_near=p->have_full=1;
    p->near_valid=p->far_valid=1;
    p->near_odometer=p->full_odometer=in->observation.odometer_at_capture_m;
    p->latest_sample=p->consumed_sample=in->observation.encoder_seq;
    p->consumed_ms=in->observation.encoder_ms;p->last_control_ms=in->observation.now_ms;return 1;
}
int sp_pipeline_prepare(sp_pipeline_t *p,const sp_pipeline_input_t *in,const sp_preview_t *near_p,const sp_preview_t *full_p,sp_pipeline_candidate_t *c)
{
    sp_input_t i;sp_plan_t plan;sp_limits_t planning;sp_preview_t rebased;const sp_preview_t *selected;uint32_t old_frame,dt_ms,elapsed;
    if(!p||!in||!c||!p->configured)return 0;memset(c,0,sizeof(*c));i=in->observation;
    if(i.new_frame&&i.near_valid&&!preview_valid(near_p,&i,&p->config.limits))i.near_valid=0;
    if(i.new_frame&&i.far_valid&&!preview_valid(full_p,&i,&p->config.limits))i.far_valid=0;
    old_frame=p->supervisor.last_frame_seq;
    sp_supervise(&p->supervisor,&p->config.policy,&i);p->latest_sample=i.encoder_seq;++p->request;
    if(p->supervisor.last_frame_seq!=old_frame){
        p->near_valid=i.near_valid;p->far_valid=i.far_valid;
        p->have_near=i.near_valid;p->have_full=i.near_valid&&i.far_valid;
        if((p->have_near||p->have_full)&&(!i.capture_odometer_valid||!isfinite(i.odometer_at_capture_m)||
           i.odometer_at_capture_m<0||i.odometer_at_capture_m>i.odometer_m)){fail(p,&i);return 0;}
        if(p->have_near){p->near_preview=*near_p;p->near_odometer=i.odometer_at_capture_m;}
        if(p->have_full){p->full_preview=*full_p;p->full_odometer=i.odometer_at_capture_m;}
    }
    if(!p->supervisor.authorized||p->supervisor.motor_inhibit){
        zero_motors(p);p->consumed_sample=i.encoder_seq;p->consumed_ms=i.encoder_ms;p->last_control_ms=i.now_ms;return 0;
    }
    if(!p->have_near){i.near_valid=0;i.new_frame=1;fail(p,&i);return 0;}
    dt_ms=i.now_ms-p->last_control_ms;
    if(!dt_ms)return 0;
    if(dt_ms>50U){fail(p,&i);return 0;}
    selected=p->supervisor.far_active?&p->near_preview:&p->full_preview;
    if(!p->supervisor.far_active&&!p->have_full){fail(p,&i);return 0;}
    planning=p->config.limits;planning.brake=p->config.planning_brake;
    if(!sp_preview_advance(selected,i.odometer_m-(p->supervisor.far_active?p->near_odometer:p->full_odometer),&rebased)||
       !sp_plan(&rebased,&planning,fmaxf(0,i.measured_speed),p->supervisor.far_active,0,&plan)){fail(p,&i);return 0;}
    c->trajectory=p->trajectory;
    if(!sp_shape(&c->trajectory,&p->config.limits,p->supervisor.stop_active?0:anticipate_curve(p,&rebased,plan.current_cap),
                 plan.current_cap,(float)dt_ms*.001f,0)||c->trajectory.infeasible){fail(p,&i);return 0;}
    c->pi[0]=p->pi[0];c->pi[1]=p->pi[1];
    /* Equal longitudinal wheel speed until actual steering/geometry calibration.
     * Distinct ticks/metre still supports different left/right count scales. */
    c->left_target=(int32_t)lroundf(c->trajectory.speed*p->config.ticks_per_metre_left*.01f);
    c->right_target=(int32_t)lroundf(c->trajectory.speed*p->config.ticks_per_metre_right*.01f);
    if(i.encoder_seq!=p->consumed_sample){
        elapsed=i.encoder_ms-p->consumed_ms;
        if(i.encoder_seq-p->consumed_sample!=1||elapsed<10||elapsed>20){fail(p,&i);return 0;}
        {float ratio=c->trajectory.speed/p->config.feedforward_full_speed;
         uint16_t ff=(uint16_t)fminf(1000,fmaxf(0,ratio*1000));
         ws_pi_step_ff_ms(&c->pi[0],c->left_target,in->measured_left_ticks10ms,p->config.kp_q8,p->config.ki_q8,1000,elapsed,ff);
         ws_pi_step_ff_ms(&c->pi[1],c->right_target,in->measured_right_ticks10ms,p->config.kp_q8,p->config.ki_q8,1000,elapsed,ff);}
    }
    if(!c->left_target)ws_pi_reset(&c->pi[0]);if(!c->right_target)ws_pi_reset(&c->pi[1]);
    c->session=p->session;c->generation=p->supervisor.generation;c->request=p->request;
    c->sample_sequence=i.encoder_seq;c->sample_ms=i.encoder_ms;c->control_ms=i.now_ms;
    c->left_pwm=c->pi[0].output;c->right_pwm=c->pi[1].output;c->cap=plan.current_cap;c->ready=1;return 1;
}
int sp_pipeline_commit(sp_pipeline_t *p,const sp_pipeline_candidate_t *c)
{
    if(!p||!c||!c->ready||p->session!=c->session||p->request!=c->request||p->latest_sample!=c->sample_sequence||
       !sp_candidate_allowed(&p->supervisor,c->generation))return 0;
    p->trajectory=c->trajectory;p->pi[0]=c->pi[0];p->pi[1]=c->pi[1];
    p->left_target=c->left_target;p->right_target=c->right_target;p->left_pwm=c->left_pwm;p->right_pwm=c->right_pwm;
    p->current_cap=c->cap;
    p->consumed_sample=c->sample_sequence;p->consumed_ms=c->sample_ms;p->last_control_ms=c->control_ms;
    ++p->commit_sequence;++p->request;return 1;
}
void sp_pipeline_snapshot(const sp_pipeline_t *p,const sp_pipeline_input_t *in,sp_telemetry_t *out)
{
    const sp_input_t *i=&in->observation;memset(out,0,sizeof(*out));
    out->control_ms=i->now_ms;out->frame_sequence=p->supervisor.last_frame_seq;out->capture_ms=p->supervisor.last_capture_ms;
    out->encoder_sequence=i->encoder_seq;out->generation=p->supervisor.generation;out->commit_sequence=p->commit_sequence;
    out->mode=p->supervisor.mode;out->faults=p->supervisor.faults;
    out->speed_reference=p->trajectory.speed;out->accel_reference=p->trajectory.accel;out->measured_speed=i->measured_speed;
    out->far_distance=p->supervisor.far_distance_m;out->left_target=p->left_target;out->right_target=p->right_target;
    out->current_cap=p->current_cap;out->brake_override=p->trajectory.brake_override;
    out->left_integral=p->pi[0].integral_q8;out->right_integral=p->pi[1].integral_q8;
    out->left_pwm=p->left_pwm;out->right_pwm=p->right_pwm;out->near_valid=p->near_valid;out->far_valid=p->far_valid;
    out->raw_near_valid=i->near_valid;out->raw_far_valid=i->far_valid;
}
int sp_telemetry_push(sp_telemetry_queue_t *q,const sp_telemetry_t *s)
{
    if(!q||!s)return 0;if(q->count==SP_TELEMETRY_CAPACITY){if(q->dropped<UINT32_MAX)++q->dropped;return 0;}
    q->data[(q->head+q->count)%SP_TELEMETRY_CAPACITY]=*s;++q->count;
    if(q->count>q->high_water)q->high_water=q->count;return 1;
}
int sp_telemetry_pop(sp_telemetry_queue_t *q,sp_telemetry_t *s)
{
    if(!q||!s||!q->count)return 0;*s=q->data[q->head];q->head=(q->head+1)%SP_TELEMETRY_CAPACITY;--q->count;return 1;
}
