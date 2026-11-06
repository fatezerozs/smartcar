#include "fsl_sdmmc_osa.h"
#include "recorder_sd.h"
#include "task.h"
static TickType_t deadline;
static int active,failed;
void recorder_sd_operation_begin(uint32_t ms)
{configASSERT(!active);deadline=xTaskGetTickCount()+pdMS_TO_TICKS(ms);active=1;}
void recorder_sd_operation_end(void){active=0;}
int recorder_sd_expired(void)
{return failed || !active || (int32_t)(xTaskGetTickCount()-deadline)>=0;}
int recorder_sd_failed(void){return failed;}
void recorder_sd_fault(void)
{
    /* Permanent quarantine until MCU reset. No subsequent reuse of SD DMA
     * bounce/internal buffers; no hot-reinitialization of the shared PLL. */
    failed=1;NVIC_DisableIRQ(USDHC1_IRQn);
    USDHC1->INT_SIGNAL_EN=0;__DSB();
}
static TickType_t remaining(uint32_t ms)
{
    TickType_t n,left;
    if(recorder_sd_expired())return 0;
    left=deadline-xTaskGetTickCount();n=ms==~0U?left:pdMS_TO_TICKS(ms);
    if(ms && !n)n=1;
    return n<left?n:left;
}
void SDMMC_OSAInit(void){}
status_t SDMMC_OSAEventCreate(void *p)
{sdmmc_osa_event_t *e=p;e->flags=0;e->sem=xSemaphoreCreateBinaryStatic(&e->storage);return e->sem?kStatus_Success:kStatus_Fail;}
status_t SDMMC_OSAEventWait(void *p,uint32_t mask,uint32_t ms,uint32_t *out)
{
    sdmmc_osa_event_t *e=p;TickType_t end=xTaskGetTickCount()+remaining(ms),n;
    for(;;) {
        if(recorder_sd_expired())return kStatus_Fail;
        taskENTER_CRITICAL();*out=e->flags&mask;e->flags&=~(*out);taskEXIT_CRITICAL();
        if(*out)return kStatus_Success;
        if((int32_t)(xTaskGetTickCount()-end)>=0)return kStatus_Fail;
        n=end-xTaskGetTickCount();
        if(xSemaphoreTake(e->sem,n)!=pdTRUE)return kStatus_Fail;
        /* Spurious/unrelated wakes never restart either deadline. */
    }
}
status_t SDMMC_OSAEventSet(void *p,uint32_t mask)
{
    sdmmc_osa_event_t *e=p;
    if(__get_IPSR()) {
        BaseType_t wake=pdFALSE;UBaseType_t saved=taskENTER_CRITICAL_FROM_ISR();
        e->flags|=mask;taskEXIT_CRITICAL_FROM_ISR(saved);
        xSemaphoreGiveFromISR(e->sem,&wake);portYIELD_FROM_ISR(wake);
    }else {taskENTER_CRITICAL();e->flags|=mask;taskEXIT_CRITICAL();xSemaphoreGive(e->sem);}
    return kStatus_Success;
}
status_t SDMMC_OSAEventGet(void *p,uint32_t mask,uint32_t *out)
{*out=((sdmmc_osa_event_t *)p)->flags&mask;return kStatus_Success;}
status_t SDMMC_OSAEventClear(void *p,uint32_t mask)
{
    sdmmc_osa_event_t *e=p;
    if(__get_IPSR()){UBaseType_t saved=taskENTER_CRITICAL_FROM_ISR();e->flags&=~mask;taskEXIT_CRITICAL_FROM_ISR(saved);}
    else {taskENTER_CRITICAL();e->flags&=~mask;taskEXIT_CRITICAL();}
    return kStatus_Success;
}
status_t SDMMC_OSAEventDestroy(void *p){vSemaphoreDelete(((sdmmc_osa_event_t *)p)->sem);return kStatus_Success;}
status_t SDMMC_OSAMutexCreate(void *p)
{sdmmc_osa_mutex_t *m=p;m->sem=xSemaphoreCreateRecursiveMutexStatic(&m->storage);return m->sem?kStatus_Success:kStatus_Fail;}
status_t SDMMC_OSAMutexLock(void *p,uint32_t ms)
{return xSemaphoreTakeRecursive(((sdmmc_osa_mutex_t *)p)->sem,remaining(ms))==pdTRUE?kStatus_Success:kStatus_Fail;}
status_t SDMMC_OSAMutexUnlock(void *p)
{return xSemaphoreGiveRecursive(((sdmmc_osa_mutex_t *)p)->sem)==pdTRUE?kStatus_Success:kStatus_Fail;}
status_t SDMMC_OSAMutexDestroy(void *p){vSemaphoreDelete(((sdmmc_osa_mutex_t *)p)->sem);return kStatus_Success;}
void SDMMC_OSADelay(uint32_t ms){TickType_t n=remaining(ms);if(n)vTaskDelay(n);}
uint32_t SDMMC_OSADelayUs(uint32_t us)
{
    TickType_t start=xTaskGetTickCount(),n;
    /* Tick-resolution delay, never a claim of microsecond precision.
     * The SDK's subtract-without-saturation busy loop is replaced separately. */
    if(us && !recorder_sd_expired()) {
        n=remaining((us+999U)/1000U);if(n)vTaskDelay(n);
    }
    return (uint32_t)(xTaskGetTickCount()-start)*1000U;
}
