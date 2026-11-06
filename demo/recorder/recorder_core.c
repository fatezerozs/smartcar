#include "recorder_core.h"
#include <string.h>
void rec_capture_init(rec_capture_t *p)
{
    memset(p,0,sizeof(*p));p->active[0]=0;p->active[1]=1;
    p->slot[0].owner=p->slot[1].owner=REC_DMA;
}
int rec_capture_prepare(rec_capture_t *p,unsigned dma,uint64_t us,uint8_t *old,uint8_t *next)
{
    unsigned i;uint8_t at;
    if(dma>1 || !old || !next)return 0;
    ++p->observed;at=p->active[dma];
    for(i=0;i<REC_CAPTURE_SLOTS;++i)if(p->slot[i].owner==REC_FREE)break;
    if(i==REC_CAPTURE_SLOTS){++p->exhausted;return 0;}
    p->slot[i].owner=REC_DMA;p->active[dma]=(uint8_t)i;
    p->slot[at].owner=REC_RETIRED;p->slot[at].sequence=p->observed;
    p->slot[at].timestamp_us=us;*old=at;*next=(uint8_t)i;return 1;
}
int rec_capture_publish(rec_capture_t *p,unsigned at)
{
    if(at>=REC_CAPTURE_SLOTS || p->slot[at].owner!=REC_RETIRED)return 0;
    p->slot[at].owner=REC_READY;return 1;
}
int rec_capture_acquire(rec_capture_t *p)
{
    unsigned i,at=REC_NONE;
    for(i=0;i<REC_CAPTURE_SLOTS;++i)
        if(p->slot[i].owner==REC_READY && (at==REC_NONE ||
            p->slot[i].sequence<p->slot[at].sequence))at=i;
    if(at!=REC_NONE)p->slot[at].owner=REC_HELD;
    return (int)at;
}
int rec_capture_release(rec_capture_t *p,unsigned at)
{
    if(at>=REC_CAPTURE_SLOTS || p->slot[at].owner!=REC_HELD)return 0;
    p->slot[at].owner=REC_FREE;return 1;
}
void rec_capture_ambiguous(rec_capture_t *p,unsigned observed_completions)
{
    /* Two sticky completion flags do not prove how many frames elapsed. */
    ++p->ambiguous;p->observed+=observed_completions;p->discarded+=observed_completions;
}
int rec_capture_valid(const rec_capture_t *p)
{
    unsigned i,n=0;
    if(p->active[0]>=REC_CAPTURE_SLOTS || p->active[1]>=REC_CAPTURE_SLOTS ||
       p->active[0]==p->active[1])return 0;
    for(i=0;i<REC_CAPTURE_SLOTS;++i)if(p->slot[i].owner==REC_DMA)++n;
    return n==2 && p->slot[p->active[0]].owner==REC_DMA &&
        p->slot[p->active[1]].owner==REC_DMA;
}
uint32_t rec_crc_update(uint32_t crc,const void *bytes,size_t size)
{
    static const uint32_t t[16]={0,0x1db71064,0x3b6e20c8,0x26d930ac,
        0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,0xedb88320,
        0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,
        0xa00ae278,0xbdbdf21c};
    const uint8_t *p=(const uint8_t *)bytes;crc=~crc;
    while(size--){crc^=*p++;crc=(crc>>4)^t[crc&15];crc=(crc>>4)^t[crc&15];}
    return ~crc;
}
static void put16(uint8_t *p,uint16_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
void rec_put32(uint8_t *p,uint32_t n)
{unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n)
{rec_put32(p,(uint32_t)n);rec_put32(p+4,(uint32_t)(n>>32));}
void rec_segment_header(uint8_t out[64],uint32_t segment,const uint8_t uuid[16],uint32_t n,uint32_t crc)
{
    memset(out,0,64);memcpy(out,"SCRAW1\r\n",8);put16(out+8,1);put16(out+12,64);
    rec_put32(out+16,segment);put16(out+20,REC_WIDTH);put16(out+22,REC_HEIGHT);
    put16(out+24,REC_WIDTH);put16(out+26,1);out[28]=out[29]=1;
    memcpy(out+32,uuid,16);rec_put32(out+48,n);rec_put32(out+52,crc);
    rec_put32(out+60,rec_crc_update(0,out,60));
}
void rec_record_header(uint8_t out[40],const char tag[4],uint64_t seq,uint64_t us,uint32_t n,uint32_t crc)
{
    memset(out,0,40);memcpy(out,tag,4);rec_put32(out+4,n+44);
    put64(out+8,seq);put64(out+16,us);rec_put32(out+24,n);rec_put32(out+28,crc);
    if(memcmp(tag,"FRM1",4))rec_put32(out+32,1); /* No associated source frame. */
    rec_put32(out+36,rec_crc_update(0,out,36));
}
