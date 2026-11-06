#ifndef B2_METRIC_STATS_H
#define B2_METRIC_STATS_H
#include <stdint.h>
#define B2_METRIC_SAMPLES 512U
typedef struct {uint32_t samples[B2_METRIC_SAMPLES],n,overflow,errors,max;uint64_t us,bytes;} b2_io_stats_t;
static inline uint32_t b2_elapsed_us(uint32_t end,uint32_t begin,uint32_t hz)
{return (uint32_t)(end-begin)/(hz/1000000U);}
static inline void b2_io_add(b2_io_stats_t *p,uint32_t us,uint32_t bytes,int okay)
{
 p->us+=us;p->bytes+=bytes;
 if(p->n<B2_METRIC_SAMPLES)p->samples[p->n++]=us;else ++p->overflow;
 if(us>p->max)p->max=us;if(!okay)++p->errors;
}
/* Caller sorts ascending only after Writer is closed. Nearest-rank percentile. */
static inline uint32_t b2_quantile(const b2_io_stats_t *p,unsigned percent)
{return p->n?p->samples[(p->n*percent+99U)/100U-1]:0;}
#endif
