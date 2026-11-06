#include "zf_common_headfile.h"
#include "fsl_sd_disk.h"
#include "sdmmc_config.h"
#include "recorder_sd.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
AT_SDRAM_NONCACHE_SECTION_ALIGN(sd_card_t g_sd,64);
/* USDHC never borrows a capture, vision or recorder-queue address. On any
 * media error this entire buffer stays quarantined until power cycle. */
AT_SDRAM_NONCACHE_SECTION_ALIGN(static uint8_t bounce[32768],64);
static int initialized;
static uint32_t init_status=~0U;
extern bool BOARD_SDCardGetDetectStatus(void);
void recorder_sd_board_init(void)
{
    recorder_sd_pins_init();BOARD_SD_Config(&g_sd,NULL,5U,NULL);
    g_sd.usrParam.maxFreq=RECORDER_SD_MAX_HZ;
}
static uint8_t pin_mux(uint32_t address,uint32_t mode,uint32_t input,uint32_t daisy,uint32_t pad)
{
    (void)mode;(void)input;(void)daisy;(void)pad;
    return (uint8_t)(*(volatile uint32_t *)(uintptr_t)address&15U);
}
void recorder_sd_status(rec_sd_status_t *out)
{
    taskENTER_CRITICAL();
    out->present=(uint8_t)BOARD_SDCardGetDetectStatus();out->initialized=(uint8_t)initialized;
    out->quarantined=(uint8_t)recorder_sd_failed();out->clock_hz=g_sd.busClock_Hz;
    out->host_capability=g_sd.host?g_sd.host->capability:0;out->init_status=init_status;
    out->bus_bits=RECORDER_SD_BUS_BITS;
    if(initialized) {
        uint32_t width=(USDHC1->PROT_CTRL&USDHC_PROT_CTRL_DTW_MASK)>>USDHC_PROT_CTRL_DTW_SHIFT;
        out->bus_bits=(uint8_t)(width==0?1U:width==1?4U:8U);
    }
    out->mux_radio_tx=pin_mux(IOMUXC_GPIO_SD_B0_04_LPUART8_TX);
    out->mux_radio_rx=pin_mux(IOMUXC_GPIO_SD_B0_05_LPUART8_RX);
    out->mux_servo=pin_mux(IOMUXC_GPIO_B1_14_FLEXPWM4_PWMA02);
    taskEXIT_CRITICAL();
}
DSTATUS sd_disk_status(BYTE drv)
{
    if(drv!=SDDISK || !initialized || recorder_sd_failed())return STA_NOINIT;
    if(!BOARD_SDCardGetDetectStatus())return STA_NOINIT|STA_NODISK;
    return 0;
}
DSTATUS sd_disk_initialize(BYTE drv)
{
    if(drv!=SDDISK || recorder_sd_failed() || !BOARD_SDCardGetDetectStatus())return STA_NOINIT;
    if(initialized)return 0;
    init_status=(uint32_t)SD_Init(&g_sd);
    if(init_status!=(uint32_t)kStatus_Success || recorder_sd_expired()) {
        recorder_sd_fault();return STA_NOINIT;
    }
    initialized=1;return 0;
}
static DRESULT transfer(BYTE drv,BYTE *out,const BYTE *in,LBA_t sector,UINT count)
{
    UINT n;status_t status;
    if(drv!=SDDISK || !count)return RES_PARERR;
    if(sd_disk_status(drv))return RES_NOTRDY;
    while(count) {
        n=count>64U?64U:count;
        if(recorder_sd_expired())goto fail;
        if(in){memcpy(bounce,in,n*512U);status=SD_WriteBlocks(&g_sd,bounce,sector,n);}
        else status=SD_ReadBlocks(&g_sd,bounce,sector,n);
        if(status!=kStatus_Success || recorder_sd_expired() || !BOARD_SDCardGetDetectStatus())goto fail;
        if(out){memcpy(out,bounce,n*512U);out+=n*512U;}
        if(in)in+=n*512U;
        sector+=n;count-=n;
    }
    return RES_OK;
fail: recorder_sd_fault();return RES_ERROR;
}
DRESULT sd_disk_write(BYTE drv,const BYTE *in,LBA_t sector,UINT count)
{return transfer(drv,NULL,in,sector,count);}
DRESULT sd_disk_read(BYTE drv,BYTE *out,LBA_t sector,UINT count)
{return transfer(drv,out,NULL,sector,count);}
DRESULT sd_disk_ioctl(BYTE drv,BYTE cmd,void *out)
{
    if(sd_disk_status(drv))return RES_NOTRDY;
    switch(cmd) {
    case CTRL_SYNC:
        if(SD_PollingCardStatusBusy(&g_sd,250U)==kStatus_SDMMC_CardStatusIdle && !recorder_sd_expired())return RES_OK;
        recorder_sd_fault();return RES_ERROR;
    case GET_SECTOR_COUNT: if(out){*(LBA_t *)out=g_sd.blockCount;return RES_OK;}break;
    case GET_SECTOR_SIZE: if(out){*(WORD *)out=(WORD)g_sd.blockSize;return RES_OK;}break;
    case GET_BLOCK_SIZE: if(out){*(DWORD *)out=g_sd.csd.eraseSectorSize+1U;return RES_OK;}break;
    default: break;
    }
    return RES_PARERR;
}
