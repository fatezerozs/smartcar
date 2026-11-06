/* Retain the original board UART/PIT dispatch and its license verbatim.
 * Only the dedicated target replaces CSI and gives SysTick to FreeRTOS. */
#define CSI_IRQHandler recorder_unused_CSI_IRQHandler
#define SysTick_Handler recorder_unused_SysTick_Handler
#include "../user/src/isr.c"
#undef CSI_IRQHandler
#undef SysTick_Handler
#include "recorder_runtime.h"
void CSI_IRQHandler(void){recorder_capture_irq();}
