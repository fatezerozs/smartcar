#include "zf_common_headfile.h"
#include "recorder_sd.h"
#include <assert.h>
#include <stdio.h>
static unsigned gpio_calls,af_calls,pad_calls,unused_data_pullup;
static uint32_t muxes[3],pads[3];
void gpio_iomuxc(int pin,uint32_t value)
{assert((pin==B23 || pin==D4) && value);++gpio_calls;}
void gpio_init(int pin,int direction,int level,int mode)
{assert(pin==D15 && direction==GPI && level==GPIO_HIGH && mode==GPI_PULL_UP);++unused_data_pullup;}
void afio_init(uint32_t reg,uint32_t mux,uint32_t input,uint32_t daisy,uint32_t pad,uint32_t force,uint32_t value)
{
    assert(reg>=12 && reg<=14 && mux==0 && input==0 && daisy==0 && pad==reg+100 && force==0 && value);
    muxes[reg-12]=mux;++af_calls;
}
void IOMUXC_SetPinConfig(uint32_t reg,uint32_t mux,uint32_t input,uint32_t daisy,uint32_t pad,uint32_t value)
{
    assert(reg>=12 && reg<=14 && mux==0 && input==0 && daisy==0 && pad==reg+100);
    pads[reg-12]=value;++pad_calls;
}
int main(void)
{
    assert(RECORDER_SD_BUS_BITS==1 && RECORDER_SD_MAX_HZ==25000000U);
    recorder_sd_pins_init();assert(gpio_calls==2 && af_calls==3 && unused_data_pullup==1);
    for(unsigned i=0;i<3;++i)assert(muxes[i]==0);
    recorder_sd_pad_config(400000U);recorder_sd_pad_config(25000000U);
    assert(pad_calls==6);
    assert(pads[0]&IOMUXC_SW_PAD_CTL_PAD_PKE_MASK);
    assert(!(pads[1]&IOMUXC_SW_PAD_CTL_PAD_PKE_MASK));
    assert(pads[2]&IOMUXC_SW_PAD_CTL_PAD_PKE_MASK);
    puts("RECORDER SD PINS PASS: CMD/CLK/DAT0 only, DAT1 input pull-up, no UART8 DATA2/3 or servo VSELECT writes.");
    return 0;
}
