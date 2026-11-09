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
    snprintf(response,capacity,"[PLANNER] REJECT command; planner, planner_load, planner_row N, planner_clear\r\n");return 1;
}
