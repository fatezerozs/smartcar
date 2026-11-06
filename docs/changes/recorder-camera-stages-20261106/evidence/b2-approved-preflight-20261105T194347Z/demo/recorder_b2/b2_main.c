/* Candidate only. No vehicle controller/encoder/actuator initialization. */
#include "main.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include "b2_policy.h"
volatile uint8_t recorder_stop_drive=1,recorder_stopped_drive=1;
const char *recorder_b2_stop_reason="not_started";
extern volatile uint8_t recorder_finish_requested;
static StaticTask_t tcbs[3],idle_tcb;
static StackType_t capture_stack[1024],storage_stack[4096],monitor_stack[1024],idle_stack[configMINIMAL_STACK_SIZE];
void vApplicationGetIdleTaskMemory(StaticTask_t **t,StackType_t **s,uint32_t *n){*t=&idle_tcb;*s=idle_stack;*n=configMINIMAL_STACK_SIZE;}
void recorder_panic(void){__disable_irq();for(;;){__NOP();}}
void recorder_assert_fail(const char *file,uint32_t line){(void)file;(void)line;recorder_panic();}
void __aeabi_assert(const char *expr,const char *file,int line){(void)expr;recorder_assert_fail(file,(uint32_t)line);for(;;){}}
void debug_assert_handler(uint8 pass,char *file,int line){if(!pass)recorder_assert_fail(file,(uint32_t)line);}
void debug_log_handler(uint8 pass,char *s,char *file,int line){(void)pass;(void)s;(void)file;(void)line;}
void vApplicationStackOverflowHook(TaskHandle_t t,char *name){(void)t;(void)name;recorder_panic();}
void recorder_vision_init(void){}
void recorder_publish_vision(const rec_frame_t *frame){(void)frame;}
int fputc(int c,FILE *f){(void)f;uart_write_byte(UART_1,(uint8)c);return c;}
void CSI_IRQHandler(void){recorder_capture_irq();}
void LPUART5_IRQHandler(void){if((LPUART_GetStatusFlags(LPUART5)&kLPUART_RxDataRegFullFlag)&&camera_uart_handler)camera_uart_handler();LPUART_ClearStatusFlags(LPUART5,kLPUART_RxOverrunFlag);}
void HardFault_Handler(void){recorder_panic();}
void MemManage_Handler(void){recorder_panic();}
void BusFault_Handler(void){recorder_panic();}
void UsageFault_Handler(void){recorder_panic();}
void USDHC1_IRQHandler(void){extern void USDHC1_DriverIRQHandler(void);USDHC1_DriverIRQHandler();}
static int command(b2_line_t *line)
{uint8 c;if(!uart_query_byte(UART_1,&c))return 0;return b2_line_feed(line,c);}
static void monitor(void *unused)
{
 b2_line_t line={0};uint32_t begin=0,record_begin=0,last_report=0;int begun=0,recording_started=0;(void)unused;
 for(;;){uint32_t now=timer_get(GPT_TIM_1);rec_stats_t s;recorder_stats(&s);
  if(!begun){begin=now;begun=1;}
  if(recorder_recording&&!recording_started){record_begin=now;recording_started=1;}
  if(command(&line)&&!strcmp(line.text,"B2 STOP")){recorder_b2_stop_reason="operator_record_stop";recorder_finish_requested=1;}
  if(!recorder_finish_requested&&b2_window_expired(now,begin,recording_started,record_begin,s.observed)){
   recorder_b2_stop_reason="bounded_static_capture_window";recorder_finish_requested=1;
  }
  if((uint32_t)(now-last_report)>=1000U){
   printf("[B2] state=%u error=%u written=%lu synced=%lu observed=%llu drop=%llu pool=%llu ambiguous=%llu queue=%lu dir=%s\r\n",recorder_storage_state,recorder_storage_error,(unsigned long)recorder_written,(unsigned long)recorder_synced,(unsigned long long)s.observed,(unsigned long long)s.queue_full,(unsigned long long)s.pool_full,(unsigned long long)s.ambiguous,(unsigned long)s.queue_high_water,recorder_session_directory());last_report=now;
  }
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
int main(void)
{
 b2_line_t line={0};
 clock_init(SYSTEM_CLOCK_600M);NVIC_SetPriority(LPUART5_IRQn,6);
 {extern const uint32_t __Vectors[];SCB->VTOR=(uint32_t)__Vectors;__DSB();__ISB();}
 timer_init(GPT_TIM_1,TIMER_MS);timer_start(GPT_TIM_1);
 uart_init(UART_1,115200,UART1_TX_B12,UART1_RX_B13);
 printf("RECORDER_CAMERA_B2_STATIC_1 ready; no actuators; wait for B2 START; B1 SKIPPED; hardware approval required\r\n");
 for(;;)if(command(&line)&&!strcmp(line.text,"B2 START"))break;
 recorder_b2_stop_reason="bounded_static_capture_window";
 recorder_sd_board_init();recorder_capture_prepare();
 CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
 if(mt9v03x_init()!=0){printf("[B2] camera init failed; no Session started\r\n");for(;;){}}
 recorder_runtime_init();
 configASSERT(xTaskCreateStatic(recorder_capture_task,"Capture",1024,NULL,4,capture_stack,&tcbs[0]));
 configASSERT(xTaskCreateStatic(recorder_storage_task,"Writer",4096,NULL,2,storage_stack,&tcbs[1]));
 configASSERT(xTaskCreateStatic(monitor,"Monitor",1024,NULL,5,monitor_stack,&tcbs[2]));
 vTaskStartScheduler();recorder_panic();return 0;
}
