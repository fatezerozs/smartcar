#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
typedef unsigned UINT;
typedef unsigned long FSIZE_t;
typedef int FRESULT;
typedef struct {int unused;} FATFS;
typedef struct {int unused;} FIL;
typedef struct {int at;} DIR;
typedef struct {char fname[32];unsigned fattrib;unsigned long fsize;} FILINFO;
enum {FR_OK=0,FR_DISK_ERR=1,FR_INVALID_NAME=6,FR_INVALID_PARAMETER=19,FA_READ=1,AM_DIR=16,kLPUART_TransmissionCompleteFlag=1,kCLOCK_UartClkRoot=1,kStatus_Success=0,GPT_TIM_1=1};
#define LPUART1 ((void*)0)
#define pdMS_TO_TICKS(n) (n)
int mock_printf(const char *,...);
#define printf mock_printf
void recorder_sd_operation_begin(uint32_t);
void recorder_sd_operation_end(void);
int recorder_sd_expired(void);
int recorder_sd_failed(void);
void recorder_sd_fault(void);
FRESULT f_mount(FATFS*,const char*,int);
FRESULT f_opendir(DIR*,const char*);
FRESULT f_readdir(DIR*,FILINFO*);
FRESULT f_closedir(DIR*);
FRESULT f_open(FIL*,const char*,int);
unsigned long f_size(FIL*);
FRESULT f_lseek(FIL*,FSIZE_t);
FRESULT f_read(FIL*,void*,UINT,UINT*);
FRESULT f_close(FIL*);
uint32_t rec_crc_update(uint32_t,const void*,size_t);
void LPUART_WriteBlocking(void*,const uint8_t*,size_t);
uint32_t LPUART_GetStatusFlags(void*);
int LPUART_SetBaudRate(void*,uint32_t,uint32_t);
uint32_t CLOCK_GetClockRootFreq(int);
uint32_t timer_get(int);
void vTaskDelay(uint32_t);
