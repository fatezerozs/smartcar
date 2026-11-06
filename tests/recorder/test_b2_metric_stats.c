#include "b2_metric_stats.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){b2_io_stats_t p=B2_IO_STATS_INIT;unsigned i;
 assert(b2_elapsed_us(0x200U,0xfffffda8U,600000000U)==1);
 assert(b2_quantile(&p,99)==0);
 b2_io_add(&p,30,3,1);b2_io_add(&p,10,4,1);b2_io_add(&p,20,5,0);
 assert(b2_stats_sort(&p));
 assert(p.n==3&&p.us==60&&p.bytes==12&&p.errors==1&&p.max==30);
 assert(b2_quantile(&p,50)==20&&b2_quantile(&p,95)==30&&b2_quantile(&p,99)==30);
 memset(&p,0,sizeof p);p.guard_lo=p.guard_hi=B2_METRIC_GUARD;for(i=0;i<515;i++)b2_io_add(&p,i,1,1);
 assert(p.n==512&&p.overflow==3&&p.max==514&&p.bytes==515&&p.us==515ULL*514/2);
 assert(b2_quantile(&p,50)==255&&b2_quantile(&p,95)==486&&b2_quantile(&p,99)==506);
 memset(&p,0,sizeof p);p.guard_lo=p.guard_hi=B2_METRIC_GUARD;for(i=0;i<512;i++)b2_io_add(&p,0,0,1);assert(b2_stats_sort(&p));assert(p.n==512&&!p.overflow&&!p.errors&&!p.max);
 for(i=0;i<512;i++)p.samples[i]=511-i;assert(b2_stats_sort(&p));for(i=0;i<512;i++)assert(p.samples[i]==i);
 p.n=513;assert(!b2_stats_sort(&p));
 p.n=512;p.guard_hi=0;assert(!b2_stats_valid(&p)&&!b2_stats_sort(&p));
 puts("PASS B2 metrics: cycle wrap, nearest-rank percentiles, sums, errors and bounded sample overflow");return 0;
}
