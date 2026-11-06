/* Sole caller is Monitor in export-only mode or after Writer unmounted.
 * Every filesystem operation is read-only. Existing quarantine/deadlines apply.
 */
#ifdef B2_EXPORT_HOST_TEST
#include "b2_export_mock.h"
#else
#include "main.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "fsl_sd_disk.h"
#include "b2_export.h"
#include "task.h"
#endif
#include "b2_export.h"
#include <stdio.h>
#include <string.h>
static FATFS export_fs;
static uint8_t bytes[4096];
static int mounted;
static void error(const char *why,FRESULT fr){printf("B2READ ERROR code=%s fr=%u\r\n",why,(unsigned)fr);}
static int end_io(FRESULT r)
{
 if(recorder_sd_expired())r=FR_DISK_ERR;
 recorder_sd_operation_end();
 if(r==FR_DISK_ERR)recorder_sd_fault();
 if(r!=FR_OK){error("IO",r);return 0;}return 1;
}
static int mount_read(void)
{
 FRESULT r;if(recorder_sd_failed()){error("QUARANTINED",FR_DISK_ERR);return 0;}
 if(mounted)return 1;
 recorder_sd_operation_begin(5000);r=f_mount(&export_fs,"2:",1);
 if(!end_io(r))return 0;mounted=1;return 1;
}
static void list(const char *dir)
{
 DIR d;FILINFO f;FRESULT r;char path[24];unsigned count=0;int okay=1;
 if(!b2_export_dir(dir)){error("DIR",FR_INVALID_NAME);return;}
 if(!mount_read())return;
 snprintf(path,sizeof path,"2:/%s",dir);
 recorder_sd_operation_begin(3000);r=f_opendir(&d,path);if(!end_io(r))return;
 printf("B2READ LIST dir=%s\r\n",dir);
 for(;;){
  recorder_sd_operation_begin(3000);r=f_readdir(&d,&f);if(!end_io(r)){okay=0;break;}
  if(!f.fname[0])break;
  if((f.fattrib&AM_DIR)||!b2_export_name(f.fname)||++count>257){error("UNEXPECTED_ENTRY",FR_INVALID_NAME);okay=0;break;}
  printf("B2READ FILE name=%s size=%lu\r\n",f.fname,(unsigned long)f.fsize);
 }
 if(!recorder_sd_failed()){recorder_sd_operation_begin(3000);r=f_closedir(&d);if(!end_io(r))okay=0;}else okay=0;
 if(okay)printf("B2READ LISTEND count=%u\r\n",count);
}
static void read_file(const char *dir,const char *name,unsigned long off,unsigned long n)
{
 FIL f;FRESULT r;UINT got=0;char path[48];unsigned long size;int okay;
 if(!b2_export_dir(dir)||!b2_export_name(name)){error("PATH",FR_INVALID_NAME);return;}
 if(!n||n>sizeof bytes||off>64UL*1024UL*1024UL){error("RANGE",FR_INVALID_PARAMETER);return;}
 if(!mount_read())return;
 snprintf(path,sizeof path,"2:/%s/%s",dir,name);
 recorder_sd_operation_begin(3000);r=f_open(&f,path,FA_READ);if(!end_io(r))return;
 size=(unsigned long)f_size(&f);okay=off<size&&n<=size-off&&size<=64UL*1024UL*1024UL;
 if(!okay)error("SIZE_RANGE",FR_INVALID_PARAMETER);
 if(okay){recorder_sd_operation_begin(3000);r=f_lseek(&f,(FSIZE_t)off);okay=end_io(r);}
 if(okay){recorder_sd_operation_begin(3000);r=f_read(&f,bytes,(UINT)n,&got);okay=end_io(r);if(okay&&got!=n){error("SHORT_READ",FR_DISK_ERR);okay=0;}}
 if(!recorder_sd_failed()){recorder_sd_operation_begin(3000);r=f_close(&f);if(!end_io(r))okay=0;}else okay=0;
 if(!okay)return;
 printf("B2READ DATA dir=%s name=%s offset=%lu length=%lu crc32=%08lX\r\n",dir,name,off,n,(unsigned long)rec_crc_update(0,bytes,n));
 LPUART_WriteBlocking(LPUART1,bytes,n);printf("\r\nB2READ END\r\n");
}
int b2_export_command(const char *line)
{
 char dir[12],name[16],extra;unsigned long off,n,baud;uint32_t start;
 if(strncmp(line,"B2 EXPORT ",10))return 0;
 line+=10;
 if(sscanf(line,"LIST %11s %c",dir,&extra)==1){list(dir);return 1;}
 if(sscanf(line,"READ %11s %15s %lu %lu %c",dir,name,&off,&n,&extra)==4){read_file(dir,name,off,n);return 1;}
 if(sscanf(line,"BAUD %lu %c",&baud,&extra)==1){
  if(baud!=115200&&baud!=921600){error("BAUD",FR_INVALID_PARAMETER);return 1;}
  printf("B2READ BAUD next=%lu\r\n",baud);start=timer_get(GPT_TIM_1);
  while(!(LPUART_GetStatusFlags(LPUART1)&kLPUART_TransmissionCompleteFlag))if(timer_get(GPT_TIM_1)-start>100)return 1;
  vTaskDelay(pdMS_TO_TICKS(30));
  if(LPUART_SetBaudRate(LPUART1,baud,CLOCK_GetClockRootFreq(kCLOCK_UartClkRoot))!=kStatus_Success)error("BAUD_SWITCH",FR_INVALID_PARAMETER);
  return 1;
 }
 error("SYNTAX",FR_INVALID_PARAMETER);return 1;
}
