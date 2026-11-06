#include "main.h"
#include "task.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "b2_metrics.h"
#include "b2_metric_stats.h"
#include <stdio.h>
#include <string.h>
static b2_io_stats_t io[3]={B2_IO_STATS_INIT,B2_IO_STATS_INIT,B2_IO_STATS_INIT};
static uint32_t runtime_last;
static uint64_t runtime_cycles;
volatile uint32_t b2_csi_irq_count,b2_csi_irq_cycles_max,b2_sd_irq_count,b2_sd_irq_cycles_max;
void b2_runtime_init(void){runtime_last=DWT->CYCCNT;runtime_cycles=0;}
uint32_t b2_runtime_us(void)
{
 uint32_t saved=__get_PRIMASK(),now,result;__disable_irq();now=DWT->CYCCNT;
 runtime_cycles+=(uint32_t)(now-runtime_last);runtime_last=now;
 result=(uint32_t)(runtime_cycles/(SystemCoreClock/1000000U));__set_PRIMASK(saved);return result;
}
uint32_t b2_io_begin(void){return DWT->CYCCNT;}
void b2_io_end(unsigned kind,uint32_t begin,uint32_t bytes,int okay)
{
 if(kind<3&&b2_stats_valid(&io[kind]))b2_io_add(&io[kind],b2_elapsed_us(DWT->CYCCNT,begin,SystemCoreClock),bytes,okay);
}
void b2_metrics_report(void)
{
 static const char *names[]={"write","sync","close"};unsigned i;TaskStatus_t tasks[4];uint32_t total=0,idle=0;UBaseType_t n;rec_sd_status_t sd;
 /* Writer state4 is a publication point after sync/close/unmount; no further I/O samples. */
 for(i=0;i<3;i++)if(!b2_stats_valid(&io[i])){printf("B2METRIC ERROR stats_guard\r\n");return;}
 recorder_sd_status(&sd);n=uxTaskGetSystemState(tasks,4,&total);
 printf("B2METRIC session=%s csi_width_cycles=%lu csi_height=%lu csi_cr1=%08lX csi_cr18=%08lX sd_hz=%lu sd_bits=%u\r\n",recorder_session_directory(),(unsigned long)((CSI_REG_IMAG_PARA(CSI)&CSI_IMAG_PARA_IMAGE_WIDTH_MASK)>>CSI_IMAG_PARA_IMAGE_WIDTH_SHIFT),(unsigned long)(CSI_REG_IMAG_PARA(CSI)&CSI_IMAG_PARA_IMAGE_HEIGHT_MASK),(unsigned long)CSI_REG_CR1(CSI),(unsigned long)CSI_REG_CR18(CSI),(unsigned long)sd.clock_hz,sd.bus_bits);
 for(i=0;i<n;i++){
  if(!strcmp(tasks[i].pcTaskName,"IDLE"))idle=tasks[i].ulRunTimeCounter;
  printf("B2METRIC task=%s runtime_us=%lu stack_free_words=%lu\r\n",tasks[i].pcTaskName,(unsigned long)tasks[i].ulRunTimeCounter,(unsigned long)tasks[i].usStackHighWaterMark);
 }
 printf("B2METRIC runtime_total_us=%lu idle_attributed_us=%lu nonidle_permille=%lu csi_irq_count=%lu csi_irq_max_us=%lu sd_irq_count=%lu sd_irq_max_us=%lu\r\n",(unsigned long)total,(unsigned long)idle,(unsigned long)(total?1000U-(uint64_t)idle*1000U/total:0),(unsigned long)b2_csi_irq_count,(unsigned long)(b2_csi_irq_cycles_max/(SystemCoreClock/1000000U)),(unsigned long)b2_sd_irq_count,(unsigned long)(b2_sd_irq_cycles_max/(SystemCoreClock/1000000U)));
 for(i=0;i<3;i++){
  b2_io_stats_t *p=&io[i];if(!b2_stats_sort(p)){printf("B2METRIC ERROR sample_count\r\n");return;}
  printf("B2METRIC io=%s count=%lu sample_overflow=%lu errors=%lu bytes=%llu total_us=%llu p50_us=%lu p95_us=%lu p99_us=%lu max_us=%lu\r\n",names[i],(unsigned long)p->n,(unsigned long)p->overflow,(unsigned long)p->errors,(unsigned long long)p->bytes,(unsigned long long)p->us,(unsigned long)b2_quantile(p,50),(unsigned long)b2_quantile(p,95),(unsigned long)b2_quantile(p,99),(unsigned long)p->max);
 }
 {unsigned free_words=(unsigned)uxTaskGetStackHighWaterMark(NULL);
  printf("B2METRIC report_stack_free_words=%u\r\n",free_words);
  for(i=0;i<3;i++)if(!b2_stats_valid(&io[i])){printf("B2METRIC ERROR stats_guard\r\n");return;}
  if(free_words<128){printf("B2METRIC ERROR stack_margin\r\n");return;}
 }
 printf("B2METRIC END session=%s\r\n",recorder_session_directory());
}
