/* No SDRAM vector relocation or external-memory startup. */
#include "fsl_device_registers.h"
#include "fsl_clock.h"
uint32_t SystemCoreClock=24000000U;
void SystemCoreClockUpdate(void){SystemCoreClock=CLOCK_GetFreq(kCLOCK_CpuClk);}
void SystemInit(void){
 SCB->CPACR|=(15UL<<20);__DSB();__ISB();
 WDOG1->WMCR&=~WDOG_WMCR_PDE_MASK;WDOG2->WMCR&=~WDOG_WMCR_PDE_MASK;
 if(WDOG1->WCR&WDOG_WCR_WDE_MASK)WDOG1->WCR&=~WDOG_WCR_WDE_MASK;
 if(WDOG2->WCR&WDOG_WCR_WDE_MASK)WDOG2->WCR&=~WDOG_WCR_WDE_MASK;
 if(RTWDOG->CS&RTWDOG_CS_CMD32EN_MASK)RTWDOG->CNT=0xD928C520U;
 else{RTWDOG->CNT=0xC520U;RTWDOG->CNT=0xD928U;}
 RTWDOG->TOVAL=0xFFFFU;RTWDOG->CS=(RTWDOG->CS&~RTWDOG_CS_EN_MASK)|RTWDOG_CS_UPDATE_MASK;
 SysTick->CTRL=0;SCB->VTOR=0x70002000U;
}
void __aeabi_assert(const char *expr,const char *file,int line){(void)expr;(void)file;(void)line;__disable_irq();for(;;){__NOP();}}
