#include "diag.h"
#include "fsl_sd.h"
#include "fsl_sdmmc_host.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_clock.h"
#include "fsl_lpuart.h"

#include "diskio.h"
#include <stdio.h>
#include <string.h>
static volatile uint32_t ticks;
static sd_card_t card __attribute__((aligned(64)));
static sdmmchost_t host __attribute__((aligned(64)));
static uint32_t descriptors[128] __attribute__((aligned(64)));
static uint8_t bounce[65536] __attribute__((aligned(64)));

static int initialized,failed;
static uint32_t init_status=~0U;
void SysTick_Handler(void){ticks++;}
uint32_t diag_ms(void){return ticks;}
static bool present(void){return GPIO_PinRead(GPIO3,4)==0;}
static void power(bool on){GPIO_PinWrite(GPIO1,23,on?0:1);}
static void pads(uint32_t hz){(void)hz;IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_00_USDHC1_CMD,0x170F9U);IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_01_USDHC1_CLK,0x10F9U);IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_02_USDHC1_DATA0,0x170F9U);}
void diag_emit(const char *s){LPUART_WriteBlocking(LPUART1,(const uint8_t*)s,strlen(s));}
void diag_status(void){char s[300];snprintf(s,sizeof s,"SD present=%u initialized=%u fault=%u init_status=%lu capacity_bytes=%llu sectors=%lu sector_bytes=%lu bus_bits=%u clock_hz=%lu root_hz=%lu cpu_hz=%lu timing=%u signal_mV=3300\r\n",present(),initialized,failed,(unsigned long)init_status,(unsigned long long)card.blockCount*card.blockSize,(unsigned long)card.blockCount,(unsigned long)card.blockSize,initialized?((USDHC1->PROT_CTRL&USDHC_PROT_CTRL_DTW_MASK)?4:1):1,(unsigned long)card.busClock_Hz,(unsigned long)host.hostController.sourceClock_Hz,(unsigned long)SystemCoreClock,(unsigned)card.currentTiming);diag_emit(s);}
DSTATUS disk_status(BYTE d){if(d!=2)return STA_NOINIT;return !present()?STA_NOINIT|STA_NODISK:(!initialized||failed)?STA_NOINIT:0;}
DSTATUS disk_initialize(BYTE d){if(d!=2||failed||!present())return STA_NOINIT;if(initialized)return 0;init_status=SD_Init(&card);if(init_status!=kStatus_Success){failed=1;return STA_NOINIT;}if(card.blockSize!=512U||card.busClock_Hz>25000000U||(USDHC1->PROT_CTRL&USDHC_PROT_CTRL_DTW_MASK)){failed=1;return STA_NOINIT;}initialized=1;return 0;}
static DRESULT transfer(BYTE d,BYTE *out,const BYTE *in,LBA_t sec,UINT count){if(!count||d!=2)return RES_PARERR;if(disk_status(d))return RES_NOTRDY;if(sec>=card.blockCount||count>card.blockCount-sec)return RES_PARERR;while(count){UINT n=count>128?128:count;status_t r;if(in){memcpy(bounce,in,n*512U);r=SD_WriteBlocks(&card,bounce,sec,n);}else r=SD_ReadBlocks(&card,bounce,sec,n);if(r!=kStatus_Success||!present()){failed=1;return RES_ERROR;}if(out){memcpy(out,bounce,n*512U);out+=n*512U;}if(in)in+=n*512U;sec+=n;count-=n;}return RES_OK;}
DRESULT disk_read(BYTE d,BYTE *p,LBA_t s,UINT n){return transfer(d,p,0,s,n);}
DRESULT disk_write(BYTE d,const BYTE *p,LBA_t s,UINT n){return transfer(d,0,p,s,n);}
DRESULT disk_ioctl(BYTE d,BYTE cmd,void *p){if(disk_status(d))return RES_NOTRDY;switch(cmd){case CTRL_SYNC:if(SD_PollingCardStatusBusy(&card,1000)==kStatus_SDMMC_CardStatusIdle)return RES_OK;failed=1;return RES_ERROR;case GET_SECTOR_COUNT:if(p){*(LBA_t*)p=card.blockCount;return RES_OK;}break;case GET_SECTOR_SIZE:if(p){*(WORD*)p=512;return RES_OK;}break;case GET_BLOCK_SIZE:if(p){*(DWORD*)p=card.csd.eraseSectorSize+1;return RES_OK;}break;default:break;}return RES_PARERR;}
static void setup(void){gpio_pin_config_t in={kGPIO_DigitalInput,0,kGPIO_NoIntmode},out={kGPIO_DigitalOutput,0,kGPIO_NoIntmode};lpuart_config_t uart;static sd_detect_card_t cd;static sd_io_voltage_t voltage;
 clock_sys_pll_config_t pll={.loopDivider=1U,.denominator=1U};
 clock_arm_pll_config_t arm={.loopDivider=100U,.src=0U};
 CLOCK_SetXtalFreq(24000000U);
 CLOCK_SetMux(kCLOCK_PeriphClk2Mux,1);CLOCK_SetDiv(kCLOCK_PeriphClk2Div,0);CLOCK_SetMux(kCLOCK_PeriphMux,1);
 SystemCoreClockUpdate();CLOCK_InitSysPll(&pll);CLOCK_InitSysPfd(kCLOCK_Pfd0,24);
 /* Same rated 600MHz voltage/PLL policy as the repository board clock code.
  * USB PLL and FlexSPI2 clock tree remain untouched for XIP. */
 DCDC->REG3=(DCDC->REG3&~DCDC_REG3_TRG_MASK)|DCDC_REG3_TRG(0x13);
 while(!(DCDC->REG0&DCDC_REG0_STS_DC_OK_MASK)){}
 CLOCK_InitArmPll(&arm);CLOCK_SetDiv(kCLOCK_ArmDiv,1);CLOCK_SetDiv(kCLOCK_AhbDiv,0);
 CLOCK_SetDiv(kCLOCK_IpgDiv,3);CLOCK_SetMux(kCLOCK_PrePeriphMux,3);CLOCK_SetMux(kCLOCK_PeriphMux,0);
 SystemCoreClockUpdate();SCB_EnableICache();
 CLOCK_SetMux(kCLOCK_UartMux,1);CLOCK_SetDiv(kCLOCK_UartDiv,0);SCB_DisableDCache();SCB->VTOR=0x70002000U;CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;SysTick_Config(SystemCoreClock/1000U);
 CLOCK_EnableClock(kCLOCK_Iomuxc);CLOCK_EnableClock(kCLOCK_Gpio1);CLOCK_EnableClock(kCLOCK_Gpio3);
 IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_12_LPUART1_TX,0);IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_13_LPUART1_RX,0);IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B0_12_LPUART1_TX,0x10B0U);IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B0_13_LPUART1_RX,0x10B0U);
 LPUART_GetDefaultConfig(&uart);uart.baudRate_Bps=115200;uart.enableTx=true;uart.enableRx=true;LPUART_Init(LPUART1,&uart,CLOCK_GetClockRootFreq(kCLOCK_UartClkRoot));
 IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B1_07_GPIO1_IO23,0);IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B1_07_GPIO1_IO23,0x10B0U);GPIO_PinInit(GPIO1,23,&out);
 IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B1_04_GPIO3_IO04,0);IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B1_04_GPIO3_IO04,0x1F0B0U);GPIO_PinInit(GPIO3,4,&in);
 IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_00_USDHC1_CMD,0);IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_01_USDHC1_CLK,0);IOMUXC_SetPinMux(IOMUXC_GPIO_SD_B0_02_USDHC1_DATA0,0);pads(0);
 CLOCK_SetDiv(kCLOCK_Usdhc1Div,1);CLOCK_SetMux(kCLOCK_Usdhc1Mux,1);
 host.hostController.base=USDHC1;host.hostController.sourceClock_Hz=CLOCK_GetFreq(kCLOCK_SysPllPfd0Clk)/2U;host.dmaDesBuffer=descriptors;host.dmaDesBufferWordsNum=128;host.enableCacheControl=kSDMMCHOST_CacheControlRWBuffer;
 cd.type=kSD_DetectCardByGpioCD;cd.cardDetected=present;cd.cdDebounce_ms=100;voltage.type=kSD_IOVoltageCtrlNotSupport;
 card.host=&host;card.usrParam.cd=&cd;card.usrParam.pwr=power;card.usrParam.ioStrength=pads;card.usrParam.ioVoltage=&voltage;card.usrParam.maxFreq=25000000;NVIC_SetPriority(USDHC1_IRQn,5);
}
int main(void){char line[80];unsigned n=0;int overflow=0;setup();diag_emit("SDPERF v2 EXPORT1 CPU600MHz; no automatic SD test; 115200 8N1 UART1 B12/B13\r\n");for(;;){if(LPUART_GetStatusFlags(LPUART1)&kLPUART_RxDataRegFullFlag){char c=(char)LPUART_ReadByte(LPUART1);if(c=='\r'||c=='\n'){if(overflow)diag_emit("ERROR line too long\r\n");else if(n){line[n]=0;diag_command(line);}n=0;overflow=0;}else if(n<sizeof(line)-1&&!overflow)line[n++]=c;else overflow=1;}}}

/* Only called between fully closed files. The card is already negotiated into
 * SD High Speed by SD_Init; raising host clock cannot replace negotiation. */
int diag_set_clock(uint32_t hz)
{
 if(!initialized||failed||!present()||(hz!=25000000U&&hz!=50000000U))return 0;
 if(card.operationVoltage!=kSDMMC_OperationVoltage330V ||
    (USDHC1->PROT_CTRL&USDHC_PROT_CTRL_DTW_MASK))return 0;
 if(hz>25000000U && card.currentTiming!=kSD_TimingSDR25HighSpeedMode)return 0;
 if(SD_PollingCardStatusBusy(&card,1000U)!=kStatus_SDMMC_CardStatusIdle)return 0;
 card.busClock_Hz=SDMMCHOST_SetCardClock(&host,hz);
 if(!card.busClock_Hz||card.busClock_Hz>hz||(hz>25000000U&&card.busClock_Hz<=25000000U)){failed=1;return 0;}
 diag_status();return 1;
}
uint32_t diag_us(void)
{
 uint32_t a,b,v;
 do{a=ticks;v=SysTick->VAL;b=ticks;}while(a!=b);
 return a*1000U+(SysTick->LOAD-v)/(SystemCoreClock/1000000U);
}

void diag_send_bytes(const uint8_t *p,unsigned n)
{LPUART_WriteBlocking(LPUART1,p,n);}
int diag_uart_baud(uint32_t baud)
{
 uint32_t t=diag_ms();
 if(baud!=115200U&&baud!=460800U&&baud!=921600U)return 0;
 while(!(LPUART_GetStatusFlags(LPUART1)&kLPUART_TransmissionCompleteFlag))
   if(diag_ms()-t>100U)return 0;
 t=diag_ms();while(diag_ms()-t<20U){}
 while(LPUART_GetStatusFlags(LPUART1)&kLPUART_RxDataRegFullFlag)(void)LPUART_ReadByte(LPUART1);
 return LPUART_SetBaudRate(LPUART1,baud,CLOCK_GetClockRootFreq(kCLOCK_UartClkRoot))==kStatus_Success;
}
