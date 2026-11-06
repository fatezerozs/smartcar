#ifndef MOCK_FSL_COMMON_H
#define MOCK_FSL_COMMON_H
#include <stdint.h>
typedef int status_t;
enum {kStatus_Success,kStatus_Fail};
typedef struct {uint32_t INT_SIGNAL_EN;} mock_usdhc_t;
extern mock_usdhc_t mock_usdhc;
#define USDHC1 (&mock_usdhc)
#define USDHC1_IRQn 22
extern unsigned mock_ipsr,mock_irq_disabled;
#define __get_IPSR() mock_ipsr
#define __DSB() ((void)0)
#define NVIC_DisableIRQ(n) ((void)(n),mock_irq_disabled=1)
#endif
