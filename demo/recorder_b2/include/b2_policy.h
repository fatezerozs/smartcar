#ifndef B2_POLICY_H
#define B2_POLICY_H
#include <stdint.h>
#include <stddef.h>
typedef struct {char text[64];unsigned used;uint8_t discard;} b2_line_t;
static inline int b2_line_feed(b2_line_t *p,uint8_t c)
{
 if(c=='\r'||c=='\n'){
  if(p->discard){p->used=0;p->discard=0;return 0;}
  if(!p->used)return 0;
  p->text[p->used]=0;p->used=0;return 1;
 }
 if(p->discard)return 0;
 if(p->used<sizeof(p->text)-1)p->text[p->used++]=(char)c;
 else{p->discard=1;p->used=0;}
 return 0;
}
static inline int b2_window_expired(uint32_t now,uint32_t begin,int recording_started,uint32_t record_begin,uint64_t observed)
{return (uint32_t)(now-begin)>=10000U || (recording_started&&(uint32_t)(now-record_begin)>=3000U) || observed>=100U;}
#ifndef B2_RX_FENCE
#define B2_RX_FENCE() ((void)0)
#endif
typedef struct {volatile uint32_t head,tail,dropped;uint8_t bytes[128];} b2_rx_t;
static inline int b2_rx_push(b2_rx_t *p,uint8_t c)
{
 uint32_t head=p->head;
 if((uint32_t)(head-p->tail)>=sizeof(p->bytes)){p->dropped++;return 0;}
 p->bytes[head&127U]=c;B2_RX_FENCE();p->head=head+1U;return 1;
}
static inline int b2_rx_pop(b2_rx_t *p,uint8_t *out)
{
 uint32_t tail=p->tail;
 if(tail==p->head)return 0;
 B2_RX_FENCE();*out=p->bytes[tail&127U];B2_RX_FENCE();p->tail=tail+1U;return 1;
}
#endif
