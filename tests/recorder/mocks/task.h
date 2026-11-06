#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define taskENTER_CRITICAL_FROM_ISR() (0U)
#define taskEXIT_CRITICAL_FROM_ISR(n) ((void)(n))
#define portYIELD_FROM_ISR(n) ((void)(n))
