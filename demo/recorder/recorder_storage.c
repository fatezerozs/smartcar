#include "main.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "recorder_identity.h"
#include "fsl_sd_disk.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
/* Defaults preserve the existing training entry. Isolated capture profiles
 * must explicitly select their mapped physical FatFs volume and identity. */
#ifndef RECORDER_VOLUME
#define RECORDER_VOLUME "0:"
#endif
#ifndef RECORDER_FIRMWARE_BUILD
#define RECORDER_FIRMWARE_BUILD "RECORDER_TRAIN_1"
#endif
#ifndef RECORDER_STOP_REASON
#define RECORDER_STOP_REASON "operator_record_stop"
#endif
#define SEGMENT_BYTES (64UL*1024UL*1024UL)
#define MAX_SEGMENTS 256U
#ifndef RECORDER_IO_BEGIN
#define RECORDER_IO_BEGIN() 0U
#define RECORDER_IO_END(k,t,n,ok) ((void)(t))
#endif
#ifdef RECORDER_HOST_TEST
#define SYNTHETIC_JSON "true"
#else
#define SYNTHETIC_JSON "false"
#endif
/* State: 0 boot, 1 mounting, 2 recording, 3 draining, 4 closed, 5 fault. */
volatile uint8_t recorder_storage_state,recorder_safe_to_remove,recorder_finish_requested;
volatile uint8_t recorder_storage_error;
volatile uint32_t recorder_written,recorder_synced;
static FATFS fs;
static FIL file;
static char directory[16],session_id[80],metadata[1536],text[8192];
static uint8_t uuid[16];
static uint32_t segment_index,segment_size;
static int metadata_length;
static uint64_t reported_queue,reported_pool,reported_ambiguity;
const char *recorder_session_directory(void){return directory;}
static int wait_record_ack(int wanted)
{
    TickType_t start=xTaskGetTickCount();
    while(recorder_record_ack()!=wanted) {
        if((uint32_t)(xTaskGetTickCount()-start)>=pdMS_TO_TICKS(1000U)) {
            recorder_storage_error=FR_TIMEOUT;return 0;
        }
        vTaskDelay(1);
    }
    return 1;
}
static int io_write(FIL *fp,const void *bytes,UINT size)
{
    UINT written=0;FRESULT result;uint32_t measured=RECORDER_IO_BEGIN();
    recorder_sd_operation_begin(3000U);result=f_write(fp,bytes,size,&written);
    if(recorder_sd_expired())result=FR_DISK_ERR;recorder_sd_operation_end();
    RECORDER_IO_END(0,measured,written,result==FR_OK&&written==size);
    if(result!=FR_OK || written!=size){recorder_storage_error=(uint8_t)(result==FR_OK?FR_DENIED:result);return 0;}
    return 1;
}
static int io_sync(FIL *fp)
{
    FRESULT result;uint32_t measured=RECORDER_IO_BEGIN();recorder_sd_operation_begin(3000U);result=f_sync(fp);
    if(recorder_sd_expired())result=FR_DISK_ERR;recorder_sd_operation_end();
    RECORDER_IO_END(1,measured,0,result==FR_OK);
    if(result!=FR_OK)recorder_storage_error=(uint8_t)result;return result==FR_OK;
}
static int io_close(FIL *fp)
{
    FRESULT result;uint32_t measured=RECORDER_IO_BEGIN();recorder_sd_operation_begin(3000U);result=f_close(fp);
    if(recorder_sd_expired())result=FR_DISK_ERR;recorder_sd_operation_end();
    RECORDER_IO_END(2,measured,0,result==FR_OK);
    if(result!=FR_OK)recorder_storage_error=(uint8_t)result;return result==FR_OK;
}
static int open_new(FIL *fp,const char *path)
{
    FRESULT result;recorder_sd_operation_begin(3000U);result=f_open(fp,path,FA_WRITE|FA_CREATE_NEW);
    if(recorder_sd_expired())result=FR_DISK_ERR;recorder_sd_operation_end();
    if(result!=FR_OK)recorder_storage_error=(uint8_t)result;return result==FR_OK;
}
static int create_session(void)
{
    uint32_t i,a,b,serial=g_sd.cid.productSerialNumber;
    FRESULT result;
    /* Enable reads of existing shadow words only; never reload/program OTP. */
    CLOCK_EnableClock(kCLOCK_Ocotp);__DSB();a=OCOTP->CFG0;b=OCOTP->CFG1;
    for(i=1;i<=65535U;++i) {
        (void)snprintf(directory,sizeof(directory),RECORDER_VOLUME "/R%07lX",(unsigned long)i);
        recorder_sd_operation_begin(3000U);result=f_mkdir(directory);
        if(recorder_sd_expired())result=FR_DISK_ERR;recorder_sd_operation_end();
        if(result==FR_OK)break;
        if(result!=FR_EXIST){recorder_storage_error=(uint8_t)result;return 0;}
        vTaskDelay(1);
    }
    if(i>65535U)return 0;
    rec_put32(uuid,a);rec_put32(uuid+4,b);rec_put32(uuid+8,serial);rec_put32(uuid+12,i);
    (void)snprintf(session_id,sizeof(session_id),"rec-%08lx%08lx-%08lx-%08lx",
        (unsigned long)a,(unsigned long)b,(unsigned long)serial,(unsigned long)i);
    metadata_length=snprintf(metadata,sizeof(metadata),
        "{\"format\":\"SCRAW-1\",\"schema_version\":\"1.0\",\"session_id\":\"%s\",\"synthetic\":" SYNTHETIC_JSON ","
        "\"width\":188,\"height\":120,\"stride\":188,\"pixel_format\":\"GRAY8\",\"timestamp_unit\":\"us\","
        "\"timestamp_semantics\":\"csi_irq_gpt1_ms_x1000\",\"timestamp_resolution_us\":1000,"
        "\"sequence_semantics\":\"observed_completion_events_lower_bound_on_ambiguity\","
        "\"firmware_build\":\"" RECORDER_FIRMWARE_BUILD "\",\"firmware_compiled\":\"%s %s\","
        "\"firmware_git_head\":\"" RECORDER_GIT_SHA "\",\"firmware_input_tree_sha256\":\"" RECORDER_TREE_SHA256 "\","
        "\"firmware_git_dirty\":%s,\"firmware_binary_sha256\":null,\"motion_reviewed\":%s,"
        "\"configured_sensor_fps\":%u,\"configured_exposure\":%u,\"configured_gain\":%u,"
        "\"configured_auto_exposure\":%u,\"actual_exposure_readback\":null,"
        "\"image_transform\":\"none\",\"session_uuid_hex\":\"",session_id,__DATE__,__TIME__,
        RECORDER_GIT_DIRTY?"true":"false",MIDTERM_VISUAL_REVIEWED?"true":"false",
        MT9V03X_FPS_DEF,MT9V03X_EXP_TIME_DEF,MT9V03X_GAIN_DEF,MT9V03X_AUTO_EXP_DEF);
    if(metadata_length<=0 || metadata_length+35>=(int)sizeof(metadata))return 0;
    for(i=0;i<16;++i){(void)snprintf(metadata+metadata_length,3,"%02x",uuid[i]);metadata_length+=2;}
    memcpy(metadata+metadata_length,"\"}",3);metadata_length+=2;return 1;
}
static int open_segment(void)
{
    char path[48];uint8_t head[64];
    if(segment_index>=MAX_SEGMENTS)return 0;
    (void)snprintf(path,sizeof(path),"%s/%06lu.scraw",directory,(unsigned long)segment_index);
    if(!open_new(&file,path))return 0;
    rec_segment_header(head,segment_index,uuid,(uint32_t)metadata_length,rec_crc_update(0,metadata,(size_t)metadata_length));
    segment_size=64U+(uint32_t)metadata_length;
    return io_write(&file,head,64) && io_write(&file,metadata,(UINT)metadata_length);
}
static int write_record(const char tag[4],uint64_t seq,uint64_t us,const void *payload,uint32_t n)
{
    uint8_t head[40],tail[4];uint32_t crc;
    rec_record_header(head,tag,seq,us,n,rec_crc_update(0,payload,n));
    crc=rec_crc_update(rec_crc_update(0,head,40),payload,n);rec_put32(tail,crc);
    if(!(io_write(&file,head,40) && io_write(&file,payload,n) && io_write(&file,tail,4)))return 0;
    segment_size+=n+44;return 1;
}
static int final_manifest(void)
{
    FIL manifest;char path[48];unsigned i;int n;
    /* Complete source session.json exists only after the END-bearing segment
     * has been synchronized and closed. Recovery never fabricates this file. */
    memcpy(text,metadata,(size_t)metadata_length);n=metadata_length-1;
    n+=snprintf(text+n,sizeof(text)-(size_t)n,",\"segments\":[");
    for(i=0;i<=segment_index;++i) {
        int add=snprintf(text+n,sizeof(text)-(size_t)n,"%s\"%06u.scraw\"",i?",":"",i);
        if(add<=0 || add>=(int)(sizeof(text)-(size_t)n))return 0;n+=add;
    }
    memcpy(text+n,"]}",3);n+=2;
    (void)snprintf(path,sizeof(path),"%s/session.json",directory);
    return open_new(&manifest,path) && io_write(&manifest,text,(UINT)n) && io_sync(&manifest) && io_close(&manifest);
}
static int write_gaps(void)
{
    rec_stats_t s;int n;recorder_stats(&s);
    if(s.queue_full==reported_queue && s.pool_full==reported_pool && s.ambiguous==reported_ambiguity)return 1;
    n=snprintf(text,sizeof(text),
        "{\"reason\":\"aggregated_capture_or_queue_loss\",\"queue_full_delta\":%llu,"
        "\"pool_full_delta\":%llu,\"capture_ambiguous_delta\":%llu,"
        "\"first_source_sequence\":null,\"last_source_sequence\":null,\"unknown_loss\":%s}",
        (unsigned long long)(s.queue_full-reported_queue),(unsigned long long)(s.pool_full-reported_pool),
        (unsigned long long)(s.ambiguous-reported_ambiguity),s.ambiguous>reported_ambiguity?"true":"false");
    if(n<=0 || n>=(int)sizeof(text) || !write_record("GAP1",0,0,text,(uint32_t)n))return 0;
    reported_queue=s.queue_full;reported_pool=s.pool_full;reported_ambiguity=s.ambiguous;return 1;
}
static int finish(void)
{
    rec_stats_t s;int n;recorder_stats(&s);
    n=snprintf(text,sizeof(text),
        "{\"observed_capture\":%llu,\"capture_frames\":null,\"vision_processed_frames\":%llu,"
        "\"accepted\":%llu,\"recorded_frames\":%lu,\"synced_before_end\":%lu,"
        "\"base_source_sequence\":%llu,\"first_source_sequence\":%llu,\"last_source_sequence\":%llu,"
        "\"queue_full\":%llu,\"pool_full\":%llu,\"capture_ambiguous\":%llu,"
        "\"unknown_source_loss\":%s,\"storage_errors\":0,\"stop_reason\":\"%s\","
        "\"control_gap_max_ms\":%lu,\"capture_copy_max_us\":%lu,\"vision_max_ms\":%lu,"
        "\"queue_high_water\":%lu,\"status\":\"%s\"}",
        (unsigned long long)s.observed,(unsigned long long)s.vision_processed,(unsigned long long)s.accepted,
        (unsigned long)recorder_written,(unsigned long)recorder_synced,
        (unsigned long long)s.base_source,(unsigned long long)s.first_source,(unsigned long long)s.last_source,
        (unsigned long long)s.queue_full,(unsigned long long)s.pool_full,(unsigned long long)s.ambiguous,
        s.ambiguous?"true":"false",RECORDER_STOP_REASON,(unsigned long)s.control_gap_max_ms,(unsigned long)s.capture_copy_max_us,
        (unsigned long)s.vision_max_ms,(unsigned long)s.queue_high_water,
        s.queue_full||s.pool_full||s.ambiguous?"incomplete":"complete_observed_window");
    if(n<=0 || n>=(int)sizeof(text) || !write_record("END1",0,0,text,(uint32_t)n))return 0;
    if(!io_sync(&file) || !io_close(&file))return 0;
    recorder_synced=recorder_written;
    return final_manifest();
}
void recorder_storage_task(void *unused)
{
    FRESULT result;uint32_t last_sync,drain_start=0;uint8_t at;rec_frame_t *frame;(void)unused;
    recorder_storage_state=1;
    recorder_sd_operation_begin(5000U);result=f_mount(&fs,RECORDER_VOLUME,1);
    if(recorder_sd_expired())result=FR_DISK_ERR;recorder_sd_operation_end();
    if(result!=FR_OK){recorder_storage_error=(uint8_t)result;goto fault;}
    if(!create_session() || !open_segment())goto fault;
    recorder_storage_state=2;recorder_record_request(1);last_sync=xTaskGetTickCount();
    if(!wait_record_ack(1))goto fault;
    for(;;) {
        if(recorder_finish_requested && recorder_storage_state==2) {
            recorder_storage_state=3;drain_start=xTaskGetTickCount();recorder_record_request(0);
            if(!wait_record_ack(0))goto fault;
        }
        if(recorder_write_acquire(&at,&frame)) {
            if(segment_size+REC_BYTES+44U>SEGMENT_BYTES) {
                if(!io_sync(&file) || !io_close(&file))goto fault;
                recorder_synced=recorder_written;++segment_index;
                if(!open_segment())goto fault;
            }
            if(!write_record("FRM1",frame->sequence,frame->timestamp_us,frame->pixels,REC_BYTES))goto fault;
            ++recorder_written;recorder_write_release(at);
        }else {
            if(recorder_storage_state==3)break;
            vTaskDelay(1);
        }
        if(recorder_storage_state==3 && (uint32_t)(xTaskGetTickCount()-drain_start)>=pdMS_TO_TICKS(5000U)) {
            recorder_storage_error=FR_TIMEOUT;goto fault;
        }
        if((uint32_t)(xTaskGetTickCount()-last_sync)>=pdMS_TO_TICKS(1000U)) {
            if(!write_gaps())goto fault;
            if(!io_sync(&file))goto fault;
            recorder_synced=recorder_written;last_sync=xTaskGetTickCount();
        }
    }
    if(!write_gaps() || !finish())goto fault;
    recorder_sd_operation_begin(3000U);result=f_mount(NULL,RECORDER_VOLUME,0);recorder_sd_operation_end();
    if(result!=FR_OK)goto fault;
    recorder_storage_state=4;
    for(;;) {
        /* Physical standstill is NOT sensed: this reports output inhibition
         * and closed/unmounted filesystem, not a measured stopping distance. */
        recorder_safe_to_remove=(uint8_t)(recorder_stop_drive && recorder_stopped_drive);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
fault:
    if(!recorder_storage_error)recorder_storage_error=FR_INT_ERR;
    recorder_record_request(0);recorder_sd_fault();recorder_storage_state=5;recorder_safe_to_remove=0;
    /* Do not call f_close/reinitialize after a timeout; outstanding SD DMA
     * buffers and the partially written filesystem objects stay quarantined. */
    for(;;)vTaskDelay(pdMS_TO_TICKS(100));
}
