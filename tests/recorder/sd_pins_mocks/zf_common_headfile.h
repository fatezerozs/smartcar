#ifndef RECORDER_SD_PINS_TEST_H
#define RECORDER_SD_PINS_TEST_H
#include <stdint.h>
#define B23 23
#define D4 104
#define D15 115
#define GPI 0
#define GPIO_HIGH 1
#define GPI_PULL_UP 2
#define SPEED_200MHZ 1U
#define PULL_EN 2U
#define PULLUP_47K 4U
#define DSE_R0 8U
#define SRE_FAST 16U
#define KEEPER_EN 32U
#define PULLDOWN_100K 64U
#define DSE_R0_6 128U
#define IOMUXC_GPIO_SD_B0_00_USDHC1_CMD 12U,0U,0U,0U,112U
#define IOMUXC_GPIO_SD_B0_01_USDHC1_CLK 13U,0U,0U,0U,113U
#define IOMUXC_GPIO_SD_B0_02_USDHC1_DATA0 14U,0U,0U,0U,114U
#define IOMUXC_SW_PAD_CTL_PAD_SPEED(n) ((uint32_t)(n)<<0)
#define IOMUXC_SW_PAD_CTL_PAD_SRE_MASK (1U<<2)
#define IOMUXC_SW_PAD_CTL_PAD_PKE_MASK (1U<<3)
#define IOMUXC_SW_PAD_CTL_PAD_PUE_MASK (1U<<4)
#define IOMUXC_SW_PAD_CTL_PAD_HYS_MASK (1U<<5)
#define IOMUXC_SW_PAD_CTL_PAD_PUS(n) ((uint32_t)(n)<<6)
#define IOMUXC_SW_PAD_CTL_PAD_DSE(n) ((uint32_t)(n)<<8)
void gpio_iomuxc(int pin,uint32_t value);
void gpio_init(int pin,int direction,int level,int mode);
void afio_init(uint32_t reg,uint32_t mux,uint32_t input,uint32_t daisy,uint32_t pad,uint32_t force,uint32_t value);
void IOMUXC_SetPinConfig(uint32_t reg,uint32_t mux,uint32_t input,uint32_t daisy,uint32_t pad,uint32_t value);
#endif
