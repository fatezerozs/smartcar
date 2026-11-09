#include "vision_midterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static uint8_t binary[VISION_WIDTH*VISION_HEIGHT];
int main(int argc,char **argv){if(argc!=2)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 3;
size_t count=750U*VISION_WIDTH*VISION_HEIGHT;uint8_t *g=malloc(count);if(!g)return 4;if(fread(g,1,count,f)!=count)return 5;fclose(f);volatile unsigned checksum=0;clock_t start=clock();
for(unsigned k=0;k<5;++k){vision_midterm_reset();for(unsigned i=0;i<750;++i){vision_result_t r;vision_midterm_process(g+i*VISION_WIDTH*VISION_HEIGHT,binary,&r);checksum+=r.confidence+r.measured_rows;}}
printf("{\"milliseconds\":%.3f,\"frames\":3750,\"checksum\":%u}\n",1000.0*(clock()-start)/CLOCKS_PER_SEC,checksum);free(g);return 0;}
