#include "diag.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static unsigned mode,clock_calls,opens,writes,reads,closes,syncs,expands,pos,size,timer;
static unsigned char disk[16777216];
static char output[8192];
uint32_t diag_ms(void){return timer/1000;}
uint32_t diag_us(void){timer+=100;return timer;}
void diag_emit(const char *s){strncat(output,s,sizeof(output)-strlen(output)-1);}
void diag_status(void){}
int diag_set_clock(uint32_t hz){assert(hz==25000000||hz==50000000);clock_calls++;return mode!=1;}
FRESULT f_getfree(const TCHAR *s,DWORD *n,FATFS **fp){static FATFS fs;(void)s;fs.fs_type=FS_FAT32;fs.csize=64;*fp=&fs;*n=mode==7?0:10000;return mode==2?FR_DISK_ERR:FR_OK;}
FRESULT f_open(FIL *f,const TCHAR *s,BYTE flags){(void)s;opens++;pos=0;if(flags&FA_CREATE_NEW){if(mode==8&&opens<3)return FR_EXIST;size=0;}else{if(mode==14)return FR_DISK_ERR;if(mode==4)disk[100]^=1;}f->obj.objsize=mode==10?size-1:size;return FR_OK;}
FRESULT f_expand(FIL *f,FSIZE_t n,BYTE opt){(void)f;assert(n==16777216&&opt==1);expands++;size=(unsigned)n;return mode==6?FR_DENIED:FR_OK;}
FRESULT f_write(FIL *f,const void *b,UINT n,UINT *out){(void)f;writes++;if(mode==3)n--;memcpy(disk+pos,b,n);pos+=n;*out=n;return mode==12?FR_DISK_ERR:FR_OK;}
FRESULT f_read(FIL *f,void *b,UINT n,UINT *out){(void)f;reads++;if(mode==11)n--;assert(pos+n<=sizeof disk);memcpy(b,disk+pos,n);pos+=n;*out=n;return FR_OK;}
FRESULT f_sync(FIL *f){(void)f;syncs++;return mode==5?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL *f){(void)f;closes++;return mode==13?FR_DISK_ERR:FR_OK;}
#include "../bench.c"
static void reset(unsigned m){mode=m;clock_calls=opens=writes=reads=closes=syncs=expands=pos=size=timer=0;output[0]=0;}
int main(void){unsigned i;
 init_table();assert(fast_crc((const uint8_t*)"123456789",9)==0xCBF43926U);puts("PASS table CRC32 known vector");
 for(i=1;i<=14;i++){if(i==9)continue;reset(i);if(i==8){assert(diag_bench(25000000)==1&&opens==4);assert(strstr(perf.file,"P0000002.BIN"));}else{assert(diag_bench(25000000)==0&&perf.errors>0);if(i==1||i==2||i==7)assert(!writes&&!opens);}printf("PASS failure/collision scenario %u\n",i);}
 reset(0);assert(diag_bench(25000000)==1);assert(perf.written==16777216&&perf.read==16777216);assert(writes==256&&reads==256&&syncs==2&&closes==2&&expands==1);assert(perf.write_io_us==25600&&perf.read_io_us==25600);assert(perf.max_us==100&&perf.p95_us==100&&perf.p99_us==100);assert(strstr(output,"read_verified_KiBps=")&&output[strlen(output)-1]=='\n');puts("PASS 16MiB 25MHz mode, full read/CRC, exact bounded calls and untruncated result");
 reset(0);assert(diag_bench(50000000)==1&&perf.requested_hz==50000000);puts("PASS 50MHz request and timing accounting");
 puts("ALL 16 BENCH CHECKS PASS; MOCK I/O ONLY");return 0;}
