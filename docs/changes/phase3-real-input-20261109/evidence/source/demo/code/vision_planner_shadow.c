#include "vision_planner_shadow.h"
#include <string.h>
static int finite_range(float x,float lo,float hi){return x>=lo&&x<=hi;}
static int coherent(const vps_state_t *s,const vps_input_t *in)
{
    const sp_input_t *i=&in->observation;const vp_stamp_t *id=&in->identity;
    return id->config_revision==s->config.planner.revision&&id->frame_seq==i->frame_seq&&
        id->encoder_seq==i->encoder_seq&&id->capture_ms==i->capture_ms&&id->encoder_ms==i->encoder_ms&&
        in->external_admitted<=1&&i->new_frame<=1&&i->near_valid<=1&&i->far_valid<=1&&
        i->encoder_valid<=1&&i->hard_stop<=1&&i->severe_fault<=1&&i->capture_odometer_valid<=1&&
        !(in->valid&~VP_VALID_ALL)&&finite_range(i->measured_speed,0,s->config.planner.limits.maximum_speed)&&
        finite_range(in->requested_speed_m_s,0,s->config.planner.limits.maximum_speed);
}
static int aligned(const sp_input_t *i)
{
    return i->capture_odometer_valid&&finite_range(i->odometer_m,0,1e9f)&&
        finite_range(i->odometer_at_capture_m,0,i->odometer_m);
}
static int preview_valid(const sp_preview_t *v,const sp_input_t *i)
{
    return v->sequence==i->frame_seq&&v->capture_ms==i->capture_ms&&aligned(i)&&
        sp_preview_valid(v)&&i->odometer_m-i->odometer_at_capture_m<v->distance[v->count-1]-.000001f;
}
static void reset_trajectory(vps_state_t *s)
{memset(&s->trajectory,0,sizeof(s->trajectory));++s->control_revision;s->seed_required=1;}
static void fault(vps_state_t *s,sp_input_t *i)
{i->severe_fault=1;sp_supervise(&s->supervisor,&s->config.policy,i);reset_trajectory(s);}
static vps_status_t finish(vps_state_t *s,vps_result_t *out,vps_status_t status)
{
    out->status=status;out->revision=s->revision;out->supervisor_mode=s->supervisor.mode;
    out->supervisor_generation=s->supervisor.generation;return status;
}
int vps_init(vps_state_t *s,vp_mode_t mode,const vps_config_t *c)
{
    if(!s)return 0;memset(s,0,sizeof(*s));sp_supervisor_init(&s->supervisor);
    if(mode!=VP_SHADOW||!c||!vp_config_valid(&c->planner)||!sp_policy_valid(&c->policy)||
       c->planner.frame_max_age_ms>c->policy.frame_timeout_ms||
       c->planner.encoder_max_age_ms>c->policy.encoder_timeout_ms)return 0;
    s->config=*c;s->configured=1;s->seed_required=1;return 1;
}
int vps_arm(vps_state_t *s,const vps_input_t *in)
{
    if(!s||!in||!s->configured||s->supervisor.mode!=SP_DISARMED||!in->external_admitted||
       !coherent(s,in)||in->valid!=VP_VALID_ALL||!preview_valid(&in->near_preview,&in->observation)||
       !preview_valid(&in->full_preview,&in->observation)||
       (uint32_t)(in->observation.now_ms-in->observation.capture_ms)>=s->config.planner.frame_max_age_ms||
       (uint32_t)(in->observation.now_ms-in->observation.encoder_ms)>=s->config.planner.encoder_max_age_ms||
       !sp_supervisor_arm(&s->supervisor,&s->config.policy,&in->observation))return 0;
    s->near_preview=in->near_preview;s->full_preview=in->full_preview;s->have_near=s->have_full=1;
    s->near_odometer=s->full_odometer=in->observation.odometer_at_capture_m;
    s->identity=in->identity;s->latest_observation=in->observation;
    s->last_control_ms=in->observation.now_ms;s->consumed_encoder=in->observation.encoder_seq;
    reset_trajectory(s);s->trajectory.speed=in->observation.measured_speed;s->seed_required=0;++s->revision;
    return 1;
}
vps_status_t vps_tick(vps_state_t *s,const vps_input_t *in,vps_result_t *out)
{
    sp_input_t i;uint32_t previous_frame,dt;float dt_s;vp_input_t plan;sp_preview_t rebased;const sp_preview_t *selected;
    if(!out)return VPS_FAULT;memset(out,0,sizeof(*out));
    if(!s||!s->configured){out->status=VPS_INACTIVE;return VPS_INACTIVE;}
    ++s->revision;
    if(!in){i=s->latest_observation;fault(s,&i);return finish(s,out,VPS_FAULT);}
    i=in->observation;
    if(!s->supervisor.authorized)return finish(s,out,s->supervisor.mode==SP_FAULT_LATCHED?VPS_FAULT:VPS_ZERO);
    if(!coherent(s,in)){fault(s,&i);return finish(s,out,VPS_FAULT);}
    if(!(in->valid&VP_VALID_ENCODER))i.encoder_valid=0;
    if(!in->external_admitted||in->identity.output_epoch!=s->identity.output_epoch)i.hard_stop=1;
    if(i.new_frame&&i.near_valid&&((in->valid&VP_VALID_ALL)!=VP_VALID_ALL||
       !preview_valid(&in->near_preview,&i)))i.near_valid=0;
    if(i.new_frame&&i.far_valid&&!preview_valid(&in->full_preview,&i))i.far_valid=0;
    previous_frame=s->supervisor.last_frame_seq;
    sp_supervise(&s->supervisor,&s->config.policy,&i);
    s->latest_observation=i;s->identity=in->identity;
    if(previous_frame!=s->supervisor.last_frame_seq){
        s->have_near=i.near_valid;s->have_full=i.near_valid&&i.far_valid;
        if(s->have_near){s->near_preview=in->near_preview;s->near_odometer=i.odometer_at_capture_m;}
        if(s->have_full){s->full_preview=in->full_preview;s->full_odometer=i.odometer_at_capture_m;}
    }
    if(!s->supervisor.authorized||s->supervisor.motor_inhibit){
        reset_trajectory(s);s->consumed_encoder=i.encoder_seq;s->last_control_ms=i.now_ms;
        return finish(s,out,s->supervisor.mode==SP_FAULT_LATCHED?VPS_FAULT:VPS_ZERO);
    }
    dt=i.now_ms-s->last_control_ms;
    if(!dt||i.encoder_seq==s->consumed_encoder)return finish(s,out,VPS_WAIT);
    /* Divide once: 20 * .001f rounds above the legal .02f boundary. */
    dt_s=(float)dt/1000.0f;
    if(i.encoder_seq-s->consumed_encoder!=1||!finite_range(dt_s,.00001f,s->config.planner.limits.max_dt_s)){
        fault(s,&i);return finish(s,out,VPS_FAULT);
    }
    selected=s->supervisor.far_active?&s->near_preview:&s->full_preview;
    if(!(s->supervisor.far_active?s->have_near:s->have_full)||
       !sp_preview_advance(selected,i.odometer_m-(s->supervisor.far_active?s->near_odometer:s->full_odometer),&rebased)){
        fault(s,&i);return finish(s,out,VPS_FAULT);
    }
    memset(&plan,0,sizeof(plan));plan.stamp=in->identity;
    /* The cached accepted frame is authoritative between camera publications.
     * Duplicate/rejected frames cannot renew its identity or geometry. */
    plan.stamp.frame_seq=s->supervisor.last_frame_seq;plan.stamp.capture_ms=s->supervisor.last_capture_ms;
    plan.stamp.control_revision=s->control_revision;plan.now_ms=i.now_ms;plan.valid=VP_VALID_ALL;
    plan.action=s->supervisor.stop_active?VP_CONTROLLED_STOP:VP_TRACK;
    plan.near_only=s->supervisor.far_active;plan.preview_scope=plan.near_only?VP_NEAR_PREVIEW:VP_FULL_PREVIEW;
    plan.preview=rebased;plan.trajectory=s->trajectory;
    if(s->seed_required){plan.trajectory.speed=i.measured_speed;plan.trajectory.accel=0;}
    plan.measured_speed_m_s=i.measured_speed;plan.requested_speed_m_s=in->requested_speed_m_s;plan.dt_s=dt_s;
    if(vp_prepare(VP_SHADOW,&s->config.planner,&plan,&out->candidate)!=VP_READY){
        fault(s,&i);return finish(s,out,VPS_FAULT);
    }
    s->trajectory=out->candidate.trajectory;++s->control_revision;s->seed_required=0;
    s->consumed_encoder=i.encoder_seq;s->last_control_ms=i.now_ms;
    out->candidate.stamp.control_revision=s->control_revision;
    s->identity=out->candidate.stamp;
    return finish(s,out,VPS_READY);
}
int vps_result_matches(const vps_state_t *s,const vps_result_t *out,const vp_stamp_t *current,uint32_t now)
{
    sp_supervisor_t check;sp_input_t i;vp_stamp_t identity;
    if(!s||!out||!current||!s->configured||out->status!=VPS_READY||out->revision!=s->revision||
       out->supervisor_generation!=s->supervisor.generation)return 0;
    identity=*current;identity.control_revision=s->control_revision;
    if(!vp_candidate_matches(VP_SHADOW,&out->candidate,&identity,now))return 0;
    check=s->supervisor;i=s->latest_observation;i.new_frame=0;i.now_ms=now;
    sp_supervise(&check,&s->config.policy,&i);
    return sp_candidate_allowed(&check,out->supervisor_generation)&&check.mode==out->supervisor_mode;
}
