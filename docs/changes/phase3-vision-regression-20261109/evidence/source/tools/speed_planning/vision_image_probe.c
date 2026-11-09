/* Offline real-image regression probe. Native detector, no actuator access.
 * Raw 188x120 input only; caller labels replicated previews separately. */
#include "vision_planner_runtime.h"
#include "phase3_shadow_profile.h"
#include <stdio.h>
#include <string.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
int main(int argc,char **argv)
{
    if(argc!=4)return 2;
    FILE *input=fopen(argv[1],"rb"),*output=fopen(argv[2],"w"),*masks=fopen(argv[3],"wb");
    if(!input||!output||!masks)return 3;
    unsigned frame=0;vision_midterm_reset();vm_vision_t metric;vm_vision_init(&metric);
    fprintf(output,"frame,row,valid,confidence,threshold,left,right,center,flags,near_valid,full_valid,x_m,y_m\n");
    for(;;){
        size_t size=fread(gray,1,sizeof(gray),input);if(!size)break;if(size!=sizeof(gray))return 4;
        vision_result_t r;vm_frame_t f;vision_midterm_process(gray,binary,&r);
        if(!vm_vision_frame(&metric,&phase3_shadow_configuration.vision,&r,frame+1,20U*(frame+1),20U*(frame+1)+4U,&f))return 5;
        for(unsigned y=0;y<VISION_HEIGHT;++y){
            const float *h=phase3_shadow_configuration.vision.homography;float x=0,z=1,gx=0,gy=0;
            if(r.row_flags[y]==3){x=(r.left[y]+r.right[y])*.5f;z=h[6]*x+h[7]*(float)y+h[8];
                if(z>1e-6f||z< -1e-6f){gx=(h[0]*x+h[1]*(float)y+h[2])/z;gy=(h[3]*x+h[4]*(float)y+h[5])/z;}}
            fprintf(output,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.8g,%.8g\n",frame,y,r.valid,r.confidence,
                r.row_threshold[y],r.left[y],r.right[y],r.center[y],r.row_flags[y],f.near_valid,f.far_valid,(double)gx,(double)gy);
        }
        if(fwrite(binary,1,sizeof(binary),masks)!=sizeof(binary))return 6;++frame;
    }
    if(ferror(input)||ferror(output)||ferror(masks))return 7;
    fclose(input);if(fclose(output)||fclose(masks))return 7;
    printf("VISION IMAGE PROBE: %u frames; original native threshold, row geometry and metric acceptance.\n",frame);
    return frame?0:8;
}
