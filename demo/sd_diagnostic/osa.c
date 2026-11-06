#include "diag.h"
#include "fsl_sdmmc_osa.h"
void SDMMC_OSAInit(void){}
status_t SDMMC_OSAEventCreate(void *p){((sdmmc_osa_event_t*)p)->eventFlag=0;return kStatus_Success;}
status_t SDMMC_OSAEventGet(void *p,uint32_t m,uint32_t *o){*o=((sdmmc_osa_event_t*)p)->eventFlag&m;return kStatus_Success;}
status_t SDMMC_OSAEventClear(void *p,uint32_t m){uint32_t s=__get_PRIMASK();__disable_irq();((sdmmc_osa_event_t*)p)->eventFlag&=~m;__set_PRIMASK(s);return kStatus_Success;}
status_t SDMMC_OSAEventSet(void *p,uint32_t m){uint32_t s=__get_PRIMASK();__disable_irq();((sdmmc_osa_event_t*)p)->eventFlag|=m;__set_PRIMASK(s);return kStatus_Success;}
status_t SDMMC_OSAEventWait(void *p,uint32_t m,uint32_t ms,uint32_t *o){uint32_t t=diag_ms();if(ms>1000)ms=1000;do{uint32_t s=__get_PRIMASK();__disable_irq();SDMMC_OSAEventGet(p,m,o);SDMMC_OSAEventClear(p,*o);__set_PRIMASK(s);if(*o)return kStatus_Success;}while(diag_ms()-t<ms);return kStatus_Fail;}
status_t SDMMC_OSAEventDestroy(void *p){(void)p;return kStatus_Success;}
status_t SDMMC_OSAMutexCreate(void *p){(void)p;return kStatus_Success;}
status_t SDMMC_OSAMutexLock(void *p,uint32_t ms){(void)p;(void)ms;return kStatus_Success;}
status_t SDMMC_OSAMutexUnlock(void *p){(void)p;return kStatus_Success;}
status_t SDMMC_OSAMutexDestroy(void *p){(void)p;return kStatus_Success;}
void SDMMC_OSADelay(uint32_t ms){uint32_t t=diag_ms();while(diag_ms()-t<ms){}}
uint32_t SDMMC_OSADelayUs(uint32_t us){uint32_t t=DWT->CYCCNT, ticks=us*(SystemCoreClock/1000000U);while(DWT->CYCCNT-t<ticks){}return us;}
