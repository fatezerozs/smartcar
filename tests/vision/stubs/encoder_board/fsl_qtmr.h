#ifndef ENCODER_QTMR_STUB_H
#define ENCODER_QTMR_STUB_H
#include <stdint.h>
typedef struct {uint16_t CTRL,SCTRL;} test_qtmr_channel_t;
typedef struct {test_qtmr_channel_t CHANNEL[4];} test_qtmr_t;
typedef struct {uint32_t GPR6;} test_gpr_t;
typedef struct {uint32_t SELECT_INPUT[154];} test_iomux_t;
extern test_qtmr_t test_tmr2;
extern test_gpr_t test_gpr;
extern test_iomux_t test_iomux;
#define TMR2 (&test_tmr2)
#define IOMUXC_GPR (&test_gpr)
#define IOMUXC (&test_iomux)
#define IOMUXC_GPR_GPR6_QTIMER2_TRM0_INPUT_SEL_MASK 0x10U
#define IOMUXC_GPR_GPR6_QTIMER2_TRM1_INPUT_SEL_MASK 0x20U
#define IOMUXC_GPR_GPR6_QTIMER2_TRM2_INPUT_SEL_MASK 0x40U
#define IOMUXC_GPR_GPR6_QTIMER2_TRM3_INPUT_SEL_MASK 0x80U
#define TMR_SCTRL_INPUT_MASK 0x100U
#define kIOMUXC_QTIMER2_TIMER0_SELECT_INPUT 94U
#define kQTMR_Channel_0 0
#define kQTMR_Channel_2 2
#define kQTMR_QuadCountMode 3
void __DSB(void);
#endif
