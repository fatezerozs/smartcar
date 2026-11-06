/* Training-only SDMMC OSA. Other SDK components retain their existing OSA. */
#ifndef _FSL_SDMMC_OSA_H_
#define _FSL_SDMMC_OSA_H_
#include "fsl_common.h"
#include "fsl_os_abstraction.h"
#include "FreeRTOS.h"
#include "semphr.h"
#define SDMMC_OSA_EVENT_TRANSFER_CMD_SUCCESS (1UL<<0)
#define SDMMC_OSA_EVENT_TRANSFER_CMD_FAIL (1UL<<1)
#define SDMMC_OSA_EVENT_TRANSFER_DATA_SUCCESS (1UL<<2)
#define SDMMC_OSA_EVENT_TRANSFER_DATA_FAIL (1UL<<3)
#define SDMMC_OSA_EVENT_TRANSFER_DMA_COMPLETE (1UL<<4)
#define SDMMC_OSA_EVENT_CARD_INSERTED (1UL<<8)
#define SDMMC_OSA_EVENT_CARD_REMOVED (1UL<<9)
typedef struct { volatile uint32_t flags; StaticSemaphore_t storage; SemaphoreHandle_t sem; } sdmmc_osa_event_t;
typedef struct { StaticSemaphore_t storage; SemaphoreHandle_t sem; } sdmmc_osa_mutex_t;
void SDMMC_OSAInit(void);
status_t SDMMC_OSAEventCreate(void *p);
status_t SDMMC_OSAEventWait(void *p,uint32_t mask,uint32_t ms,uint32_t *out);
status_t SDMMC_OSAEventSet(void *p,uint32_t mask);
status_t SDMMC_OSAEventGet(void *p,uint32_t mask,uint32_t *out);
status_t SDMMC_OSAEventClear(void *p,uint32_t mask);
status_t SDMMC_OSAEventDestroy(void *p);
status_t SDMMC_OSAMutexCreate(void *p);
status_t SDMMC_OSAMutexLock(void *p,uint32_t ms);
status_t SDMMC_OSAMutexUnlock(void *p);
status_t SDMMC_OSAMutexDestroy(void *p);
void SDMMC_OSADelay(uint32_t ms);
uint32_t SDMMC_OSADelayUs(uint32_t us);
#endif
