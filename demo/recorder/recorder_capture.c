#include "main.h"
#include "recorder_runtime.h"
#include "task.h"
#include <string.h>
AT_SDRAM_NONCACHE_SECTION_ALIGN(static uint8_t raw[REC_CAPTURE_SLOTS][REC_ALIGNED_BYTES],64);
AT_SDRAM_NONCACHE_SECTION_ALIGN(static rec_frame_t recording[REC_QUEUE_SLOTS],64);
/* sizeof(rec_frame_t) must be a cache-line multiple, not merely its pixels. */
typedef char rec_frame_alignment_check[(sizeof(rec_frame_t)%64U==0U)?1:-1];
AT_SDRAM_SECTION_ALIGN(static rec_frame_t vision[3],64);
static uint8_t vision_owner[3],pending=REC_NONE;
static rec_capture_t capture;
static rec_stats_t counters;
static uint8_t free_bytes[REC_QUEUE_SLOTS],filled_bytes[REC_QUEUE_SLOTS];
static StaticQueue_t free_storage,filled_storage;
static QueueHandle_t free_queue,filled_queue;
static volatile uint8_t requested,ack;
volatile uint8_t recorder_recording;
static uint64_t baseline_ambiguous,baseline_pool;
extern void recorder_publish_vision(const rec_frame_t *frame);
void recorder_capture_prepare(void){rec_capture_init(&capture);}
void recorder_runtime_init(void)
{
    unsigned i;free_queue=xQueueCreateStatic(REC_QUEUE_SLOTS,1,free_bytes,&free_storage);
    filled_queue=xQueueCreateStatic(REC_QUEUE_SLOTS,1,filled_bytes,&filled_storage);
    configASSERT(free_queue && filled_queue);
    for(i=0;i<REC_QUEUE_SLOTS;++i){uint8_t at=(uint8_t)i;configASSERT(xQueueSend(free_queue,&at,0)==pdTRUE);}
}
void recorder_capture_start(csi_handle_t *unused)
{
    (void)unused;NVIC_DisableIRQ(CSI_IRQn);
    CSI_REG_CR18(CSI)=(CSI_REG_CR18(CSI)&~CSI_CR18_MASK_OPTION_MASK)|
        CSI_CR18_MASK_OPTION(0)|CSI_CR18_BASEADDR_SWITCH_SEL_MASK|CSI_CR18_BASEADDR_SWITCH_EN_MASK;
    CSI_SetRxBufferAddr(CSI,0,(uint32_t)raw[0]);CSI_SetRxBufferAddr(CSI,1,(uint32_t)raw[1]);
    CSI_ClearStatusFlags(CSI,CSI_GetStatusFlags(CSI));
    CSI_ReflashFifoDma(CSI,kCSI_RxFifo);
    CSI_EnableInterrupts(CSI,kCSI_RxBuffer0DmaDoneInterruptEnable|kCSI_RxBuffer1DmaDoneInterruptEnable);
    NVIC_SetPriority(CSI_IRQn,2U);__DSB();CSI_Start(CSI);NVIC_EnableIRQ(CSI_IRQn);
}
void recorder_capture_irq(void)
{
    static uint32_t previous;static uint64_t epoch;
    uint32_t flags=CSI_GetStatusFlags(CSI),done=flags&
        (kCSI_RxBuffer0DmaDoneFlag|kCSI_RxBuffer1DmaDoneFlag),ms=timer_get(GPT_TIM_1);
    uint8_t old,next;CSI_ClearStatusFlags(CSI,flags);
    if(ms<previous)epoch+=UINT64_C(0x100000000);previous=ms;
    if(done==(kCSI_RxBuffer0DmaDoneFlag|kCSI_RxBuffer1DmaDoneFlag) ||
       (flags&kCSI_RxFifoOverrunFlag)) {
        unsigned observed=((done&kCSI_RxBuffer0DmaDoneFlag)?1U:0U)+
            ((done&kCSI_RxBuffer1DmaDoneFlag)?1U:0U);
        rec_capture_ambiguous(&capture,observed);__DSB();return;
    }
    if(done && rec_capture_prepare(&capture,done==kCSI_RxBuffer1DmaDoneFlag?1U:0U,
        (epoch+ms)*1000U,&old,&next)) {
        CSI_SetRxBufferAddr(CSI,done==kCSI_RxBuffer1DmaDoneFlag?1U:0U,(uint32_t)raw[next]);
        __DSB();configASSERT(rec_capture_publish(&capture,old));
    }
    __DSB();
}
void recorder_record_request(int on){requested=(uint8_t)(on!=0);__DMB();}
int recorder_record_ack(void){return ack;}
void recorder_stats(rec_stats_t *out)
{
    uint32_t saved=__get_PRIMASK();__disable_irq();*out=counters;
    if(ack) {
        out->ambiguous=capture.ambiguous-baseline_ambiguous;
        out->pool_full=capture.exhausted-baseline_pool;
    }
    __set_PRIMASK(saved);
}
int recorder_write_acquire(uint8_t *at,rec_frame_t **frame)
{if(xQueueReceive(filled_queue,at,0)!=pdTRUE)return 0;*frame=&recording[*at];return 1;}
void recorder_write_release(uint8_t at){configASSERT(xQueueSend(free_queue,&at,0)==pdTRUE);}
static void offer_vision(const uint8_t *pixels,uint64_t seq,uint64_t us)
{
    unsigned i;taskENTER_CRITICAL();
    for(i=0;i<3;++i)if(vision_owner[i]==0)break;
    if(i<3)vision_owner[i]=1;taskEXIT_CRITICAL();
    if(i==3)return;
    vision[i].sequence=seq;vision[i].timestamp_us=us;memcpy(vision[i].pixels,pixels,REC_BYTES);
    taskENTER_CRITICAL();
    if(pending!=REC_NONE)vision_owner[pending]=0;
    pending=(uint8_t)i;vision_owner[i]=2;taskEXIT_CRITICAL();
}
void recorder_capture_task(void *unused)
{
    (void)unused;
    for(;;) {
        int at;uint32_t saved,start;uint64_t seq,us;
        if(requested!=ack) {
            saved=__get_PRIMASK();__disable_irq();
            if(requested) {
                counters.base_source=capture.observed;baseline_ambiguous=capture.ambiguous;
                baseline_pool=capture.exhausted;
            }else {
                counters.ambiguous=capture.ambiguous-baseline_ambiguous;
                counters.pool_full=capture.exhausted-baseline_pool;
            }
            recorder_recording=ack=requested;__DMB();__set_PRIMASK(saved);
        }
        saved=__get_PRIMASK();__disable_irq();at=rec_capture_acquire(&capture);__set_PRIMASK(saved);
        if(at==REC_NONE){vTaskDelay(1);continue;}
        seq=capture.slot[at].sequence;us=capture.slot[at].timestamp_us;
        start=DWT->CYCCNT;
        offer_vision(raw[at],seq,us);
        if(ack && seq>counters.base_source) {
            uint8_t dest;
            if(!counters.first_source)counters.first_source=seq;
            counters.last_source=seq;counters.observed=seq-counters.base_source;
            if(xQueueReceive(free_queue,&dest,0)==pdTRUE) {
                recording[dest].sequence=seq;recording[dest].timestamp_us=us;
                memcpy(recording[dest].pixels,raw[at],REC_BYTES);
                configASSERT(xQueueSend(filled_queue,&dest,0)==pdTRUE);++counters.accepted;
                {uint32_t depth=uxQueueMessagesWaiting(filled_queue);
                 if(depth>counters.queue_high_water)counters.queue_high_water=depth;}
            }else ++counters.queue_full;
        }
        {uint32_t elapsed=(DWT->CYCCNT-start)/(SystemCoreClock/1000000U);
         if(elapsed>counters.capture_copy_max_us)counters.capture_copy_max_us=elapsed;}
        saved=__get_PRIMASK();__disable_irq();configASSERT(rec_capture_release(&capture,(unsigned)at));__set_PRIMASK(saved);
        /* One frame per release; even at camera saturation lower tasks run. */
        vTaskDelay(1);
    }
}
void recorder_vision_task(void *unused)
{
    extern void recorder_vision_init(void);(void)unused;recorder_vision_init();
    for(;;) {
        uint8_t at;uint32_t start=timer_get(GPT_TIM_1);
        taskENTER_CRITICAL();at=pending;pending=REC_NONE;
        if(at!=REC_NONE)vision_owner[at]=3;taskEXIT_CRITICAL();
        if(at!=REC_NONE) {
            recorder_publish_vision(&vision[at]);
            taskENTER_CRITICAL();vision_owner[at]=0;
            if(ack && vision[at].sequence>counters.base_source)++counters.vision_processed;
            {uint32_t elapsed=timer_get(GPT_TIM_1)-start;if(elapsed>counters.vision_max_ms)counters.vision_max_ms=elapsed;}
            taskEXIT_CRITICAL();
        }
        vTaskDelay(1);
    }
}
void recorder_control_gap(uint32_t gap)
{if(gap>counters.control_gap_max_ms)counters.control_gap_max_ms=gap;}
