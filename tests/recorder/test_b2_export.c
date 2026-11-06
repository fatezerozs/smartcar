#include "b2_export_mock.h"
#include "b2_export.h"
#include <assert.h>
#include <string.h>
#include <stdarg.h>
static char logtext[8192];static unsigned used,opened,binary,active,faulted;static const char *mode;
int mock_printf(const char *fmt,...){int n;va_list a;va_start(a,fmt);n=vsnprintf(logtext+used,sizeof logtext-used,fmt,a);va_end(a);assert(n>=0&&(unsigned)n<sizeof logtext-used);used+=(unsigned)n;return n;}
void recorder_sd_operation_begin(uint32_t ms){assert(ms&& !active);active=1;}
void recorder_sd_operation_end(void){assert(active);active=0;}
int recorder_sd_expired(void){return !strcmp(mode,"timeout");}
int recorder_sd_failed(void){return faulted||!strcmp(mode,"quarantine");}
void recorder_sd_fault(void){faulted=1;}
FRESULT f_mount(FATFS *f,const char *v,int now){assert(f&&now==1&&!strcmp(v,"2:"));return !strcmp(mode,"mountfail")?FR_DISK_ERR:FR_OK;}
FRESULT f_opendir(DIR *d,const char *p){assert(!strcmp(p,"2:/R0000001"));d->at=0;return FR_OK;}
FRESULT f_readdir(DIR *d,FILINFO *f){memset(f,0,sizeof *f);if(d->at==0)strcpy(f->fname,!strcmp(mode,"unexpected")?"evil.bin":"session.json");else if(d->at==1)strcpy(f->fname,"000000.scraw");f->fsize=3;d->at++;return FR_OK;}
FRESULT f_closedir(DIR *d){(void)d;return FR_OK;}
FRESULT f_open(FIL *f,const char *p,int flags){(void)f;assert(!strcmp(p,"2:/R0000001/000000.scraw")&&flags==FA_READ);opened++;return FR_OK;}
unsigned long f_size(FIL *f){(void)f;return 3;}
FRESULT f_lseek(FIL *f,FSIZE_t n){(void)f;assert(n==0);return FR_OK;}
FRESULT f_read(FIL *f,void *b,UINT n,UINT *got){(void)f;assert(n==3);memcpy(b,"abc",3);*got=!strcmp(mode,"short")?2:3;return FR_OK;}
FRESULT f_close(FIL *f){(void)f;return !strcmp(mode,"closefail")?FR_DISK_ERR:FR_OK;}
void LPUART_WriteBlocking(void *p,const uint8_t *b,size_t n){(void)p;assert(n==3&&!memcmp(b,"abc",3));binary++;}
uint32_t LPUART_GetStatusFlags(void *p){(void)p;return 1;}
int LPUART_SetBaudRate(void *p,uint32_t b,uint32_t hz){(void)p;assert(hz&&b==921600);return 0;}
uint32_t CLOCK_GetClockRootFreq(int n){(void)n;return 80000000;}
uint32_t timer_get(int n){(void)n;return 0;}
void vTaskDelay(uint32_t n){assert(n==30);}
int main(int argc,char **argv){assert(argc==2);mode=argv[1];
 assert(b2_export_dir("R0000001")&&!b2_export_dir("")&&!b2_export_dir("R")&&!b2_export_dir("R0000001/.."));
 assert(b2_export_name("000000.scraw")&&b2_export_name("session.json")&&!b2_export_name("")&&!b2_export_name("000")&&!b2_export_name("../session.json"));
 if(!strcmp(mode,"paths")){
  assert(!b2_export_command("B2 START"));b2_export_command("B2 EXPORT READ R0000001 ../session.json 0 3");b2_export_command("B2 EXPORT READ R0000001 000000.scraw 0 4097");b2_export_command("B2 EXPORT READ R0000001 000000.scraw 3 1");assert(!binary&&opened==1);
 }else if(!strcmp(mode,"list")||!strcmp(mode,"unexpected")){
  b2_export_command("B2 EXPORT LIST R0000001");assert(strstr(logtext,!strcmp(mode,"list")?"LISTEND count=2":"UNEXPECTED_ENTRY"));
 }else{
  b2_export_command("B2 EXPORT READ R0000001 000000.scraw 0 3");
  if(!strcmp(mode,"ok")){assert(binary==1&&strstr(logtext,"crc32=352441C2"));}
  else{assert(!binary&&strstr(logtext,"B2READ ERROR"));}
 }
 assert(!active);fprintf(stdout,"PASS B2 readonly export %s\n",mode);return 0;
}
