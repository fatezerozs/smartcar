#include "zf_common_headfile.h"
#include "recorder_sd.h"
/* Native USDHC1, 1-bit, 3.3V. D16/D17 remain UART8; C30 remains servo PWM.
 * The generic SDK initializer also muxes DATA2/3 and VSELECT: never call it. */
#define REC_SD_PAD (SPEED_200MHZ | PULL_EN | PULLUP_47K | DSE_R0 | SRE_FAST)
void recorder_sd_pins_init(void)
{
    gpio_iomuxc(B23,SPEED_200MHZ | KEEPER_EN | PULLDOWN_100K | DSE_R0_6);
    gpio_iomuxc(D4,REC_SD_PAD);
    afio_init(IOMUXC_GPIO_SD_B0_00_USDHC1_CMD,0,REC_SD_PAD);
    afio_init(IOMUXC_GPIO_SD_B0_01_USDHC1_CLK,0,REC_SD_PAD);
    afio_init(IOMUXC_GPIO_SD_B0_02_USDHC1_DATA0,0,REC_SD_PAD);
    gpio_init(D15,GPI,GPIO_HIGH,GPI_PULL_UP); /* Unused card DATA1. */
}
void recorder_sd_pad_config(uint32_t frequency)
{
    uint32_t data=IOMUXC_SW_PAD_CTL_PAD_SPEED(0) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
        IOMUXC_SW_PAD_CTL_PAD_PKE_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(1) |
        IOMUXC_SW_PAD_CTL_PAD_DSE(7);
    uint32_t clock=IOMUXC_SW_PAD_CTL_PAD_SPEED(0) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
        IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_DSE(7);
    (void)frequency; /* The private host/card policy caps all modes at 25MHz. */
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_00_USDHC1_CMD,data);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_01_USDHC1_CLK,clock);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_02_USDHC1_DATA0,data);
}
