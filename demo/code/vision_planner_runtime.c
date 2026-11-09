#include "vision_drive_internal.h"
#include "vision_planner_runtime.h"
#include <string.h>
#if VISION_PLANNER_RUNTIME
static vdr_config_t configuration;
static vdr_info_t info;
static vm_odom_t odometry;
static vm_vision_t geometry;
static vm_frame_t pending_frame,accepted_frame;
static vps_state_t planner;
static vps_result_t result;
static vp_stamp_t identity;
static uint32_t last_sample;
static uint8_t pending,have_frame,have_sample,have_result;
static uint32_t observed_sequence,observed_capture;
static uint8_t have_observation;
static int eligible(void);
static void reset_session(void)
{
    (void)vm_odom_init(&odometry,&configuration.odometry);vm_vision_init(&geometry);
    (void)vps_init(&planner,VP_SHADOW,&configuration.planning);
    pending=have_frame=have_sample=have_result=have_observation=0;info.session_active=0;
    memset(&identity,0,sizeof(identity));identity.config_revision=configuration.planning.planner.revision;
}
static void revoke(void)
{int owned=have_result||info.session_active;have_result=0;info.session_active=info.result_valid=0;if(info.mode==VDR_CONTROL&&owned)vd_speed_stop();}
static void runtime_trip(uint32_t reason)
{
    /* Internal diagnosis keeps existing recoverable/severe classes. The public
     * vision_drive_trip API intentionally adds EXTERNAL for external callers. */
    uint32_t mask=__get_PRIMASK();__disable_irq();vd.guard_faults|=reason;vd_output_zero();
    __DMB();__set_PRIMASK(mask);vd_route_switch_cancel();vc_trip(&vd.control,reason);
}
static void input_fault(vm_status_t status)
{
    info.planning_status=status==VM_BAD_CONFIG?VP_BAD_CONFIG:VP_BAD_INPUT;revoke();
    if(info.mode==VDR_CONTROL&&eligible())runtime_trip(status==VM_BAD_CONFIG?VC_FAULT_CONFIG:VC_FAULT_ENCODER);
}
static uint32_t mapped_fault(uint32_t faults)
{
    uint32_t mapped=0;
    if(faults&SP_FAULT_FRAME)mapped|=VC_FAULT_FRAME;
    if(faults&SP_FAULT_NEAR)mapped|=VC_FAULT_VISION;
    if(faults&SP_FAULT_ENCODER)mapped|=VC_FAULT_ENCODER;
    if(faults&SP_FAULT_CONTROL)mapped|=VC_FAULT_RUNTIME;
    if(faults&SP_FAULT_CONFIG)mapped|=VC_FAULT_CONFIG;
    return mapped?mapped:VC_FAULT_CONFIG;
}
static int eligible(void)
{
    return vd.control.output.state==VC_RUNNING&&vd.output_authorized&&!vd.guard_faults&&!vd.control.output.faults&&
        !vd.mode_stop_pending&&!vd.probe_active&&!vd.motion.phase&&app_mode_allows(APP_MODE_DRIVE)&&!app_mode_change_pending();
}
#endif
void vd_planner_runtime_init(void)
{
#if VISION_PLANNER_RUNTIME
    memset(&info,0,sizeof(info));pending=have_frame=have_sample=have_result=have_observation=0;
#endif
}
int vision_drive_planner_configure(vdr_mode_t mode,const vdr_config_t *c)
{
#if VISION_PLANNER_RUNTIME
    vd_speed_info_t speed;vision_drive_speed_get(&speed);
    if(vd.control.output.state!=VC_DISARMED||vd.output_authorized||vd.guard_faults||vd.probe_active||vd.motion.phase)return 0;
    if(mode==VDR_OFF){if(!vd_speed_select_source(VD_SPEED_LEGACY))return 0;vd_planner_runtime_init();return 1;}
    if((mode!=VDR_SHADOW&&mode!=VDR_CONTROL)||!c||!vp_config_valid(&c->planning.planner)||
       !sp_policy_valid(&c->planning.policy)||!vm_vision_config_valid(&c->vision)||!speed.enabled||speed.faults||
       !speed.signs_confirmed||speed.sign3!=c->odometry.sign_left||speed.sign4!=c->odometry.sign_right||
       c->odometry.revision!=c->planning.planner.revision||c->vision.revision!=c->odometry.revision||
       c->odometry.ticks_per_metre_left!=c->planning.planner.ticks_per_metre_left||
       c->odometry.ticks_per_metre_right!=c->planning.planner.ticks_per_metre_right||
       !(c->requested_speed_m_s>=0&&c->requested_speed_m_s<=c->planning.planner.limits.maximum_speed)||
       !c->pwm_cap||c->pwm_cap>1000||c->planning.planner.max_left_ticks_10ms>speed.max_ticks||
       c->planning.planner.max_right_ticks_10ms>speed.max_ticks)return 0;
    vm_odom_t checked;vps_state_t check;
    if(!vm_odom_init(&checked,&c->odometry)||!vps_init(&check,VP_SHADOW,&c->planning))return 0;
#if !VISION_PLANNER_ACTIVE
    if(mode==VDR_CONTROL)return 0;
#endif
    if(!vd_speed_select_source(mode==VDR_CONTROL?VD_SPEED_METRIC:VD_SPEED_LEGACY))return 0;
    configuration=*c;memset(&info,0,sizeof(info));info.mode=mode;info.configured=1;reset_session();return 1;
#else
    (void)mode;(void)c;return 0;
#endif
}
void vd_planner_runtime_observe(const vision_result_t *road,uint32_t seq,uint32_t capture,uint32_t now)
{
#if VISION_PLANNER_RUNTIME
    if(!info.configured)return;
    if((uint32_t)(now-capture)>=configuration.vision.max_age_ms)return;
    if(have_observation&&(!(uint32_t)(seq-observed_sequence)||(uint32_t)(seq-observed_sequence)>=UINT32_C(0x80000000)||
       !(uint32_t)(capture-observed_capture)||(uint32_t)(capture-observed_capture)>=UINT32_C(0x80000000)))return;
    if(vm_vision_frame(&geometry,&configuration.vision,road,seq,capture,now,&pending_frame)){
        ++info.observed_frames;info.near_geometry_frames+=pending_frame.near_valid;
        info.full_geometry_frames+=pending_frame.far_valid;
        pending=have_observation=1;observed_sequence=seq;observed_capture=capture;
    }
#else
    (void)road;(void)seq;(void)capture;(void)now;
#endif
}
void vd_planner_runtime_poll(uint32_t now)
{
#if VISION_PLANNER_RUNTIME
    vd_speed_info_t speed;vm_encoder_t sample;vps_input_t input;vm_status_t status;int changed=0;
    if(!info.configured)return;++info.polls;
    vision_drive_speed_get(&speed);
    if(!vm_encoder_from_speed(&speed,speed.samples!=0||have_sample,&sample)){
        input_fault(VM_BAD_SOURCE);return;
    }
    if(!have_sample||sample.sequence!=last_sample){
        status=vm_odom_push(&odometry,&sample);
        if(status!=VM_OK&&status!=VM_DUPLICATE){input_fault(status);return;}
        have_sample=1;last_sample=sample.sequence;
        changed=1;
    }
    if(!eligible()){revoke();return;}
    /* A new physical lease starts an explicit new supervisor session. A
     * latched supervisor never re-arms under the same held-ON lease. */
    if(identity.output_epoch!=vd.output_epoch){
        (void)vps_init(&planner,VP_SHADOW,&configuration.planning);
        info.session_active=0;have_result=0;identity.output_epoch=vd.output_epoch;
    }
    if(!changed&&!pending&&info.session_active)return;
    if(pending){
        status=vm_shadow_input(&odometry,&pending_frame,1,identity,now,configuration.requested_speed_m_s,1,&input);
        if(status==VM_OK){accepted_frame=pending_frame;have_frame=1;pending=0;}
        else if(status==VM_WAIT){++info.frame_waits;}
        else if(status==VM_NO_BRACKET){pending=0;} /* Discard unalignable frame, retain the previous horizon. */
        else{input_fault(status);return;}
    } else status=VM_WAIT;
    if(status!=VM_OK){
        if(!have_frame)return;
        status=vm_shadow_input(&odometry,&accepted_frame,0,identity,now,configuration.requested_speed_m_s,1,&input);
        if(status!=VM_OK){input_fault(status);return;}
    }
    info.output_epoch=input.identity.output_epoch;info.frame_sequence=input.identity.frame_seq;
    info.capture_ms=input.identity.capture_ms;info.encoder_sequence=input.identity.encoder_seq;
    info.encoder_ms=input.identity.encoder_ms;info.measured_speed_m_s=input.observation.measured_speed;
    info.near_valid=input.observation.near_valid;info.far_valid=input.observation.far_valid;
    if(!info.session_active){
        if(!vps_arm(&planner,&input))return;
        info.session_active=1;have_result=0;return;
    }
    vps_result_t next;vps_status_t prepared=vps_tick(&planner,&input,&next);
    info.supervisor_mode=planner.supervisor.mode;++info.revision;
    if(prepared==VPS_READY){
        result=next;
        ++info.plans;have_result=1;identity=result.candidate.stamp;info.planning_status=VP_READY;
        info.target_right_ticks=result.candidate.right_ticks_10ms;info.target_left_ticks=result.candidate.left_ticks_10ms;
        info.planned_speed_m_s=result.candidate.trajectory.speed;info.cap_m_s=result.candidate.cap_m_s;
        if(info.mode==VDR_CONTROL){
            if(!vd_speed_metric_request(result.candidate.right_ticks_10ms,result.candidate.left_ticks_10ms,
                configuration.pwm_cap,configuration.feedforward_right,configuration.feedforward_left,
                identity.output_epoch,identity.encoder_seq,&info.source_version))have_result=0;
        }
    } else if(prepared!=VPS_WAIT){
        result=next;
        have_result=0;info.planning_status=result.candidate.status;
        if(info.mode==VDR_CONTROL)vd_speed_stop();
        if(prepared==VPS_FAULT){if(info.mode==VDR_CONTROL)runtime_trip(mapped_fault(planner.supervisor.faults));}
        else if(planner.supervisor.mode==SP_STOP_LATCHED&&info.mode==VDR_CONTROL)vision_drive_stop();
    } else if(have_result){
        if(result.supervisor_mode==planner.supervisor.mode&&result.supervisor_generation==planner.supervisor.generation)
            result.revision=planner.revision; /* Held PI, no duplicate integral step. */
        else have_result=0;
    }
    info.result_valid=have_result;
#else
    (void)now;
#endif
}
uint16_t vd_planner_runtime_cap(void)
{
#if VISION_PLANNER_RUNTIME && VISION_PLANNER_ACTIVE
    return info.configured&&info.mode==VDR_CONTROL?configuration.pwm_cap:0;
#else
    return 0;
#endif
}
#if VISION_PLANNER_RUNTIME && VISION_PLANNER_ACTIVE
static void limit_budget(vdr_output_t *t,uint32_t now,uint32_t started,uint32_t duration)
{
    uint32_t elapsed=now-started;
    if(elapsed>=duration)t->valid_for_ms=0;
    else if(duration-elapsed<t->valid_for_ms)t->valid_for_ms=duration-elapsed;
}
#endif
void vd_planner_runtime_prepare(uint32_t epoch,vdr_output_t *t)
{
    if(!t)return;memset(t,0,sizeof(*t));
#if VISION_PLANNER_RUNTIME && VISION_PLANNER_ACTIVE
    if(!info.configured||info.mode!=VDR_CONTROL||!have_result||epoch!=identity.output_epoch||!eligible())return;
    vp_stamp_t current=identity;vd_speed_info_t speed;vision_drive_speed_get(&speed);
    uint32_t now=timer_get(GPT_TIM_1);
    current.encoder_seq=speed.samples;current.encoder_ms=speed.sample_ms;
    if(!vps_result_matches(&planner,&result,&current,now))return;
    t->epoch=epoch;t->revision=planner.revision;t->source_version=info.source_version;
    t->sample_sequence=speed.samples;t->prepared_ms=now;t->valid_for_ms=UINT32_C(0x7fffffff);
    limit_budget(t,now,result.candidate.prepared_ms,result.candidate.candidate_max_age_ms);
    limit_budget(t,now,result.candidate.stamp.capture_ms,result.candidate.frame_max_age_ms);
    limit_budget(t,now,result.candidate.stamp.encoder_ms,result.candidate.encoder_max_age_ms);
    limit_budget(t,now,planner.supervisor.last_capture_ms,configuration.planning.policy.frame_timeout_ms);
    limit_budget(t,now,planner.supervisor.last_encoder_ms,configuration.planning.policy.encoder_timeout_ms);
    if(planner.supervisor.near_active)
        limit_budget(t,now,planner.supervisor.near_started_ms,configuration.planning.policy.near_window_ms);
    if(planner.supervisor.stop_active)
        limit_budget(t,now,planner.supervisor.stop_started_ms,configuration.planning.policy.stop_timeout_ms);
    if(planner.supervisor.far_active&&planner.supervisor.mode!=SP_CONTROLLED_STOP)
        limit_budget(t,now,planner.supervisor.far_started_ms,configuration.planning.policy.far_budget_ms);
    t->valid=t->valid_for_ms!=0;
#else
    (void)epoch;
#endif
}
int vd_planner_runtime_output(const vdr_output_t *t,uint16_t *right,uint16_t *left)
{
    if(!right||!left)return 0;*right=*left=0;
#if VISION_PLANNER_RUNTIME && VISION_PLANNER_ACTIVE
    if(!t||!t->valid||!info.configured||info.mode!=VDR_CONTROL||!have_result||
       t->epoch!=vd.output_epoch||t->epoch!=identity.output_epoch||t->revision!=planner.revision||
       (uint32_t)(timer_get(GPT_TIM_1)-t->prepared_ms)>=t->valid_for_ms||!eligible())return 0;
    return vd_speed_metric_cached(t->source_version,t->sample_sequence,right,left);
#else
    (void)t;return 0;
#endif
}
void vision_drive_planner_info(vdr_info_t *out)
{
    if(!out)return;
#if VISION_PLANNER_RUNTIME
    *out=info;
#else
    memset(out,0,sizeof(*out));
#endif
}
