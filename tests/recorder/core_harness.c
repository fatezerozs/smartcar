#include "recorder_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void capture_tests(void)
{
    rec_capture_t p;uint8_t old,next;unsigned i;int held[6];
    rec_capture_init(&p);assert(rec_capture_valid(&p));
    for(i=0;i<6;++i) {
        assert(rec_capture_prepare(&p,i%2,1000U*i,&old,&next));
        assert(p.slot[old].owner==REC_RETIRED);assert(rec_capture_acquire(&p)==REC_NONE);
        assert(old!=p.active[0] && old!=p.active[1]);
        assert(rec_capture_publish(&p,old));held[i]=rec_capture_acquire(&p);assert(held[i]==old);
        assert(!rec_capture_publish(&p,old));assert(rec_capture_valid(&p));
    }
    /* Six CPU-held + two active DMA: never recycle a held payload. */
    assert(!rec_capture_prepare(&p,0,7000,&old,&next));assert(p.exhausted==1);
    assert(!rec_capture_release(&p,p.active[1]));
    for(i=0;i<6;++i){assert(rec_capture_release(&p,(unsigned)held[i]));assert(!rec_capture_release(&p,(unsigned)held[i]));}
    for(i=0;i<100000;++i) {
        assert(rec_capture_prepare(&p,i%2,100000U+i,&old,&next));
        assert(p.slot[old].owner==REC_RETIRED);
        assert(rec_capture_publish(&p,old));assert(rec_capture_acquire(&p)==old);
        assert(rec_capture_release(&p,old));assert(rec_capture_valid(&p));
        if(i%17==0)rec_capture_ambiguous(&p,2);
    }
    assert(p.ambiguous>0 && p.discarded==2*p.ambiguous);
    assert(rec_crc_update(0,"123456789",9)==0xcbf43926U);
    assert(rec_crc_update(rec_crc_update(0,"1234",4),"56789",5)==0xcbf43926U);
}
/* Independent consumer tests inspect these exact bytes, not C struct memory. */
int main(int argc,char **argv)
{
    FILE *f;uint8_t sh[64],rh[40],tail[4],uuid[16],pixels[REC_BYTES];uint32_t crc;
    unsigned i;const char *metadata;
    capture_tests();
    if(argc==1){puts("capture ownership/pressure/100000 transitions/CRC PASS");return 0;}
    if(argc!=3)return 2;
    metadata=argv[2];for(i=0;i<16;++i)uuid[i]=(uint8_t)i;
    f=fopen(argv[1],"wb");if(!f)return 3;
    rec_segment_header(sh,0,uuid,(uint32_t)strlen(metadata),rec_crc_update(0,metadata,strlen(metadata)));
    assert(fwrite(sh,1,64,f)==64);assert(fwrite(metadata,1,strlen(metadata),f)==strlen(metadata));
    for(i=0;i<3;++i) {
        memset(pixels,(int)(20+i),sizeof(pixels));
        rec_record_header(rh,"FRM1",UINT64_C(0x100000000)+i,UINT64_C(0x100000000)*1000U+i*20000U,
                          REC_BYTES,rec_crc_update(0,pixels,REC_BYTES));
        crc=rec_crc_update(rec_crc_update(0,rh,40),pixels,REC_BYTES);rec_put32(tail,crc);
        assert(fwrite(rh,1,40,f)==40);assert(fwrite(pixels,1,REC_BYTES,f)==REC_BYTES);assert(fwrite(tail,1,4,f)==4);
    }
    assert(fclose(f)==0);return 0;
}
