#define _CRT_SECURE_NO_WARNINGS
#include "vision_midterm.h"
#include "vision_control.h"
#include <stdio.h>
#include <stdlib.h>

/* Diagnostic replay of the captured 94x60 preview, not the missing original
 * 188x120 frame. Replicating each sample 2x2 cannot restore camera detail.
 * Useful for comparing candidate algorithms; never a real-board acceptance. */
static uint8_t preview[VISION_PREVIEW_WIDTH * VISION_PREVIEW_HEIGHT];
static uint8_t gray[VISION_WIDTH * VISION_HEIGHT], binary[VISION_WIDTH * VISION_HEIGHT];
int main(int argc, char **argv)
{
    FILE *input;
    vision_result_t result;
    vc_config_t config;
    vc_controller_t control;
    int x,y;
    if(argc!=2){fprintf(stderr,"usage: replay_preview capture.gray\n");return 2;}
    input=fopen(argv[1],"rb");
    if(!input){perror(argv[1]);return 2;}
    if(fread(preview,1,sizeof(preview),input)!=sizeof(preview) || fgetc(input)!=EOF){
        fprintf(stderr,"Expected exactly 5640 bytes (94x60 preview).\n");fclose(input);return 2;
    }
    fclose(input);
    for(y=0;y<VISION_HEIGHT;++y)for(x=0;x<VISION_WIDTH;++x)
        gray[y*VISION_WIDTH+x]=preview[(y/2)*VISION_PREVIEW_WIDTH+x/2];
    vision_midterm_reset();vision_midterm_process(gray,binary,&result);
    vc_default_config(&config);
    /* Current static field trial bands only, not a new production default. */
    config.near_y=.78f;config.far_y=.68f;config.band_half_height=.025f;
    vc_init(&control,&config);vc_observe(&control,&result,1,20,20);
    printf("PREVIEW REPLAY ONLY (2x2 replicated, not camera raw): valid=%u q=%u both=%u threshold=%u "
           "control_valid=%u far_ok=%u near_x=%.5f far_x=%.5f\n",
           result.valid,result.confidence,result.measured_rows,result.threshold,
           control.output.observation_valid,control.output.far_valid,
           (double)control.output.near_center_x,(double)control.output.far_center_x);
    return 0;
}
