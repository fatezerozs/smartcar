/* Host-only bridge: real observed flags/times, explicit synthetic encoder.
 * No image processing, metric planning, PI, actuator or hardware access. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "speed_planner.h"
#include "visual_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv)
{
    FILE *input,*output;char line[512];unsigned index,seq,cap,now,near_ok,far_ok;
    unsigned arm_index,budget,recovery;sp_supervisor_t state;sp_policy_t policy=VISUAL_POLICY;
    if(argc!=6)return 2;
    arm_index=(unsigned)strtoul(argv[3],NULL,10);
    budget=(unsigned)strtoul(argv[4],NULL,10);recovery=(unsigned)strtoul(argv[5],NULL,10);
    policy.far_budget_ms=budget;policy.far_frames=recovery;
    input=fopen(argv[1],"r");if(!input)return 3;
    output=fopen(argv[2],"w");if(!output){fclose(input);return 3;}
    sp_supervisor_init(&state);
    fprintf(output,"index,mode,authorized,inhibit,farActive,farStreak,farStartedMs,stopActive,faults,generation,armAttempt,armAccepted\n");
    if(!fgets(line,sizeof(line),input))return 4;
    while(fgets(line,sizeof(line),input)){
        sp_input_t observation;int attempt,accepted=0;
        if(sscanf(line,"%u,%u,%u,%u,%u,%u",&index,&seq,&cap,&now,&near_ok,&far_ok)!=6)return 4;
        memset(&observation,0,sizeof(observation));
        observation.now_ms=now;observation.capture_ms=cap;observation.frame_seq=seq;
        observation.near_valid=(uint8_t)near_ok;observation.far_valid=(uint8_t)far_ok;
        observation.new_frame=1;
        /* These values are fixtures, never measurements from the static trace. */
        observation.encoder_valid=observation.capture_odometer_valid=1;
        observation.encoder_seq=index+1;observation.encoder_ms=now;
        attempt=index==arm_index;
        if(attempt)accepted=sp_supervisor_arm(&state,&policy,&observation);
        else sp_supervise(&state,&policy,&observation);
        fprintf(output,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d\n",index,
            (unsigned)state.mode,(unsigned)state.authorized,(unsigned)state.motor_inhibit,
            (unsigned)state.far_active,state.far_streak,(unsigned)state.far_started_ms,
            (unsigned)state.stop_active,(unsigned)state.faults,(unsigned)state.generation,attempt,accepted);
    }
    if(ferror(input)||ferror(output)){fclose(input);fclose(output);return 5;}
    fclose(input);return fclose(output)?5:0;
}
