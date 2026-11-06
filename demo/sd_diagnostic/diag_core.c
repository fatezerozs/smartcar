#include "diag.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BLOCK 4096U
#define LIMIT (16U*1024U*1024U)
static FATFS fs;
static uint8_t block[BLOCK];
static uint32_t latency[LIMIT/BLOCK];
static int mounted,basic_ok;
static struct {unsigned errors,passed;uint32_t written,read,write_ms,read_ms,sync_ms,total_ms,max_ms,p95_ms,p99_ms;char file[24],stage[24];} result;
uint32_t diag_crc(const uint8_t *p,unsigned n){uint32_t c=~0U;unsigned i;while(n--){c^=*p++;for(i=0;i<8;i++)c=(c>>1)^(0xEDB88320U & (0U-(c&1U)));}return ~c;}
static void put32(uint8_t *p,uint32_t x){unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(x>>(i*8));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void pattern(uint32_t seq,uint32_t tag){unsigned i;put32(block,0x53444447);put32(block+4,seq);put32(block+8,tag);for(i=12;i<BLOCK-4;i++)block[i]=(uint8_t)((i*33U)^seq^tag^(seq>>8));put32(block+BLOCK-4,diag_crc(block,BLOCK-4));}
static int verify(uint32_t seq,uint32_t tag){unsigned i;if(get32(block)!=0x53444447||get32(block+4)!=seq||get32(block+8)!=tag||get32(block+BLOCK-4)!=diag_crc(block,BLOCK-4))return 0;for(i=12;i<BLOCK-4;i++)if(block[i]!=(uint8_t)((i*33U)^seq^tag^(seq>>8)))return 0;return 1;}
static int mount_fs(void){FRESULT r=f_mount(&fs,"2:",1);mounted=(r==FR_OK);if(!mounted){char s[64];snprintf(s,sizeof s,"MOUNT FAIL FatFs=%u\r\n",(unsigned)r);diag_emit(s);}return mounted;}
static void show_result(void){char s[360];snprintf(s,sizeof s,"RESULT pass=%u errors=%u stage=%s file=%s written=%lu read=%lu write_ms=%lu read_ms=%lu sync_close_ms=%lu total_ms=%lu max_ms=%lu p95_ms=%lu p99_ms=%lu write_KiBps=%lu read_KiBps=%lu\r\n",result.passed,result.errors,result.stage,result.file,(unsigned long)result.written,(unsigned long)result.read,(unsigned long)result.write_ms,(unsigned long)result.read_ms,(unsigned long)result.sync_ms,(unsigned long)result.total_ms,(unsigned long)result.max_ms,(unsigned long)result.p95_ms,(unsigned long)result.p99_ms,(unsigned long)((uint64_t)result.written*1000/(1024U*(uint64_t)(result.write_ms?result.write_ms:1))),(unsigned long)((uint64_t)result.read*1000/(1024U*(uint64_t)(result.read_ms?result.read_ms:1))));diag_emit(s);}
static int compare(const void *a,const void *b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return x>y?1:x<y?-1:0;}
static void run(int stress){FIL file;FRESULT r;UINT n;uint32_t seq,bytes=stress?LIMIT:64U*1024U,tag=diag_ms(),t,start=diag_ms(),samples=0,i;int opened=0;
 if(stress&&!basic_ok){diag_emit("STRESS BLOCKED: successful BASIC required\r\n");return;}
 basic_ok=0;memset(&result,0,sizeof result);strcpy(result.stage,"mount");
 if(!mount_fs())goto fail;
 if(fs.fs_type!=FS_FAT16&&fs.fs_type!=FS_FAT32&&fs.fs_type!=FS_FAT12){strcpy(result.stage,"filesystem");goto fail;}
 {DWORD free_clusters;FATFS *fp;strcpy(result.stage,"free-space");if(f_getfree("2:",&free_clusters,&fp)!=FR_OK||(uint64_t)free_clusters*fp->csize*512U<(uint64_t)bytes+65536U)goto fail;}
 strcpy(result.stage,"create-new");for(i=0;i<10000000;i++){snprintf(result.file,sizeof result.file,"2:/S%07lu.BIN",(unsigned long)i);r=f_open(&file,result.file,FA_WRITE|FA_CREATE_NEW);if(r==FR_OK){opened=1;break;}if(r!=FR_EXIST)goto fail;}if(!opened)goto fail;
 strcpy(result.stage,"write");t=diag_ms();for(seq=0;seq<bytes/BLOCK;seq++){uint32_t a,b;pattern(seq,tag);a=diag_ms();r=f_write(&file,block,BLOCK,&n);b=diag_ms()-a;latency[samples++]=b;if(b>result.max_ms)result.max_ms=b;result.written+=n;if(r!=FR_OK||n!=BLOCK)goto fail;}result.write_ms=diag_ms()-t;
 strcpy(result.stage,"sync-close");t=diag_ms();if(f_sync(&file)!=FR_OK)goto fail;r=f_close(&file);opened=0;if(r!=FR_OK)goto fail;result.sync_ms=diag_ms()-t;
 strcpy(result.stage,"reopen");if(f_open(&file,result.file,FA_READ)!=FR_OK)goto fail;opened=1;
 if(f_size(&file)!=bytes){strcpy(result.stage,"size");goto fail;}
 strcpy(result.stage,"read-crc");t=diag_ms();for(seq=0;seq<bytes/BLOCK;seq++){r=f_read(&file,block,BLOCK,&n);result.read+=n;if(r!=FR_OK||n!=BLOCK||!verify(seq,tag))goto fail;}result.read_ms=diag_ms()-t;
 strcpy(result.stage,"read-close");r=f_close(&file);opened=0;if(r!=FR_OK)goto fail;result.passed=1;basic_ok=stress?0:1;strcpy(result.stage,"done");goto done;
fail:result.errors++;if(opened&&f_close(&file)!=FR_OK)result.errors++;
done:if(samples){qsort(latency,samples,sizeof latency[0],compare);result.p95_ms=latency[(samples*95U+99U)/100U-1U];result.p99_ms=latency[(samples*99U+99U)/100U-1U];}result.total_ms=diag_ms()-start;show_result();}
void diag_command(const char *s)
{
    if (!strcmp(s, "SDDIAG HELP")) {
        diag_emit("SDDIAG HELP | STATUS | RUN BASIC | RUN STRESS | RESULT\r\n");
    } else if (!strcmp(s, "SDDIAG STATUS")) {
        char b[160];
        DWORD c;
        FATFS *fp;
        if (mount_fs()) {
            if (f_getfree("2:", &c, &fp) == FR_OK) {
                snprintf(b, sizeof b,
                    "FS mounted=1 type=%u cluster_sectors=%u free_bytes=%llu\r\n",
                    fp->fs_type, fp->csize,
                    (unsigned long long)c * fp->csize * 512U);
                diag_emit(b);
            } else {
                diag_emit("FS mounted=1 free=unknown\r\n");
            }
        } else {
            diag_emit("FS mounted=0 free=unknown\r\n");
        }
        diag_status();
    } else if (!strcmp(s, "SDDIAG RUN BASIC")) {
        run(0);
    } else if (!strcmp(s, "SDDIAG RUN STRESS")) {
        run(1);
    } else if (!strcmp(s, "SDDIAG RESULT")) {
        show_result();
    } else {
        diag_emit("ERROR unknown command; SDDIAG HELP\r\n");
    }
}
