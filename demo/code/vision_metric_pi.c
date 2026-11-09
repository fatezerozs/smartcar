#include "vision_metric_pi.h"
#include <string.h>
static int feedback_valid(const vm_pi_state_t *s,const vm_motion_t *m,uint32_t now)
{
    return m&&m->velocity_valid==1&&m->sequence_span==1&&m->sequence-s->last_sequence==1&&
        m->elapsed_ms==m->ms-s->last_ms&&m->elapsed_ms>=VD_SPEED_SAMPLE_MIN_INTERVAL_MS&&
        m->elapsed_ms<=VD_SPEED_SAMPLE_MAX_INTERVAL_MS&&
        (uint32_t)(now-m->ms)<VD_SPEED_SAMPLE_MAX_AGE_MS&&
        m->delta_left>=-32768&&m->delta_left<=32768&&m->delta_right>=-32768&&m->delta_right<=32768;
}
int vm_pi_init(vm_pi_state_t *s,const vm_pi_config_t *c,uint64_t session,const vm_motion_t *anchor)
{
    if(!s)return 0;memset(s,0,sizeof(*s));
    if(!c||!c->revision||!session||!anchor||!c->kp_left_q8||!c->kp_right_q8||
       !c->pwm_cap_left||c->pwm_cap_left>1000||!c->pwm_cap_right||c->pwm_cap_right>1000)return 0;
    s->config=*c;s->session=session;s->last_sequence=anchor->sequence;s->last_ms=anchor->ms;s->configured=1;return 1;
}
void vm_pi_stop(vm_pi_state_t *s,const vm_motion_t *m)
{
    if(!s)return;ws_pi_reset(&s->left);ws_pi_reset(&s->right);++s->revision;
    if(m&&((m->sequence==s->last_sequence&&m->ms==s->last_ms)||
       ((uint32_t)(m->sequence-s->last_sequence)>0&&(uint32_t)(m->sequence-s->last_sequence)<UINT32_C(0x80000000)&&
        (uint32_t)(m->ms-s->last_ms)>0&&(uint32_t)(m->ms-s->last_ms)<UINT32_C(0x80000000)))){
        s->last_sequence=m->sequence;s->last_ms=m->ms;
    }
}
vm_pi_status_t vm_pi_prepare(const vm_pi_state_t *s,const vps_state_t *p,const vps_result_t *r,
                              const vm_motion_t *m,const vp_stamp_t *current,uint32_t now,
                              uint16_t ff_left,uint16_t ff_right,vm_pi_candidate_t *out)
{
    if(!out)return VM_PI_REJECTED;memset(out,0,sizeof(*out));out->status=VM_PI_REJECTED;
    if(!s||!s->configured||!m||!r||!current||!p||
       s->config.revision!=p->config.planner.revision)return VM_PI_REJECTED;
    if(m->sequence==s->last_sequence&&m->ms==s->last_ms){out->status=VM_PI_WAIT;return VM_PI_WAIT;}
    if(!vps_result_matches(p,r,current,now)||!feedback_valid(s,m,now)||
       m->sequence!=r->candidate.stamp.encoder_seq||m->ms!=r->candidate.stamp.encoder_ms)return VM_PI_REJECTED;
    out->left=s->left;out->right=s->right;out->stamp=r->candidate.stamp;out->session=s->session;
    out->base_revision=s->revision;out->result_revision=r->revision;out->feedback_revision=m->revision;
    out->elapsed_ms=m->elapsed_ms;out->target_left=r->candidate.left_ticks_10ms;out->target_right=r->candidate.right_ticks_10ms;
    out->measured_left=(int32_t)((int64_t)m->delta_left*10/m->elapsed_ms);
    out->measured_right=(int32_t)((int64_t)m->delta_right*10/m->elapsed_ms);
    /* Feedforward is explicit, bounded by the same shared kernel as legacy
     * control. No duty-to-speed model, repeated target ramp or steering mix. */
    ws_pi_step_ff_ms(&out->left,out->target_left,out->measured_left,s->config.kp_left_q8,s->config.ki_left_q8,
        s->config.pwm_cap_left,m->elapsed_ms,ff_left);
    ws_pi_step_ff_ms(&out->right,out->target_right,out->measured_right,s->config.kp_right_q8,s->config.ki_right_q8,
        s->config.pwm_cap_right,m->elapsed_ms,ff_right);
    out->status=VM_PI_READY;return VM_PI_READY;
}
int vm_pi_commit(vm_pi_state_t *s,const vps_state_t *p,const vps_result_t *r,const vm_motion_t *m,
                    const vp_stamp_t *current,uint32_t now,const vm_pi_candidate_t *c)
{
    if(!s||!s->configured||!c||c->status!=VM_PI_READY||!r||!current||!p||
       s->config.revision!=p->config.planner.revision||
       c->session!=s->session||c->base_revision!=s->revision||c->result_revision!=r->revision||
       !vps_result_matches(p,r,current,now)||!feedback_valid(s,m,now)||c->feedback_revision!=m->revision||
       m->sequence!=c->stamp.encoder_seq||m->ms!=c->stamp.encoder_ms||m->elapsed_ms!=c->elapsed_ms||
       c->target_left!=r->candidate.left_ticks_10ms||c->target_right!=r->candidate.right_ticks_10ms||
       !vp_candidate_matches(VP_SHADOW,&r->candidate,&c->stamp,now)||
       c->measured_left!=(int32_t)((int64_t)m->delta_left*10/m->elapsed_ms)||
       c->measured_right!=(int32_t)((int64_t)m->delta_right*10/m->elapsed_ms))return 0;
    s->left=c->left;s->right=c->right;s->last_sequence=m->sequence;s->last_ms=m->ms;++s->revision;return 1;
}
vm_pi_status_t vm_pi_apply_shadow(vm_pi_state_t *s,const vps_state_t *p,const vps_result_t *r,
                                    const vm_motion_t *m,const vp_stamp_t *current,uint32_t now,
                                    uint16_t ff_left,uint16_t ff_right,vm_pi_candidate_t *out)
{
    if(!out){vm_pi_stop(s,m);return VM_PI_REJECTED;}
    memset(out,0,sizeof(*out));out->status=VM_PI_REJECTED;
    if(!s||!s->configured||!p||!r||r->revision!=p->revision){vm_pi_stop(s,m);return VM_PI_REJECTED;}
    if(r->status==VPS_ZERO||r->status==VPS_FAULT||r->status==VPS_INACTIVE||
       !p->supervisor.authorized||p->supervisor.motor_inhibit){
        vm_pi_stop(s,m);out->status=VM_PI_ZERO;return VM_PI_ZERO;
    }
    if(r->status==VPS_WAIT){out->status=VM_PI_WAIT;return VM_PI_WAIT;}
    vm_pi_status_t status=vm_pi_prepare(s,p,r,m,current,now,ff_left,ff_right,out);
    if(status==VM_PI_WAIT)return status;
    if(status!=VM_PI_READY||!vm_pi_commit(s,p,r,m,current,now,out)){
        vm_pi_stop(s,m);memset(out,0,sizeof(*out));out->status=VM_PI_REJECTED;return VM_PI_REJECTED;
    }
    return VM_PI_READY;
}
