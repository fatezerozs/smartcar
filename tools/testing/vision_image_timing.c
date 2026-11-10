/* Host-only timing of the production detector and metric adapter. Images are
 * preloaded; reset between complete passes, retain temporal state within each
 * pass. No file IO, allocation, rendering or diagnostic work in timed regions. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "vision_planner_runtime.h"
#include "phase3_shadow_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static double frequency;
static double tick(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (double)t.QuadPart;}
static int clock_init(void){LARGE_INTEGER f;if(!QueryPerformanceFrequency(&f))return 0;frequency=(double)f.QuadPart;return 1;}
#else
#include <time.h>
static double frequency=1e9;
static double tick(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec*1e9+(double)t.tv_nsec;}
static int clock_init(void){return 1;}
#endif
enum { PIXELS=VISION_WIDTH*VISION_HEIGHT };
typedef struct {double vision_ms,metric_ms;unsigned hash,near_valid,full_valid,valid;} measurement_t;
static unsigned hash_bytes(unsigned h,const void *p,size_t size)
{const unsigned char *s=p;for(size_t i=0;i<size;++i){h^=s[i];h*=16777619U;}return h;}
int main(int argc,char **argv)
{
    if((argc!=6&&argc!=7)||!clock_init())return 2;
    int route_active=argc==7&&!strcmp(argv[6],"route-active");
    if(argc==7&&!route_active)return 2;
    unsigned count=(unsigned)strtoul(argv[3],NULL,10),warmup=(unsigned)strtoul(argv[4],NULL,10),repeats=(unsigned)strtoul(argv[5],NULL,10);
    if(!count||count>10000||warmup>20||repeats<2||repeats>100)return 2;
    unsigned char *images=malloc((size_t)count*PIXELS),*binary=malloc(PIXELS);
    measurement_t *records=calloc(count,sizeof(*records));if(!images||!binary||!records)return 3;
    FILE *input=fopen(argv[1],"rb");if(!input)return 4;
    if(fread(images,PIXELS,count,input)!=count||fgetc(input)!=EOF||ferror(input))return 5;
    fclose(input);FILE *output=fopen(argv[2],"w");if(!output)return 4;
    fprintf(output,"repetition,index,vision_ms,metric_ms,output_hash,raw_valid,near_valid,full_valid\n");
    for(unsigned pass=0;pass<warmup+repeats;++pass){
        vm_vision_t metric;vm_vision_init(&metric);vision_midterm_reset();vision_midterm_route_enable((uint8_t)route_active);
        for(unsigned i=0;i<count;++i){
            vision_result_t road;vm_frame_t frame;measurement_t *m=&records[i];
            double start=tick();vision_midterm_process(images+(size_t)i*PIXELS,binary,&road);double end=tick();
            m->vision_ms=(end-start)*1000/frequency;
            start=tick();int ok=vm_vision_frame(&metric,&phase3_shadow_configuration.vision,&road,i+1,20U*(i+1),20U*(i+1)+4U,&frame);end=tick();
            if(!ok)return 6;m->metric_ms=(end-start)*1000/frequency;
            /* Compare semantic outputs across passes, without struct padding. */
            m->hash=hash_bytes(2166136261U,road.left,VISION_HEIGHT);
            m->hash=hash_bytes(m->hash,road.right,VISION_HEIGHT);m->hash=hash_bytes(m->hash,road.center,VISION_HEIGHT);
            m->hash=hash_bytes(m->hash,road.row_flags,VISION_HEIGHT);m->hash=hash_bytes(m->hash,binary,PIXELS);
            m->valid=road.valid;m->near_valid=frame.near_valid;m->full_valid=frame.far_valid;
        }
        if(pass>=warmup)for(unsigned i=0;i<count;++i){measurement_t *m=&records[i];
            fprintf(output,"%u,%u,%.9f,%.9f,%u,%u,%u,%u\n",pass-warmup,i,m->vision_ms,m->metric_ms,m->hash,m->valid,m->near_valid,m->full_valid);
        }
    }
    if(ferror(output)||fclose(output))return 7;free(records);free(binary);free(images);
    printf("TIMING: %u frames x %u measured passes, %u excluded warmup passes; high-resolution monotonic host clock.\n",count,repeats,warmup);return 0;
}
