/* Host-only consumer of unchanged production vision. No board/actuator modules. */
#include "vision_midterm.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
static uint64_t now_ns(void) {
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count); QueryPerformanceFrequency(&frequency);
    return (uint64_t)((double)count.QuadPart * 1000000000.0 / frequency.QuadPart);
}
#else
#include <time.h>
static uint64_t now_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) exit(3);
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}
#endif

static uint8_t gray[VISION_WIDTH * VISION_HEIGHT], binary[VISION_WIDTH * VISION_HEIGHT];
static void array(const char *name, const uint8_t *values) {
    int y; printf(",\"%s\":[", name);
    for (y=0; y<VISION_HEIGHT; ++y) printf("%s%u", y ? "," : "", (unsigned)values[y]);
    printf("]");
}
int main(int argc, char **argv) {
    FILE *input, *output; int command; vision_result_t result; unsigned n=0;
    if(argc!=3) return 2;
    input=fopen(argv[1],"rb"); if(!input) return 2;
    output=fopen(argv[2],"wb"); if(!output){fclose(input); return 2;}
    /* stdout is redirected to the explicit result file, never a hardware port. */
    fclose(output);
    if(!freopen(argv[2],"wb",stdout)){fclose(input); return 2;}
    vision_midterm_reset(); vision_midterm_route_enable(1);
    while((command=fgetc(input))!=EOF) {
        uint64_t begin, elapsed; unsigned i;
        if(command!='R' && command!='C'){fclose(input); return 2;}
        if(fread(gray,1,sizeof(gray),input)!=sizeof(gray)){fclose(input); return 2;}
        if(command=='R'){vision_midterm_reset(); vision_midterm_route_enable(1);}
        begin=now_ns(); vision_midterm_process(gray,binary,&result); elapsed=now_ns()-begin;
        printf("{\"index\":%u,\"compute_ns\":%llu,\"valid\":%u,\"status\":%u,\"threshold\":%u,"
               "\"roundabout_state\":%u,\"roundabout_side\":%d,\"error_px\":%.9g,\"raw_error_px\":%.9g",
               n++,(unsigned long long)elapsed,(unsigned)result.valid,(unsigned)result.status,
               (unsigned)result.threshold,(unsigned)result.roundabout_state,(int)result.roundabout_side,
               (double)result.error_px,(double)result.raw_error_px);
        array("left",result.left); array("right",result.right); array("center",result.center); array("row_flags",result.row_flags);
        printf(",\"binary_hex\":\"");
        for(i=0;i<sizeof(binary);++i) printf("%02x",(unsigned)binary[i]);
        printf("\"}\n");
        if(ferror(stdout)){fclose(input); return 3;}
    }
    if(ferror(input)){fclose(input); return 3;}
    fclose(input); return fclose(stdout)==0 ? 0 : 3;
}
