#include "diag.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PERF_CHUNK 65536U
#define PERF_BYTES (16U*1024U*1024U)
#define PERF_SAMPLES (PERF_BYTES/PERF_CHUNK)
static uint8_t perf_buffer[PERF_CHUNK] __attribute__((aligned(64)));
static uint32_t crc_table[256],latencies[PERF_SAMPLES];
static struct {
    uint32_t requested_hz,written,read,write_io_us,write_sync_us,read_io_us;
    uint32_t read_verified_us,prealloc_us,sync_us,total_ms,max_us,p95_us,p99_us;
    unsigned passed,errors,fatfs_error;
    char file[24],stage[24];
} perf;
static void put32(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void init_table(void)
{
    unsigned i,j;
    for(i=0;i<256;i++){
        uint32_t c=i;
        for(j=0;j<8;j++)c=(c>>1)^(0xEDB88320U&(0U-(c&1U)));
        crc_table[i]=c;
    }
}
static uint32_t fast_crc(const uint8_t *p,unsigned n)
{
    uint32_t c=~0U;
    while(n--)c=(c>>8)^crc_table[(c^*p++)&255U];
    return ~c;
}
static uint8_t payload(unsigned i){return (uint8_t)((i*37U)^(i>>8)^0xA5U);}
static unsigned rate(uint32_t bytes,uint32_t us)
{
    return us?(unsigned)((uint64_t)bytes*1000000U/(1024U*(uint64_t)us)):0;
}
void diag_bench_result(void)
{
    char s[620];
    snprintf(s,sizeof s,
        "PERF RESULT pass=%u errors=%u stage=%s fatfs=%u file=%s requested_hz=%lu chunk_bytes=65536 written=%lu read=%lu prealloc_us=%lu write_io_us=%lu write_sync_us=%lu read_io_us=%lu read_verified_us=%lu sync_us=%lu total_ms=%lu max_us=%lu p95_us=%lu p99_us=%lu write_io_KiBps=%u write_sync_KiBps=%u read_io_KiBps=%u read_verified_KiBps=%u\r\n",
        perf.passed,perf.errors,perf.stage,perf.fatfs_error,perf.file,
        (unsigned long)perf.requested_hz,(unsigned long)perf.written,(unsigned long)perf.read,
        (unsigned long)perf.prealloc_us,(unsigned long)perf.write_io_us,
        (unsigned long)perf.write_sync_us,(unsigned long)perf.read_io_us,
        (unsigned long)perf.read_verified_us,(unsigned long)perf.sync_us,
        (unsigned long)perf.total_ms,(unsigned long)perf.max_us,
        (unsigned long)perf.p95_us,(unsigned long)perf.p99_us,
        rate(perf.written,perf.write_io_us),rate(perf.written,perf.write_sync_us),
        rate(perf.read,perf.read_io_us),rate(perf.read,perf.read_verified_us));
    diag_emit(s);
}
static int compare_latency(const void *a,const void *b)
{
    uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;
    return x>y?1:x<y?-1:0;
}
int diag_bench(uint32_t hz)
{
    FIL file;
    FATFS *fs;
    DWORD free_clusters;
    FRESULT r=FR_OK;
    UINT n=0;
    uint32_t start=diag_ms(),tag=start,t=0,stream_start=0,seq,i,samples=0;
    int opened=0;
    memset(&perf,0,sizeof perf);perf.requested_hz=hz;
    strcpy(perf.stage,"space");
    r=f_getfree("2:",&free_clusters,&fs);
    if(r!=FR_OK)goto fail;
    if((fs->fs_type!=FS_FAT16&&fs->fs_type!=FS_FAT32)||
       (uint64_t)free_clusters*fs->csize*512U<PERF_BYTES+65536U)goto fail;
    strcpy(perf.stage,"clock");
    if(!diag_set_clock(hz))goto fail;
    init_table();
    for(i=12;i<PERF_CHUNK-4;i++)perf_buffer[i]=payload(i);
    put32(perf_buffer,0x50455246U);put32(perf_buffer+8,tag);
    strcpy(perf.stage,"create-new");
    for(i=0;i<10000000U;i++){
        snprintf(perf.file,sizeof perf.file,"2:/P%07lu.BIN",(unsigned long)i);
        r=f_open(&file,perf.file,FA_WRITE|FA_CREATE_NEW);
        if(r==FR_OK){opened=1;break;}
        if(r!=FR_EXIST)goto fail;
    }
    if(!opened)goto fail;
    diag_emit("PERF phase=preallocate bytes=16777216\r\n");
    strcpy(perf.stage,"preallocate");t=diag_us();
    r=f_expand(&file,PERF_BYTES,1);if(r!=FR_OK)goto fail;
    r=f_sync(&file);if(r!=FR_OK)goto fail;
    perf.prealloc_us=diag_us()-t;
    diag_emit("PERF phase=write chunk_bytes=65536\r\n");
    strcpy(perf.stage,"write");stream_start=diag_us();
    for(seq=0;seq<PERF_SAMPLES;seq++){
        uint32_t elapsed;
        put32(perf_buffer+4,seq);
        put32(perf_buffer+PERF_CHUNK-4,fast_crc(perf_buffer,PERF_CHUNK-4));
        t=diag_us();n=0;r=f_write(&file,perf_buffer,PERF_CHUNK,&n);elapsed=diag_us()-t;
        perf.write_io_us+=elapsed;latencies[samples++]=elapsed;
        if(elapsed>perf.max_us)perf.max_us=elapsed;
        perf.written+=n;
        if(r!=FR_OK||n!=PERF_CHUNK)goto fail;
    }
    strcpy(perf.stage,"sync-close");t=diag_us();
    r=f_sync(&file);if(r!=FR_OK)goto fail;
    r=f_close(&file);opened=0;if(r!=FR_OK)goto fail;
    perf.sync_us=diag_us()-t;perf.write_sync_us=diag_us()-stream_start;
    diag_emit("PERF phase=read-verify\r\n");
    strcpy(perf.stage,"reopen");r=f_open(&file,perf.file,FA_READ);
    if(r!=FR_OK)goto fail;
    opened=1;
    if(f_size(&file)!=PERF_BYTES){strcpy(perf.stage,"size");goto fail;}
    strcpy(perf.stage,"read-crc");stream_start=diag_us();
    for(seq=0;seq<PERF_SAMPLES;seq++){
        t=diag_us();n=0;r=f_read(&file,perf_buffer,PERF_CHUNK,&n);
        perf.read_io_us+=diag_us()-t;perf.read+=n;
        if(r!=FR_OK||n!=PERF_CHUNK)goto fail;
        if(get32(perf_buffer)!=0x50455246U||get32(perf_buffer+4)!=seq||
           get32(perf_buffer+8)!=tag||get32(perf_buffer+PERF_CHUNK-4)!=fast_crc(perf_buffer,PERF_CHUNK-4))goto fail;
        for(i=12;i<PERF_CHUNK-4;i++)if(perf_buffer[i]!=payload(i))goto fail;
    }
    r=f_close(&file);opened=0;if(r!=FR_OK)goto fail;
    perf.read_verified_us=diag_us()-stream_start;
    perf.passed=1;strcpy(perf.stage,"done");goto done;
fail:
    perf.errors++;perf.fatfs_error=(unsigned)r;
    if(opened&&f_close(&file)!=FR_OK)perf.errors++;
done:
    if(samples){
        qsort(latencies,samples,sizeof latencies[0],compare_latency);
        perf.p95_us=latencies[(samples*95U+99U)/100U-1U];
        perf.p99_us=latencies[(samples*99U+99U)/100U-1U];
    }
    perf.total_ms=diag_ms()-start;diag_bench_result();
    return perf.passed?1:0;
}
