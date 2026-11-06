/* Private instrumentation config; the original Recorder config is unchanged. */
#ifndef B2_FREERTOS_CONFIG_H
#define B2_FREERTOS_CONFIG_H
#include "../../recorder/include/FreeRTOSConfig.h"
#define configUSE_TRACE_FACILITY 1
#define configGENERATE_RUN_TIME_STATS 1
void b2_runtime_init(void);
uint32_t b2_runtime_us(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() b2_runtime_init()
#define portGET_RUN_TIME_COUNTER_VALUE() b2_runtime_us()
#endif
