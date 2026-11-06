/* Dedicated training exception dispatch; retain vendor definitions/license. */
#define SVC_Handler recorder_unused_SVC_Handler
#define PendSV_Handler recorder_unused_PendSV_Handler
#define HardFault_Handler recorder_unused_HardFault_Handler
#define MemManage_Handler recorder_unused_MemManage_Handler
#define BusFault_Handler recorder_unused_BusFault_Handler
#define UsageFault_Handler recorder_unused_UsageFault_Handler
#include "../../libraries/zf_common/zf_common_vector.c"
#undef SVC_Handler
#undef PendSV_Handler
#undef HardFault_Handler
#undef MemManage_Handler
#undef BusFault_Handler
#undef UsageFault_Handler
extern void recorder_panic(void);
void HardFault_Handler(void){recorder_panic();}
void MemManage_Handler(void){recorder_panic();}
void BusFault_Handler(void){recorder_panic();}
void UsageFault_Handler(void){recorder_panic();}
