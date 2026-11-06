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
#endif
