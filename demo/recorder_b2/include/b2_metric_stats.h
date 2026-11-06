#ifndef B2_METRIC_STATS_H
#define B2_METRIC_STATS_H
#include <stdint.h>
#define B2_METRIC_SAMPLES 512U
#define B2_METRIC_GUARD 0xB2A55A2BU
#define B2_IO_STATS_INIT {.guard_lo=B2_METRIC_GUARD,.guard_hi=B2_METRIC_GUARD}
typedef struct {uint32_t guard_lo,samples[B2_METRIC_SAMPLES],n,overflow,errors,max;uint64_t us,bytes;uint32_t guard_hi;} b2_io_stats_t;
static inline int b2_stats_valid(const b2_io_stats_t *p)
{return p->guard_lo==B2_METRIC_GUARD&&p->guard_hi==B2_METRIC_GUARD&&p->n<=B2_METRIC_SAMPLES;}
static inline uint32_t b2_elapsed_us(uint32_t end,uint32_t begin,uint32_t hz)
{return (uint32_t)(end-begin)/(hz/1000000U);}
static inline void b2_io_add(b2_io_stats_t *p,uint32_t us,uint32_t bytes,int okay)
{
 p->us+=us;p->bytes+=bytes;
 if(p->n<B2_METRIC_SAMPLES)p->samples[p->n++]=us;else ++p->overflow;
 if(us>p->max)p->max=us;if(!okay)++p->errors;
}
/* Fixed-space insertion sort: <=512 elements, <=130816 shifts, no recursion.
 * Many zero-duration small writes must not trigger a recursive library sort.
 */
static inline int b2_stats_sort(b2_io_stats_t *p)
{
 uint32_t i,j,value;if(!b2_stats_valid(p))return 0;
 for(i=1;i<p->n;i++){value=p->samples[i];j=i;while(j&&p->samples[j-1]>value){p->samples[j]=p->samples[j-1];--j;}p->samples[j]=value;}
 return 1;
}
/* Caller sorts ascending only after Writer is closed. Nearest-rank percentile. */
static inline uint32_t b2_quantile(const b2_io_stats_t *p,unsigned percent)
{return p->n?p->samples[(p->n*percent+99U)/100U-1]:0;}
#endif
