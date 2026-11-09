/* Native snapshot input replay, no device access. Scales are mandatory CLI
 * inputs. Capture queries are synthetic midpoint times within logged samples. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "vision_metric_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    vm_odom_t state;vm_odom_config_t config;char line[256],*end;FILE *input,*output;
    unsigned sequence,ms,previous_ms=0,index=0;int left,right;
    if(argc!=5)return 2;
    memset(&config,0,sizeof(config));config.revision=1;config.max_age_ms=30;config.interpolation_gap_ms=20;
    config.sign_left=-1;config.sign_right=1;
    config.ticks_per_metre_left=strtof(argv[3],&end);if(*end)return 2;
    config.ticks_per_metre_right=strtof(argv[4],&end);if(*end)return 2;
    if(!vm_odom_init(&state,&config))return 2;
    input=fopen(argv[1],"r");if(!input)return 3;output=fopen(argv[2],"w");if(!output){fclose(input);return 3;}
    fprintf(output,"index,sequence,ms,odometerM,leftSpeedMps,rightSpeedMps,speedMps,velocityValid,captureMs,captureStatus,captureOdometerM\n");
    if(!fgets(line,sizeof(line),input))return 4;
    while(fgets(line,sizeof(line),input)){
        vd_speed_info_t native;vm_encoder_t sample;vm_motion_t motion;float captured=0;uint32_t capture;
        if(sscanf(line,"%u,%u,%d,%d",&sequence,&ms,&left,&right)!=4)return 4;
        memset(&native,0,sizeof(native));native.samples=sequence;native.sample_ms=ms;
        native.total3=left;native.total4=right;native.sign3=-1;native.sign4=1;native.signs_confirmed=1;
        if(!vm_encoder_from_speed(&native,1,&sample)||vm_odom_push(&state,&sample)!=VM_OK||
           vm_odom_current(&state,ms,&motion)!=VM_OK)return 5;
        capture=index?previous_ms+(uint32_t)(ms-previous_ms)/2U:ms;
        vm_status_t status=vm_odom_at_capture(&state,capture,ms,&captured);
        fprintf(output,"%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%u,%u,%u,%.9g\n",index,sequence,ms,
            (double)motion.odometer_m,(double)motion.left_speed_m_s,(double)motion.right_speed_m_s,
            (double)motion.speed_m_s,(unsigned)motion.velocity_valid,(unsigned)capture,(unsigned)status,(double)captured);
        previous_ms=ms;++index;
    }
    if(ferror(input)||ferror(output)){fclose(input);fclose(output);return 6;}
    fclose(input);return fclose(output)?6:0;
}
