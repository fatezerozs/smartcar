#ifndef B2_METRICS_H
#define B2_METRICS_H
#include <stdint.h>
uint32_t b2_io_begin(void);
void b2_io_end(unsigned kind,uint32_t begin,uint32_t bytes,int okay);
void b2_metrics_report(void);
extern volatile uint32_t b2_csi_irq_count,b2_csi_irq_cycles_max,b2_sd_irq_count,b2_sd_irq_cycles_max;
#endif
