#include "vision_metric_io.h"
#include <limits.h>
#include <string.h>
static int range(float x,float a,float b){return x>=a&&x<=b;}
static int duration(uint32_t x){return x&&x<UINT32_C(0x80000000);}
static int sign_ok(int x){return x==1||x==-1;}
static uint64_t magnitude(int64_t x){return (uint64_t)(x<0?-x:x);}
static float distance(const vm_odom_t *s,uint64_t l,uint64_t r)
{return .5f*((float)l/s->config.ticks_per_metre_left+(float)r/s->config.ticks_per_metre_right);}
static int source_ok(const vm_odom_t *s,const vm_encoder_t *e)
{
    /* Native totals saturate at INT32 endpoints; they do not wrap. Do not
     * interpret saturation as genuine stationary feedback or invent counts. */
    return e&&e->valid==1&&e->sign_left==s->config.sign_left&&e->sign_right==s->config.sign_right&&
        e->total_left!=INT32_MIN&&e->total_left!=INT32_MAX&&e->total_right!=INT32_MIN&&e->total_right!=INT32_MAX;
}
int vm_encoder_from_speed(const vd_speed_info_t *s,int have,vm_encoder_t *out)
{
    if(!out)return 0;memset(out,0,sizeof(*out));
    if(!s||have!=1||s->faults||!s->signs_confirmed||!sign_ok(s->sign3)||!sign_ok(s->sign4))return 0;
    out->sequence=s->samples;out->ms=s->sample_ms;out->total_left=s->total3;out->total_right=s->total4;
    out->sign_left=s->sign3;out->sign_right=s->sign4;out->valid=1;return 1;
}
int vm_odom_init(vm_odom_t *s,const vm_odom_config_t *c)
{
    if(!s)return 0;memset(s,0,sizeof(*s));
    if(!c||!c->revision||!duration(c->max_age_ms)||!duration(c->interpolation_gap_ms)||
       !range(c->ticks_per_metre_left,1e-6f,1e6f)||!range(c->ticks_per_metre_right,1e-6f,1e6f)||
       !sign_ok(c->sign_left)||!sign_ok(c->sign_right))return 0;
    s->config=*c;s->configured=1;return 1;
}
vm_status_t vm_odom_push(vm_odom_t *s,const vm_encoder_t *e)
{
    uint32_t dt=0,span=0;int64_t l=0,r=0;uint8_t continuous=0;
    if(!s||!s->configured)return VM_BAD_CONFIG;
    if(!source_ok(s,e))return VM_BAD_SOURCE;
    if(s->count){
        dt=e->ms-s->last.ms;span=e->sequence-s->last.sequence;
        if(!span)return e->ms==s->last.ms&&e->total_left==s->last.total_left&&e->total_right==s->last.total_right?VM_DUPLICATE:VM_CONFLICT;
        if(!duration(span)||!duration(dt))return VM_OLD;
        l=((int64_t)e->total_left-s->last.total_left)*s->config.sign_left;
        r=((int64_t)e->total_right-s->last.total_right)*s->config.sign_right;
        /* Derived from the native signed16 counter delta per sample, not a
         * newly guessed physical speed threshold. Preserve gaps explicitly. */
        if(magnitude(l)>(uint64_t)span*32768U||magnitude(r)>(uint64_t)span*32768U||
           l<INT32_MIN||l>INT32_MAX||r<INT32_MIN||r>INT32_MAX)return VM_BAD_SOURCE;
        if(UINT64_MAX-s->travel_left<magnitude(l)||UINT64_MAX-s->travel_right<magnitude(r))return VM_BAD_SOURCE;
        continuous=(uint8_t)(span==1&&dt<=s->config.interpolation_gap_ms);
    }
    s->travel_left+=magnitude(l);s->travel_right+=magnitude(r);
    s->delta_left=(int32_t)l;s->delta_right=(int32_t)r;s->elapsed_ms=dt;s->sequence_span=span;
    s->last=*e;++s->revision;
    /* Keep the retained clock window unambiguous across uint32 wrap even if
     * this offline consumer is supplied widely separated logged samples. */
    while(s->count&&(uint32_t)(e->ms-s->history[(s->head+VM_HISTORY_CAPACITY-s->count)%VM_HISTORY_CAPACITY].ms)>=UINT32_C(0x80000000))
        --s->count;
    s->history[s->head]=(vm_history_t){e->ms,e->sequence,s->travel_left,s->travel_right,continuous};
    s->head=(s->head+1)%VM_HISTORY_CAPACITY;if(s->count<VM_HISTORY_CAPACITY)++s->count;
    return VM_OK;
}
vm_status_t vm_odom_current(const vm_odom_t *s,uint32_t now,vm_motion_t *out)
{
    if(!out)return VM_BAD_SOURCE;memset(out,0,sizeof(*out));
    if(!s||!s->configured)return VM_BAD_CONFIG;if(!s->count)return VM_WAIT;
    if((uint32_t)(now-s->last.ms)>=s->config.max_age_ms)return VM_EXPIRED;
    out->sequence=s->last.sequence;out->ms=s->last.ms;out->revision=s->revision;
    out->elapsed_ms=s->elapsed_ms;out->sequence_span=s->sequence_span;
    out->delta_left=s->delta_left;out->delta_right=s->delta_right;
    out->odometer_m=distance(s,s->travel_left,s->travel_right);
    if(s->elapsed_ms){
        float per_s=1000.0f/(float)s->elapsed_ms;
        out->left_speed_m_s=(float)s->delta_left/s->config.ticks_per_metre_left*per_s;
        out->right_speed_m_s=(float)s->delta_right/s->config.ticks_per_metre_right*per_s;
        out->speed_m_s=.5f*((float)magnitude(s->delta_left)/s->config.ticks_per_metre_left+
                          (float)magnitude(s->delta_right)/s->config.ticks_per_metre_right)*per_s;
        out->velocity_valid=1;
    }
    return VM_OK;
}
vm_status_t vm_odom_at_capture(const vm_odom_t *s,uint32_t capture,uint32_t now,float *out)
{
    if(!out)return VM_BAD_SOURCE;*out=0;
    /* This query needs only timestamp freshness and distance history. Avoid
     * recomputing wheel velocities when the caller already read current(). */
    if(!s||!s->configured)return VM_BAD_CONFIG;
    if(!s->count)return VM_WAIT;
    if((uint32_t)(now-s->last.ms)>=s->config.max_age_ms)return VM_EXPIRED;
    if((uint32_t)(now-capture)>=UINT32_C(0x80000000))return VM_BAD_SOURCE;
    if((uint32_t)(capture-s->last.ms)>0&&(uint32_t)(capture-s->last.ms)<UINT32_C(0x80000000))return VM_WAIT;
    for(unsigned j=0;j<s->count;++j){
        unsigned index=(s->head+VM_HISTORY_CAPACITY-1-j)%VM_HISTORY_CAPACITY;
        const vm_history_t *b=&s->history[index];
        if(b->ms==capture){*out=distance(s,b->travel_left,b->travel_right);return VM_OK;}
        if(j+1<s->count){
            const vm_history_t *a=&s->history[(index+VM_HISTORY_CAPACITY-1)%VM_HISTORY_CAPACITY];
            uint32_t width=b->ms-a->ms,offset=capture-a->ms;
            if(duration(width)&&offset&&offset<width){
                if(!b->continuous)return VM_NO_BRACKET;
                float fraction=(float)offset/(float)width;
                *out=distance(s,a->travel_left,a->travel_right)+fraction*distance(s,b->travel_left-a->travel_left,b->travel_right-a->travel_right);
                return VM_OK;
            }
        }
    }
    return VM_NO_BRACKET;
}
int vm_vision_config_valid(const vm_vision_config_t *c)
{
    if(!c||!c->revision||!duration(c->max_age_ms)||c->near_count<5||c->near_count>SP_CAPACITY||
       c->full_count<5||c->full_count>SP_CAPACITY||c->min_confidence>100||
       !range(c->max_step_m,1e-6f,10000)||!range(c->min_x_m,-10000,10000)||
       !range(c->max_x_m,c->min_x_m,10000)||!range(c->min_y_m,0,10000)||!range(c->max_y_m,c->min_y_m,10000))return 0;
    for(unsigned i=0;i<9;++i)if(!range(c->homography[i],-1e20f,1e20f))return 0;
    return 1;
}
static int domain(const sp_geometry_history_t *h,const vm_vision_config_t *c)
{
    for(unsigned j=0;j<h->count;++j)if(!range(h->points[j].x,c->min_x_m,c->max_x_m)||
        !range(h->points[j].y,c->min_y_m,c->max_y_m))return 0;
    return 1;
}
void vm_vision_init(vm_vision_t *s){if(s)memset(s,0,sizeof(*s));}
int vm_vision_frame(vm_vision_t *s,const vm_vision_config_t *c,const vision_result_t *source,
                     uint32_t seq,uint32_t capture,uint32_t now,vm_frame_t *out)
{
    sp_geometry_history_t near_history,full_history;
    if(!out)return 0;memset(out,0,sizeof(*out));out->sequence=seq;out->capture_ms=capture;
    if(!s||!vm_vision_config_valid(c)||(s->revision&&s->revision!=c->revision))return 0;
    s->revision=c->revision;
    out->config_revision=c->revision;
    near_history=s->near_history;full_history=s->full_history;
    if(sp_vision_preview(&near_history,source,c->near_rows,c->near_count,c->min_confidence,c->homography,
        seq,capture,now,c->max_age_ms,c->max_step_m,&out->near_preview)&&domain(&near_history,c)){
        out->near_valid=1;s->near_history=near_history;
    }
    if(sp_vision_preview(&full_history,source,c->full_rows,c->full_count,c->min_confidence,c->homography,
        seq,capture,now,c->max_age_ms,c->max_step_m,&out->full_preview)&&domain(&full_history,c)){
        /* Independent evidence: a rejected near sample does not invalidate
         * the separately checked full path. This is not drive admission. */
        out->far_valid=1;s->full_history=full_history;
    }
    out->domain_valid=out->near_valid;return 1;
}
vm_status_t vm_shadow_input(const vm_odom_t *s,const vm_frame_t *f,int new_frame,vp_stamp_t id,
                             uint32_t now,float requested,int admitted,vps_input_t *out)
{
    vm_motion_t m;float captured=0;vm_status_t status;uint8_t capture_valid=0;
    if(!out)return VM_BAD_SOURCE;memset(out,0,sizeof(*out));
    if(!f||(new_frame!=0&&new_frame!=1)||(admitted!=0&&admitted!=1)||
       !range(requested,0,100)||f->near_valid>1||f->far_valid>1||f->domain_valid>1)return VM_BAD_SOURCE;
    if(!s||!s->configured||!id.config_revision||s->config.revision!=id.config_revision||
       f->config_revision!=id.config_revision)return VM_BAD_CONFIG;
    status=vm_odom_current(s,now,&m);if(status!=VM_OK)return status;if(!m.velocity_valid)return VM_WAIT;
    if(f->near_valid&&f->domain_valid){
        status=vm_odom_at_capture(s,f->capture_ms,now,&captured);if(status!=VM_OK)return status;
        capture_valid=1;
    }else if(f->far_valid){
        /* Preserve aligned full evidence when available, but never delay a
         * near-loss event waiting for the optional full-path bracket. */
        status=vm_odom_at_capture(s,f->capture_ms,now,&captured);
        if(status==VM_OK)capture_valid=1;else captured=0;
    }
    id.frame_seq=f->sequence;id.capture_ms=f->capture_ms;id.encoder_seq=m.sequence;id.encoder_ms=m.ms;
    out->identity=id;out->external_admitted=(uint8_t)admitted;out->requested_speed_m_s=requested;
    out->near_preview=f->near_preview;out->full_preview=f->full_preview;
    out->valid=VP_VALID_ENCODER;
    if(f->near_valid)out->valid|=VP_VALID_NEAR|VP_VALID_METRIC_GEOMETRY;
    if(f->domain_valid)out->valid|=VP_VALID_GROUND_DOMAIN;
    out->observation=(sp_input_t){now,f->sequence,f->capture_ms,m.sequence,m.ms,m.odometer_m,m.speed_m_s,captured,
        (uint8_t)new_frame,f->near_valid,f->far_valid,1,0,0,capture_valid};
    return VM_OK;
}
