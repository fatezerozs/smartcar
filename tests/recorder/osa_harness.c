#include "fsl_sdmmc_osa.h"
#include "recorder_sd.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>
mock_usdhc_t mock_usdhc;
unsigned mock_ipsr,mock_irq_disabled;
static TickType_t ticks;
static unsigned spurious,control_ticks;
TickType_t xTaskGetTickCount(void){return ticks;}
void vTaskDelay(TickType_t n){ticks+=n;control_ticks+=n;}
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *p){p->available=0;return p;}
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *p){p->available=1;return p;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t p,TickType_t n)
{
    if(p->available){--p->available;return pdTRUE;}
    if(spurious && n>1){--spurious;vTaskDelay(n/2);return pdTRUE;}
    vTaskDelay(n);return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t p){p->available=1;return pdTRUE;}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t p,BaseType_t *wake){*wake=pdTRUE;return xSemaphoreGive(p);}
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t p,TickType_t n){return xSemaphoreTake(p,n);}
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t p){return xSemaphoreGive(p);}
void vSemaphoreDelete(SemaphoreHandle_t p){(void)p;}
int main(void)
{
    sdmmc_osa_event_t event;uint32_t out;TickType_t start;
    assert(SDMMC_OSAEventCreate(&event)==kStatus_Success);
    recorder_sd_operation_begin(1000);
    mock_ipsr=1;assert(SDMMC_OSAEventSet(&event,1)==kStatus_Success);mock_ipsr=0;
    assert(SDMMC_OSAEventWait(&event,1,250,&out)==kStatus_Success && out==1);
    /* Spurious notifications and the previous stale semaphore must not
     * extend the per-command wait; fake delays represent OS blocking. */
    start=ticks;spurious=3;
    assert(SDMMC_OSAEventWait(&event,2,250,&out)==kStatus_Fail);
    assert(ticks-start==250 && control_ticks==250);
    recorder_sd_operation_end();
    recorder_sd_operation_begin(100);assert(SDMMC_OSADelayUs(125)==1000U);
    recorder_sd_operation_end();
    ticks=0xfffffff0U;recorder_sd_operation_begin(32);start=ticks;
    assert(SDMMC_OSAEventWait(&event,2,~0U,&out)==kStatus_Fail);
    assert(ticks-start==32 && recorder_sd_expired());
    assert(SDMMC_OSAEventWait(&event,2,~0U,&out)==kStatus_Fail && ticks-start==32);
    recorder_sd_operation_end();
    recorder_sd_operation_begin(100);recorder_sd_fault();
    assert(mock_irq_disabled && recorder_sd_failed() && recorder_sd_expired());
    assert(SDMMC_OSAEventWait(&event,2,~0U,&out)==kStatus_Fail);
    recorder_sd_operation_end();
    puts("actual SD OSA mock: absolute deadline, spurious wake, tick wrap, permanent quarantine PASS");return 0;
}
