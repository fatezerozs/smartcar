#include "diag.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mode,opens,reads,closed,mounts,baud_calls;
static unsigned long offset;
static uint8_t output[70000];static unsigned used;
void diag_emit(const char *s){unsigned n=(unsigned)strlen(s);memcpy(output+used,s,n);used+=n;output[used]=0;}
void diag_send_bytes(const uint8_t *p,unsigned n){memcpy(output+used,p,n);used+=n;output[used]=0;}
int diag_uart_baud(uint32_t b){assert(b==115200||b==460800||b==921600);baud_calls++;return mode!=6;}
uint32_t diag_crc(const uint8_t *p,unsigned n){uint32_t c=~0U;unsigned j;while(n--){c^=*p++;for(j=0;j<8;j++)c=(c>>1)^(0xEDB88320U&(0U-(c&1U)));}return ~c;}
FRESULT f_mount(FATFS *f,const TCHAR *s,BYTE now){(void)f;assert(!strcmp(s,"2:")&&now==1);mounts++;return mode==1?FR_NOT_READY:FR_OK;}
FRESULT f_open(FIL *f,const TCHAR *s,BYTE flags){assert(!strcmp(s,"2:/P0000000.BIN")||!strcmp(s,"2:/S0000000.BIN"));assert(flags==FA_READ);opens++;f->obj.objsize=65536;return mode==2?FR_NO_FILE:FR_OK;}
FRESULT f_lseek(FIL *f,FSIZE_t off){(void)f;offset=off;return mode==3?FR_DISK_ERR:FR_OK;}
FRESULT f_read(FIL *f,void *p,UINT n,UINT *got){(void)f;reads++;memset(p,0,n);if(!offset&&n>=3)memcpy(p,"abc",3);*got=mode==4?n-1:n;return mode==7?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL *f){(void)f;closed++;return mode==5?FR_DISK_ERR:FR_OK;}
#include "../export.c"
static void reset(unsigned m){mode=m;opens=reads=closed=mounts=baud_calls=used=0;offset=0;output[0]=0;export_mounted=0;}
static void error_expected(const char *command,unsigned m){reset(m);assert(diag_export_command(command));assert(strstr((char*)output,"SDREAD ERROR"));assert(!strstr((char*)output,"SDREAD DATA"));}
int main(void){unsigned i;
 reset(0);assert(!diag_export_command("SDDIAG RUN BASIC")&&!opens);puts("PASS unrelated commands not intercepted");
 error_expected("SDDIAG EXPORT INFO ../bad",0);assert(!opens);puts("PASS whitelist rejects paths");
 reset(0);assert(diag_export_command("SDDIAG EXPORT INFO P0000000.BIN"));assert(strstr((char*)output,"size=65536")&&closed==1&&!reads);puts("PASS INFO FA_READ only");
 reset(0);assert(diag_export_command("SDDIAG EXPORT READ P0000000.BIN 0 3"));assert(strstr((char*)output,"length=3 crc32=352441C2\r\nabc\r\nSDREAD END\r\n"));assert(closed==1&&reads==1);puts("PASS raw framing and independent known CRC");
 for(i=1;i<=5;i++){error_expected("SDDIAG EXPORT READ P0000000.BIN 0 3",i);printf("PASS failure path %u\n",i);}
 error_expected("SDDIAG EXPORT READ P0000000.BIN 0 3",7);puts("PASS read API error");
 error_expected("SDDIAG EXPORT READ P0000000.BIN 0 0",0);assert(!opens);
 error_expected("SDDIAG EXPORT READ P0000000.BIN 0 32769",0);assert(!opens);
 error_expected("SDDIAG EXPORT READ P0000000.BIN 65536 1",0);assert(closed==1);
 error_expected("SDDIAG EXPORT READ P0000000.BIN 0 3 extra",0);assert(!opens);puts("PASS length/range/trailing input bounds");
 reset(0);assert(diag_export_command("SDDIAG EXPORT BAUD 921600"));assert(baud_calls==1&&!opens&&strstr((char*)output,"next=921600"));puts("PASS explicit high baud switch");
 error_expected("SDDIAG EXPORT BAUD 12345",0);assert(!baud_calls&&!opens);puts("PASS unsupported baud rejected");
 error_expected("SDDIAG EXPORT BAUD 921600",6);puts("PASS baud switch failure reported");
 puts("EXPORT TESTS PASS; no write/format/delete API linked into this fixture");return 0;}
