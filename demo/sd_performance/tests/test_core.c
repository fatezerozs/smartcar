#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "diag.h"
static unsigned tick,mode,open_count,close_count,writes,reads,syncs;
static unsigned offset,size;
static unsigned char data[16U*1024U*1024U];
static char output[4096];
uint32_t diag_ms(void){return tick++;}
void diag_emit(const char *s){strncat(output,s,sizeof output-strlen(output)-1);}
void diag_status(void){diag_emit("MOCK STATUS\n");}
FRESULT f_mount(FATFS *fs,const TCHAR *path,BYTE now){(void)path;(void)now;fs->fs_type=FS_FAT32;fs->csize=8;return mode==1?FR_NO_FILESYSTEM:FR_OK;}
FRESULT f_getfree(const TCHAR *path,DWORD *c,FATFS **fp){static FATFS fs;(void)path;fs.csize=8;*fp=&fs;*c=mode==8?0:100000;return FR_OK;}
FRESULT f_open(FIL *f,const TCHAR *path,BYTE flags){(void)path;open_count++;offset=0;if(flags&FA_CREATE_NEW){if(mode==9&&open_count<4)return FR_EXIST;size=0;}else{if(mode==6)return FR_DISK_ERR;if(mode==3)data[100]^=1;}f->obj.objsize=size;return FR_OK;}
FRESULT f_write(FIL *f,const void *b,UINT n,UINT *actual){(void)f;writes++;if(mode==2)n--;memcpy(data+offset,b,n);offset+=n;size=offset;*actual=n;return mode==10?FR_DISK_ERR:FR_OK;}
FRESULT f_read(FIL *f,void *b,UINT n,UINT *actual){(void)f;reads++;if(mode==7)n--;if(n>size-offset)n=size-offset;memcpy(b,data+offset,n);offset+=n;*actual=n;return FR_OK;}
FRESULT f_sync(FIL *f){(void)f;syncs++;return mode==4?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL *f){(void)f;close_count++;return mode==5?FR_DISK_ERR:FR_OK;}
static unsigned bench_calls;
int diag_bench(uint32_t hz){(void)hz;bench_calls++;return 1;}
void diag_bench_result(void){}
int diag_export_command(const char *s){(void)s;return 0;}
#include "../diag_core.c"
static void reset(unsigned m){mode=m;tick=open_count=close_count=writes=reads=syncs=offset=size=0;output[0]=0;basic_ok=0;memset(&result,0,sizeof result);}
int main(void){unsigned i;assert(diag_crc((const uint8_t*)"123456789",9)==0xCBF43926U);puts("PASS CRC32 IEEE known vector");
 reset(0);diag_command("SDDIAG RUN STRESS");assert(!writes&&strstr(output,"BLOCKED"));puts("PASS STRESS requires BASIC");
 for(i=1;i<=10;i++){reset(i);diag_command("SDDIAG RUN BASIC");if(i==9){assert(result.passed&&open_count==5);assert(strstr(result.file,"S0000003.BIN"));}else{assert(!result.passed&&result.errors>0&&!basic_ok);}printf("PASS scenario %u %s\n",i,i==1?"mount failure":i==2?"short write":i==3?"CRC corruption":i==4?"sync failure":i==5?"close failure":i==6?"reopen failure":i==7?"short read":i==8?"insufficient space":i==9?"CREATE_NEW collisions":"write error");}
 reset(0);diag_command("SDDIAG RUN BASIC");assert(result.passed&&basic_ok&&result.written==65536&&result.read==65536);assert(writes==16&&reads==16&&syncs==1&&close_count==2);puts("PASS BASIC 64KiB write/sync/close/reopen/verify");
 diag_command("SDDIAG RUN STRESS");assert(result.passed&&result.written==16777216&&result.read==16777216&&!basic_ok);assert(writes==4112&&reads==4112&&result.p95_ms==1&&result.p99_ms==1);puts("PASS bounded STRESS 16MiB, percentiles, manual gating");
 reset(0);diag_command("SDDIAG STATUS");diag_command("SDDIAG HELP");diag_command("SDDIAG RESULT");diag_command("bad");assert(!writes&&strstr(output,"unknown"));puts("PASS command routing, STATUS no file writes");reset(0);bench_calls=0;diag_command("SDDIAG RUN BENCH50");diag_command("SDDIAG RUN BENCH25");assert(!bench_calls);
 diag_command("SDDIAG RUN BASIC");assert(basic_ok);diag_command("SDDIAG RUN BENCH50");assert(!bench_calls);diag_command("SDDIAG RUN BENCH25");assert(bench_calls==1);diag_command("SDDIAG RUN BENCH50");assert(bench_calls==2&&!basic_ok&&!bench25_ok);diag_command("SDDIAG RUN BENCH50");assert(bench_calls==2);puts("PASS BASIC -> BENCH25 -> BENCH50 gating and consumed authorization");
 puts("ALL 16 CORE CHECKS PASS; MOCK I/O ONLY; HARDWARE UNVERIFIED");return 0;}
