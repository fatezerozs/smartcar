#include "main.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "fsl_sd_disk.h"
#include "task.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern volatile uint8_t recorder_finish_requested;
static jmp_buf done;
static unsigned scenario,total=3,next,released,ticks,active,faulted,writes,syncs,closes,files;
static uint32_t deadline;
static uint8_t ack;
static rec_frame_t frame;
static struct {char name[64];unsigned size;unsigned char bytes[100000];} output[4];
volatile uint8_t recorder_stop_drive,recorder_stopped_drive=1,recorder_recording;
mock_ocotp_t mock_ocotp={0x12345678,0x90abcdef};
sd_card_t g_sd={{0x1234}};
TickType_t xTaskGetTickCount(void){return ticks;}
void vTaskDelay(TickType_t n)
{
    ticks+=n;
    if(recorder_storage_state==4 || recorder_storage_state==5)longjmp(done,1);
    assert(ticks<20000U);
}
void recorder_sd_operation_begin(uint32_t n){assert(!active);active=1;deadline=ticks+n;}
void recorder_sd_operation_end(void){assert(active);active=0;}
int recorder_sd_expired(void){return faulted || (active && ticks>=deadline);}
int recorder_sd_failed(void){return faulted;}
void recorder_sd_fault(void){faulted=1;}
void recorder_record_request(int on){ack=recorder_recording=(uint8_t)on;}
int recorder_record_ack(void){return ack;}
int recorder_write_acquire(uint8_t *at,rec_frame_t **out)
{
    if(next==total)return 0;
    assert(next==released);frame.sequence=++next;frame.timestamp_us=(uint64_t)next*20000;
    memset(frame.pixels,(int)next,REC_BYTES);*at=(uint8_t)next;*out=&frame;return 1;
}
void recorder_write_release(uint8_t at){assert(at==(uint8_t)(released+1));++released;}
void recorder_stats(rec_stats_t *s)
{
    memset(s,0,sizeof(*s));s->observed=s->accepted=total;s->vision_processed=total;
    s->first_source=1;s->last_source=total;s->queue_high_water=3;
    if(scenario==9){s->queue_full=1;++s->observed;++s->last_source;}
}
FRESULT f_mount(FATFS *fs,const char *path,unsigned immediate)
{(void)fs;(void)path;(void)immediate;assert(active);return scenario==1?FR_DISK_ERR:FR_OK;}
FRESULT f_mkdir(const char *path){(void)path;assert(active);return FR_OK;}
FRESULT f_open(FIL *fp,const char *path,unsigned mode)
{
    unsigned i;assert(active && mode==(FA_WRITE|FA_CREATE_NEW) && files<4);
    for(i=0;i<files;++i)assert(strcmp(output[i].name,path));
    fp->index=files++;strcpy(output[fp->index].name,path);return FR_OK;
}
FRESULT f_write(FIL *fp,const void *data,UINT n,UINT *written)
{
    unsigned at=fp->index,size=output[at].size;assert(active);++writes;
    if(scenario==10)vTaskDelay(1000);
    if(scenario==2 && writes==4){*written=n-1;return FR_OK;}
    if(scenario==5 && writes==4){*written=0;return FR_DENIED;}
    *written=n;
    if(size+n<=sizeof(output[at].bytes))memcpy(output[at].bytes+size,data,n);
    output[at].size+=n;return FR_OK;
}
FRESULT f_sync(FIL *fp){(void)fp;assert(active);++syncs;return scenario==3?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL *fp){(void)fp;assert(active);++closes;return scenario==4?FR_DISK_ERR:FR_OK;}
int main(int argc,char **argv)
{
    unsigned i;scenario=argc>1?(unsigned)atoi(argv[1]):0;
    if(scenario==8)total=3000;
    recorder_finish_requested=1;if(scenario==7)recorder_stop_drive=1;
    if(!setjmp(done))recorder_storage_task(NULL);
    if((scenario>=1 && scenario<=5) || scenario==10) {
        assert(recorder_storage_state==5 && faulted && !recorder_safe_to_remove && !ack);
        assert(!recorder_stop_drive); /* Storage failure never stops/arms Control. */
        if(scenario==2 || scenario==5)assert(!recorder_written && !released);
        if(scenario==3 || scenario==4)assert(recorder_written==3 && files==1);
        if(scenario==10)assert(recorder_written<3 && recorder_storage_error==FR_TIMEOUT);
    }else {
        assert(recorder_storage_state==4 && !faulted && !ack);
        assert(recorder_written==total && released==total && recorder_synced==total);
        assert(recorder_safe_to_remove==(scenario==7));
        assert(files==(scenario==8?3U:2U));assert(syncs && closes);
    }
    if(argc==3)for(i=0;i<files;++i) {
        char path[512];FILE *f;const char *name=strrchr(output[i].name,'/');assert(name);
        assert(output[i].size<=sizeof(output[i].bytes));
        assert(snprintf(path,sizeof(path),"%s/%s",argv[2],name+1)<(int)sizeof(path));
        f=fopen(path,"wb");assert(f);assert(fwrite(output[i].bytes,1,output[i].size,f)==output[i].size);assert(!fclose(f));
    }
    printf("actual Storage task mock case=%u state=%u written=%lu files=%u PASS\n",scenario,recorder_storage_state,(unsigned long)recorder_written,files);
    return 0;
}
