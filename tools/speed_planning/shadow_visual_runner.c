/* Hybrid fixture: measured visual flags/times; all geometry and motion are
 * explicitly synthetic. Executes target shadow code, never PI or hardware. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "vision_planner_shadow.h"
#include "visual_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    FILE *input,*output;char line[512];unsigned index,seq,cap,now,near_ok,far_ok;
    unsigned arm_index;vps_state_t state;vps_config_t config;vps_input_t in;vps_result_t result;
    if(argc!=6)return 2;arm_index=(unsigned)strtoul(argv[3],NULL,10);
    memset(&config,0,sizeof(config));config.policy=(sp_policy_t)VISUAL_POLICY;
    config.policy.far_budget_ms=(uint32_t)strtoul(argv[4],NULL,10);
    config.policy.far_frames=(unsigned)strtoul(argv[5],NULL,10);
    config.planner.revision=1;
    config.planner.limits=(sp_limits_t){SP_SIMULATION,3,1,2,1,2,4,6,.02f,.02f,.0001f,.05f};
    config.planner.ticks_per_metre_left=11000;config.planner.ticks_per_metre_right=12000;
    config.planner.max_left_ticks_10ms=config.planner.max_right_ticks_10ms=1000;
    config.planner.frame_max_age_ms=100;config.planner.encoder_max_age_ms=30;config.planner.candidate_max_age_ms=20;
    if(!vps_init(&state,VP_SHADOW,&config))return 3;
    input=fopen(argv[1],"r");if(!input)return 3;output=fopen(argv[2],"w");if(!output){fclose(input);return 3;}
    fprintf(output,"index,mode,authorized,inhibit,farActive,farStreak,farStartedMs,stopActive,faults,generation,armAttempt,armAccepted,status,action,leftTicks10ms,rightTicks10ms,matches\n");
    if(!fgets(line,sizeof(line),input))return 4;
    while(fgets(line,sizeof(line),input)){
        int attempt,accepted=0;
        if(sscanf(line,"%u,%u,%u,%u,%u,%u",&index,&seq,&cap,&now,&near_ok,&far_ok)!=6)return 4;
        memset(&in,0,sizeof(in));memset(&result,0,sizeof(result));
        in.identity=(vp_stamp_t){9,1,1,seq,index+1,cap,now,0};in.external_admitted=1;in.valid=VP_VALID_ALL;
        in.observation.now_ms=now;in.observation.capture_ms=cap;in.observation.frame_seq=seq;
        in.observation.near_valid=(uint8_t)near_ok;in.observation.far_valid=(uint8_t)far_ok;
        in.observation.new_frame=in.observation.encoder_valid=in.observation.capture_odometer_valid=1;
        in.observation.encoder_seq=index+1;in.observation.encoder_ms=now;
        /* Not image-derived or calibrated: straight 2.25m/3.75m fixtures,
         * zero measured/reference speed and zero distance for this replay. */
        in.near_preview.units=in.full_preview.units=SP_METRES;
        in.near_preview.count=in.full_preview.count=SP_CAPACITY;
        in.near_preview.sequence=in.full_preview.sequence=seq;
        in.near_preview.capture_ms=in.full_preview.capture_ms=cap;
        for(unsigned j=0;j<SP_CAPACITY;++j){in.near_preview.distance[j]=(float)j*.15f;in.full_preview.distance[j]=(float)j*.25f;}
        attempt=index==arm_index;
        if(attempt)accepted=vps_arm(&state,&in);else (void)vps_tick(&state,&in,&result);
        fprintf(output,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%u,%u,%d\n",index,
            (unsigned)state.supervisor.mode,(unsigned)state.supervisor.authorized,(unsigned)state.supervisor.motor_inhibit,
            (unsigned)state.supervisor.far_active,state.supervisor.far_streak,(unsigned)state.supervisor.far_started_ms,
            (unsigned)state.supervisor.stop_active,(unsigned)state.supervisor.faults,(unsigned)state.supervisor.generation,
            attempt,accepted,(unsigned)result.status,(unsigned)result.candidate.action,
            (unsigned)result.candidate.left_ticks_10ms,(unsigned)result.candidate.right_ticks_10ms,
            vps_result_matches(&state,&result,&in.identity,now));
    }
    if(ferror(input)||ferror(output)){fclose(input);fclose(output);return 5;}
    fclose(input);return fclose(output)?5:0;
}
