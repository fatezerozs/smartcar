#include "main.h"
#include "fsl_qtmr.h"
#include "board_encoder.h"
void board_encoder_init(void)
{
    /* Claim only QTMR2's four external inputs. Boot/XBAR state must not
     * silently replace these physical encoder pins. */
    IOMUXC_GPR->GPR6 &= ~(IOMUXC_GPR_GPR6_QTIMER2_TRM0_INPUT_SEL_MASK|
        IOMUXC_GPR_GPR6_QTIMER2_TRM1_INPUT_SEL_MASK|
        IOMUXC_GPR_GPR6_QTIMER2_TRM2_INPUT_SEL_MASK|
        IOMUXC_GPR_GPR6_QTIMER2_TRM3_INPUT_SEL_MASK);
    __DSB();
    encoder_quad_init(ENCODER_3,ENCODER_3_A,ENCODER_3_B);
    encoder_quad_init(ENCODER_4,ENCODER_4_A,ENCODER_4_B);
    /* Keep the vendor A-rising/B-direction mode as a measured baseline.
     * Do not infer output wiring or force 4x decoding from a "1024" label. */
    encoder_clear_count(ENCODER_3);encoder_clear_count(ENCODER_4);
}
void board_encoder_capture(board_encoder_diag_t *out)
{
    out->ctrl3=TMR2->CHANNEL[0].CTRL;out->ctrl4=TMR2->CHANNEL[2].CTRL;
    out->gpr6=IOMUXC_GPR->GPR6;out->inputs=0;
    for(unsigned i=0;i<4;++i) {
        if(TMR2->CHANNEL[i].SCTRL&TMR_SCTRL_INPUT_MASK)out->inputs|=(uint8_t)(1U<<i);
        out->daisy[i]=(uint8_t)IOMUXC->SELECT_INPUT[kIOMUXC_QTIMER2_TIMER0_SELECT_INPUT+i];
    }
}
void board_encoder_read(uint16_t *left,uint16_t *right)
{
    *left=(uint16_t)encoder_get_count(ENCODER_3);
    *right=(uint16_t)encoder_get_count(ENCODER_4);
    /* Never clear during sampling: modular subtraction avoids read/clear
     * edge loss; valid while movement is below 32768 counts per sample. */
}
