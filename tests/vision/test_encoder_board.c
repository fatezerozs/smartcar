#include "main.h"
#include "fsl_qtmr.h"
#include "board_encoder.h"
#include <assert.h>
#include <stdio.h>
test_qtmr_t test_tmr2;
test_gpr_t test_gpr;
test_iomux_t test_iomux;
static unsigned initialized,cleared,read_count;
void __DSB(void){}
void encoder_quad_init(int n,int a,int b)
{
    if(!initialized)assert(n==QTIMER2_ENCODER1&&a==QTIMER2_ENCODER1_CH1_C3&&b==QTIMER2_ENCODER1_CH2_C4);
    else assert(n==QTIMER2_ENCODER2&&a==QTIMER2_ENCODER2_CH1_C5&&b==QTIMER2_ENCODER2_CH2_C25);
    /* Model the documented vendor default; board layer must not override CM. */
    TMR2->CHANNEL[initialized?2:0].CTRL=5U<<13;
    IOMUXC->SELECT_INPUT[kIOMUXC_QTIMER2_TIMER0_SELECT_INPUT+initialized*2]=1;
    IOMUXC->SELECT_INPUT[kIOMUXC_QTIMER2_TIMER0_SELECT_INPUT+initialized*2+1]=1;
    ++initialized;
}
void encoder_clear_count(int n){assert(n==(cleared?QTIMER2_ENCODER2:QTIMER2_ENCODER1));++cleared;}
int16_t encoder_get_count(int n){++read_count;assert(n==QTIMER2_ENCODER1||n==QTIMER2_ENCODER2);return n==QTIMER2_ENCODER1?-12:34;}
int main(void)
{
    uint16_t left,right;board_encoder_diag_t d;
    IOMUXC_GPR->GPR6=0xA5A5A5FFU;board_encoder_init();assert(initialized==2&&cleared==2);
    assert(IOMUXC_GPR->GPR6==0xA5A5A50FU);
    board_encoder_read(&left,&right);assert(left==(uint16_t)-12&&right==34&&read_count==2&&cleared==2);
    TMR2->CHANNEL[0].SCTRL=TMR_SCTRL_INPUT_MASK;TMR2->CHANNEL[3].SCTRL=TMR_SCTRL_INPUT_MASK;
    board_encoder_capture(&d);assert(d.inputs==9&&d.ctrl3==(5U<<13)&&d.ctrl4==(5U<<13)&&d.daisy[3]==1);
    assert(cleared==2&&IOMUXC_GPR->GPR6==0xA5A5A50FU);
    puts("ENCODER BOARD PASS: encoder3 LEFT, encoder4 RIGHT, vendor mode, QTMR2 pin input ownership, read-only diagnostics.");return 0;
}
