#include "vision_drive_internal.h"
#include "vision_planner_shadow_diag.h"
#include "phase3_shadow_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if VISION_PLANNER_ACTIVE || !VISION_PLANNER_RUNTIME
#error "Shadow diagnostic service requires runtime=1 and active=0."
#endif
#define SHADOW_ROWS 256U
typedef struct {
    uint32_t ms,epoch,frame,encoder,revision,poll_cycles,observe_cycles;
    int16_t measured_mm_s,planned_mm_s,cap_mm_s;
    uint16_t target_right,target_left;
    uint8_t mode,status,flags;
} shadow_row_t;
static shadow_row_t rows[SHADOW_ROWS];
static unsigned head,count,overwritten,run,frozen,loaded,was_running;
static uint32_t recorded_revision,recorded_encoder,recorded_epoch;
static uint32_t poll_max,observe_max,last_observe;
static unsigned cycles_enabled;
void vd_shadow_diag_init(void)
{
    head=count=overwritten=run=frozen=loaded=was_running=cycles_enabled=0;
    recorded_revision=recorded_encoder=recorded_epoch=UINT32_MAX;poll_max=observe_max=last_observe=0;
}
#if VISION_HOST_SHADOW_DIAG
extern uint32_t vision_test_shadow_cycles(void);
static uint32_t cycles(void){return vision_test_shadow_cycles();}
static void enable_cycles(void){cycles_enabled=1;}
static uint32_t clock_hz(void){return 600000000U;}
#else
static uint32_t cycles(void){return cycles_enabled?DWT->CYCCNT:0;}
static void enable_cycles(void)
{
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    cycles_enabled=(DWT->CTRL&DWT_CTRL_CYCCNTENA_Msk)!=0;
}
static uint32_t clock_hz(void){return SystemCoreClock;}
#endif
/* A stopped-only algorithm benchmark. Private stack states and synthetic
 * stamps never enter vd_planner_runtime, the live PI or the final writer. */
static void benchmark(char *response,unsigned capacity)
{
    vps_state_t state;vps_config_t config=phase3_shadow_configuration.planning;
    vps_input_t input;vps_result_t result;uint32_t maximum=0,total=0,expired=0;
    unsigned ready=0,zero=0,checks=0;uint16_t last_right=0,last_left=0;
    enable_cycles();
    config.planner.limits.provenance=SP_SIMULATION;
    /* Short, explicitly synthetic far episode, solely to exercise the path. */
    config.policy.far_budget_ms=50U;config.policy.far_frames=3U;
    for(unsigned scenario=0;scenario<4;++scenario){
        uint32_t period=scenario==3?20U:10U;
        float distance_per_tick=.2f*((float)period/1000.0f);
        memset(&input,0,sizeof(input));
        input.identity.output_epoch=1;input.identity.config_revision=config.planner.revision;
        input.valid=VP_VALID_ALL;input.external_admitted=1;input.requested_speed_m_s=.3f;
        input.near_preview.count=input.full_preview.count=5;
        input.near_preview.units=input.full_preview.units=SP_METRES;
        for(unsigned j=0;j<5;++j){input.near_preview.distance[j]=input.full_preview.distance[j]=(float)j*.25f;}
        checks+=(unsigned)vps_init(&state,VP_SHADOW,&config);
        for(unsigned j=0;j<=64;++j){
            uint32_t now=100U+j*period,frame=1U+j/2U,capture=100U+(j/2U)*2U*period;
            input.identity.frame_seq=frame;input.identity.capture_ms=capture;
            input.identity.encoder_seq=j+1U;input.identity.encoder_ms=now;
            input.near_preview.sequence=input.full_preview.sequence=frame;
            input.near_preview.capture_ms=input.full_preview.capture_ms=capture;
            input.observation=(sp_input_t){now,frame,capture,j+1U,now,(float)j*distance_per_tick,.2f,(float)(j/2U)*2.0f*distance_per_tick,
                (uint8_t)(j%2U==0),1,1,1,0,0,1};
            if(scenario==1&&j>=20U&&j<30U){input.observation.near_valid=0;input.observation.far_valid=0;input.valid=VP_VALID_ENCODER;}
            else input.valid=VP_VALID_ALL;
            if(scenario==2&&j>=20U)input.observation.far_valid=0;
            if(!j){checks+=(unsigned)vps_arm(&state,&input);continue;}
            uint32_t start=cycles();vps_status_t status=vps_tick(&state,&input,&result);uint32_t elapsed=cycles()-start;
            total+=elapsed;if(elapsed>maximum)maximum=elapsed;
            if(status==VPS_READY){
                ++ready;last_right=result.candidate.right_ticks_10ms;last_left=result.candidate.left_ticks_10ms;
                checks+=(unsigned)vps_result_matches(&state,&result,&result.candidate.stamp,now);
                expired+=(unsigned)!vps_result_matches(&state,&result,&result.candidate.stamp,now+config.planner.candidate_max_age_ms);
            }else if(status==VPS_ZERO)++zero;
            else if(status==VPS_FAULT){snprintf(response,capacity,"[PLANNER_BENCH] FAIL scenario=%u sample=%u status=%u synthetic=1\r\n",scenario,j,(unsigned)status);return;}
        }
        checks+=(unsigned)(scenario==2?state.supervisor.mode==SP_CONTROLLED_STOP:state.supervisor.mode==SP_NORMAL_TRACK);
    }
    unsigned passed=checks==ready+12U&&expired==ready&&ready==238U&&zero==18U&&last_left&&last_right&&cycles_enabled;
    snprintf(response,capacity,"[PLANNER_BENCH] %s synthetic=1 metric_active=0 profile=%s cases=4 ticks=256 periods_ms=10,20 ready=%u zero=%u checks=%u expired_rejected=%lu target_R/L=%u/%u cpu_hz=%lu cycles_enabled=%u mean_cycles=%lu max_cycles=%lu; includes=supervisor+planner; excludes=vision+PI+PWM+UART; no_drive_authority\r\n",
        passed?"PASS":"FAIL",PHASE3_SHADOW_PROFILE_ID,ready,zero,checks,(unsigned long)expired,last_right,last_left,
        (unsigned long)clock_hz(),cycles_enabled,(unsigned long)(total/256U),(unsigned long)maximum);
}
static int16_t millimetres(float x)
{
    if(!(x>=0&&x<=32.767f))return 0;
    return (int16_t)(x*1000.0f);
}
static void record(uint32_t now,const vdr_info_t *s,uint32_t elapsed)
{
    shadow_row_t *r=&rows[head];memset(r,0,sizeof(*r));
    r->ms=now;r->epoch=s->output_epoch;r->frame=s->frame_sequence;r->encoder=s->encoder_sequence;
    r->revision=s->revision;r->poll_cycles=elapsed;r->observe_cycles=last_observe;
    r->measured_mm_s=millimetres(s->measured_speed_m_s);r->planned_mm_s=millimetres(s->planned_speed_m_s);
    r->cap_mm_s=millimetres(s->cap_m_s);r->target_right=s->target_right_ticks;r->target_left=s->target_left_ticks;
    r->mode=(uint8_t)s->supervisor_mode;r->status=(uint8_t)s->planning_status;
    r->flags=(uint8_t)(s->near_valid|(s->far_valid<<1)|(s->result_valid<<2)|(s->session_active<<3));
    head=(head+1)%SHADOW_ROWS;if(count<SHADOW_ROWS)++count;else ++overwritten;
    recorded_revision=s->revision;recorded_encoder=s->encoder_sequence;recorded_epoch=s->output_epoch;
}
void vd_shadow_diag_poll(uint32_t now)
{
    uint32_t start=cycles();vd_planner_runtime_poll(now);
    if(!loaded)return;
    uint32_t elapsed=cycles()-start;vdr_info_t s;vision_drive_planner_info(&s);
    if(elapsed>poll_max)poll_max=elapsed;
    unsigned running=vd.control.output.state==VC_RUNNING&&vd.output_authorized;
    if(running&&!was_running&&!frozen){++run;recorded_revision=recorded_encoder=recorded_epoch=UINT32_MAX;}
    if(running&&!frozen&&(s.revision!=recorded_revision||s.encoder_sequence!=recorded_encoder||s.output_epoch!=recorded_epoch))
        record(now,&s,elapsed);
    if(!running&&was_running&&count)frozen=1;
    was_running=running;
}
void vd_shadow_diag_observe(const vision_result_t *road,uint32_t sequence,uint32_t capture,uint32_t now)
{
    uint32_t start=cycles();vd_planner_runtime_observe(road,sequence,capture,now);
    if(!loaded)return;last_observe=cycles()-start;if(last_observe>observe_max)observe_max=last_observe;
}
int vd_shadow_diag_field(const char *command,char *response,unsigned capacity)
{
    if(strncmp(command,"planner",7))return 0;
    if(vd.control.output.state==VC_RUNNING||vd.output_authorized){
        snprintf(response,capacity,"[PLANNER] REJECT stop first\r\n");return 1;
    }
    if(count)frozen=1;was_running=0;
    if(!strcmp(command,"planner_bench")){
        benchmark(response,capacity);return 1;
    }
    if(!strcmp(command,"planner_load")){
        int ok=!count&&vision_drive_planner_configure(VDR_SHADOW,&phase3_shadow_configuration);
        if(ok){loaded=1;enable_cycles();poll_max=observe_max=last_observe=0;}
        snprintf(response,capacity,"[PLANNER_LOAD] %s profile=%s mode=SHADOW metric_active=0 legacy_PI=unchanged; requires stopped encoder-ready; clear retained rows before reload\r\n",
            ok?"OK":"REJECT",PHASE3_SHADOW_PROFILE_ID);return 1;
    }
    if(!strcmp(command,"planner_clear")){
        head=count=overwritten=frozen=0;recorded_revision=recorded_encoder=recorded_epoch=UINT32_MAX;
        snprintf(response,capacity,"[PLANNER_CLEAR] OK; trace only; no drive authorization\r\n");return 1;
    }
    if(!strncmp(command,"planner_row ",12)){
        const char *p=command+12;char *end;unsigned long index=strtoul(p,&end,10);
        if(*p<'0'||*p>'9'||*end||index>=count||!frozen){snprintf(response,capacity,"[PLANNER_ROW] REJECT index/frozen\r\n");return 1;}
        shadow_row_t *r=&rows[(head+SHADOW_ROWS-count+(unsigned)index)%SHADOW_ROWS];
        snprintf(response,capacity,"[PLANNER_ROW] run=%u i=%lu ms=%lu epoch=%lu frame=%lu enc=%lu rev=%lu mode=%u status=%u flags=%u measured_mm_s=%d planned_mm_s=%d cap_mm_s=%d ticks_R/L=%u/%u poll_cycles=%lu observe_cycles=%lu\r\n",
            run,index,(unsigned long)r->ms,(unsigned long)r->epoch,(unsigned long)r->frame,(unsigned long)r->encoder,(unsigned long)r->revision,
            r->mode,r->status,r->flags,r->measured_mm_s,r->planned_mm_s,r->cap_mm_s,r->target_right,r->target_left,
            (unsigned long)r->poll_cycles,(unsigned long)r->observe_cycles);return 1;
    }
    if(!strcmp(command,"planner")){
        vdr_info_t s;vision_drive_planner_info(&s);
        snprintf(response,capacity,"[PLANNER] profile=%s configured=%u mode=%u metric_active=0 source=%u session=%u plans=%lu waits=%lu seen=%lu near_geometry=%lu full_geometry=%lu status=%u supervisor=%u run=%u count=%u frozen=%u overwritten=%u cycles=%u cpu_hz=%lu poll_max_cycles=%lu observe_max_cycles=%lu; references=engineering-only; query stops; legacy PI/gains/PWM unchanged\r\n",
            PHASE3_SHADOW_PROFILE_ID,s.configured,(unsigned)s.mode,vd_speed_source(),s.session_active,
            (unsigned long)s.plans,(unsigned long)s.frame_waits,(unsigned long)s.observed_frames,
            (unsigned long)s.near_geometry_frames,(unsigned long)s.full_geometry_frames,(unsigned)s.planning_status,(unsigned)s.supervisor_mode,
            run,count,frozen,overwritten,cycles_enabled,(unsigned long)clock_hz(),(unsigned long)poll_max,(unsigned long)observe_max);
        return 1;
    }
    snprintf(response,capacity,"[PLANNER] REJECT command; planner, planner_load, planner_bench, planner_row N, planner_clear\r\n");return 1;
}
