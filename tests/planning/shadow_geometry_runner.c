/* Offline probe: actual original images and current native vision + measured
 * candidate mapping. No motion, PI, motor authorization or physical truth. */
#include "vision_planner_runtime.h"
#include "phase3_shadow_profile.h"
#include <stdio.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
int main(int argc,char **argv)
{
    if(argc!=3)return 2;FILE *in=fopen(argv[1],"rb"),*out=fopen(argv[2],"w");
    if(!in||!out)return 3;
    vision_midterm_reset();vm_vision_t state;vm_vision_init(&state);
    unsigned frames=0,near=0,far=0;
    fprintf(out,"index,raw_valid,confidence,centre70,flags70,near_geometry,full_geometry\n");
    while(fread(gray,1,sizeof(gray),in)==sizeof(gray)){
        vision_result_t road;vm_frame_t mapped;
        vision_midterm_process(gray,binary,&road);
        vm_vision_frame(&state,&phase3_shadow_configuration.vision,&road,frames+1,100+frames*20,104+frames*20,&mapped);
        near+=mapped.near_valid;far+=mapped.far_valid;
        fprintf(out,"%u,%u,%u,%u,%u,%u,%u\n",frames,road.valid,road.confidence,road.center[70],road.row_flags[70],mapped.near_valid,mapped.far_valid);
        ++frames;
    }
    if(ferror(in)||ferror(out))return 4;fclose(in);if(fclose(out))return 4;
    printf("SHADOW GEOMETRY frames=%u near=%u full=%u; native replay, no physical validity claim\n",frames,near,far);
    return frames==750?0:5;
}
