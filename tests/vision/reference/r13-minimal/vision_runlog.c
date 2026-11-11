#include "vision_runlog.h"
#include <string.h>
#include <stdio.h>
#if !defined(RUNLOG_HOST)
#define RUNLOG_MEMORY __attribute__((section(".bss.SDRAM_CACHE"),aligned(64)))
#else
#define RUNLOG_MEMORY
#endif
RUNLOG_MEMORY static uint8_t runlog_buffer[RUNLOG_CAPACITY];
volatile runlog_status_t g_runlog;
_Static_assert(sizeof(VISION_FIELD_BUILD)<=sizeof(g_runlog.build),"Runlog build identity too long");
static uint32_t off_since,last_image;
static uint8_t was_enabled,published;
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p,uint32_t v){put16(p,(uint16_t)v);put16(p+2,(uint16_t)(v>>16));}
uint32_t runlog_crc32(const void *data,uint32_t n)
{
    const uint8_t *p=data;uint32_t c=~0U;
    while(n--){c^=*p++;for(int b=0;b<8;++b)c=(c>>1)^(0xEDB88320U&(0U-(c&1U)));}
    return ~c;
}
static void append(uint16_t kind,uint32_t now,uint32_t frame,const void *payload,uint32_t size)
{
    uint32_t at=g_runlog.used;uint8_t *p;
    if(size>RUNLOG_CAPACITY-32U||at>RUNLOG_CAPACITY-size-32U){++g_runlog.dropped;return;}
    p=runlog_buffer+at;memcpy(p,"RLG1",4);put16(p+4,kind);put16(p+6,32);
    put32(p+8,size);put32(p+12,frame);put32(p+16,now);put32(p+20,g_runlog.run);
    put32(p+24,runlog_crc32(payload,size));put32(p+28,runlog_crc32(p,28));
    memcpy(p+32,payload,size);g_runlog.used=at+32+size;++g_runlog.records;published=0;
}
void runlog_init(void)
{
    char text[180];int n;
    memset((void *)&g_runlog,0,sizeof(g_runlog));
    g_runlog.magic=0x31474C52U;g_runlog.format=1;
    g_runlog.address=(uint32_t)(uintptr_t)runlog_buffer;g_runlog.capacity=RUNLOG_CAPACITY;
    memcpy((void *)g_runlog.build,VISION_FIELD_BUILD,sizeof(VISION_FIELD_BUILD));
    was_enabled=published=0;off_since=last_image=0;
    n=snprintf(text,sizeof(text),"{\"build\":\"%s\",\"gray\":[94,60],\"metrics_bytes\":72,\"frame_id\":\"processed\",\"sd_write\":\"C26_OFF_only\"}",VISION_FIELD_BUILD);
    if(n>0&&(unsigned)n<sizeof(text))append(0,0,0,text,(uint32_t)n);
}
void runlog_poll(uint32_t now,int enabled,int stopped)
{
    uint8_t event[12];enabled=!!enabled;
    if(enabled!=was_enabled){
        if(enabled){++g_runlog.run;last_image=now-500U;}
        put32(event,(uint32_t)enabled);put32(event+4,g_runlog.used);put32(event+8,g_runlog.dropped);
        append(3,now,0,event,sizeof(event));was_enabled=(uint8_t)enabled;
        off_since=now;g_runlog.recording=enabled?1U:2U;
    }
    if(enabled||!stopped)return;
    if((uint32_t)(now-off_since)<200U)return;
    if(!published){runlog_publish(runlog_buffer,g_runlog.used);published=1;g_runlog.recording=0;}
    if(g_runlog.flushed<g_runlog.used&&g_runlog.sd_state!=3)
        runlog_store_poll(runlog_buffer,g_runlog.used,&g_runlog);
}
static int16_t scaled(float x)
{
    if(x!=x)return 0;
    if(x>32.767f)return 32767;if(x<-32.768f)return -32768;
    return (int16_t)(x*1000);
}
void runlog_capture(uint32_t now,uint32_t frame,const vision_result_t *r,
                    const uint8_t *gray,const vc_output_t *c,const vd_speed_info_t *s)
{
    uint8_t p[72]={0};
    if(g_runlog.recording!=1)return;
    put32(p,c->faults);put32(p+4,s->faults);
    put16(p+8,(uint16_t)scaled(c->lateral));put16(p+10,(uint16_t)scaled(c->heading));
    put16(p+12,(uint16_t)scaled(c->steering));put16(p+14,(uint16_t)scaled(c->duty));
    put16(p+16,(uint16_t)s->raw3);put16(p+18,(uint16_t)s->raw4);
    put16(p+20,(uint16_t)s->target_left);put16(p+22,(uint16_t)s->target_right);
    put16(p+24,(uint16_t)s->measured_left);put16(p+26,(uint16_t)s->measured_right);
    put16(p+28,s->pwm_left);put16(p+30,s->pwm_right);
    put16(p+32,(uint16_t)scaled(r->error_px*.01f));put16(p+34,(uint16_t)scaled(r->raw_error_px*.01f));
    p[36]=r->roundabout_state;p[37]=(uint8_t)r->roundabout_side;
    p[38]=r->roundabout_committed;p[39]=r->roundabout_rows;
    p[40]=r->entry_guide_rows;p[41]=r->entry_path_valid;p[42]=r->path_fallback;
    p[43]=r->valid;p[44]=r->confidence;p[45]=(uint8_t)c->state;
    p[46]=c->recovering;p[47]=c->search_source;p[48]=r->measured_rows;
    p[49]=r->inferred_rows;p[50]=r->junction_rows;p[51]=r->junction_anchors;
    p[52]=s->fault_detail;p[53]=c->observation_valid;
    put16(p+54,r->camera_width90);put32(p+56,s->samples);
    put32(p+60,(uint32_t)s->total3);put32(p+64,(uint32_t)s->total4);
    put32(p+68,s->sample_ms);
    append(1,now,frame,p,sizeof(p));
    if(gray&&(uint32_t)(now-last_image)>=500U){
        /* Keep one MiB reserved for telemetry after images reach capacity. */
        if(g_runlog.used+6000U+1024U*1024U<RUNLOG_CAPACITY){
            static uint8_t image[4+94*60+5*60];
            put16(image,94);put16(image+2,60);
            for(int y=0;y<60;++y){
                for(int x=0;x<94;++x)image[4+y*94+x]=gray[(y*2)*VISION_WIDTH+x*2];
                image[4+94*60+y*5]=r->left[y*2];image[5+94*60+y*5]=r->right[y*2];
                image[6+94*60+y*5]=r->center[y*2];image[7+94*60+y*5]=r->entry_guide[y*2];
                image[8+94*60+y*5]=r->row_flags[y*2];
            }
            append(2,now,frame,image,sizeof(image));
        }else ++g_runlog.dropped;
        last_image=now;
    }
}
