#include "main.h"
#include "joint.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "recorder_diagnostics.h"
#include "app_link.h"
#include "board_io.h"
#include "board_encoder.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "b2_policy.h"
#include "b2_export.h"
#include "joint_identity.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
volatile uint8_t joint_writer_start,recorder_camera_ok;
extern volatile uint8_t recorder_finish_requested;
volatile uint32_t recorder_boot_stage_id;
static StaticTask_t tcbs[5],idle_tcb;
static StackType_t control_stack[2048],capture_stack[1024],vision_stack[4096],writer_stack[4096],monitor_stack[2048],idle_stack[configMINIMAL_STACK_SIZE];
void vApplicationGetIdleTaskMemory(StaticTask_t **t,StackType_t **s,uint32_t *n){*t=&idle_tcb;*s=idle_stack;*n=configMINIMAL_STACK_SIZE;}
void recorder_panic(void){__disable_irq();pwm_set_duty(MOTOR1_PWM,0);pwm_set_duty(MOTOR2_PWM,0);pwm_set_duty(SERVO_MOTOR_PWM,0);for(;;){__NOP();}}
void recorder_assert_fail(const char *file,uint32_t line){(void)file;(void)line;recorder_panic();}
void vApplicationStackOverflowHook(TaskHandle_t t,char *name){(void)t;(void)name;recorder_panic();}
void recorder_boot_stage(uint32_t stage){recorder_boot_stage_id=stage;}
static void identity(void){printf("JOINT id=RECORDER_JOINT_STATIC_C26_1 outputs=forced_zero source=%s c26=%u camera=%u storage=%u frames_limit=100 record_soft_limit_ms=3000 cpr_x10_L/R=%u/%u sign_L/R=%d/%d repeat_run=reset_required\r\n",JOINT_INPUT_DIGEST,(unsigned)gpio_get_level(C26),recorder_camera_ok,recorder_storage_state,MT_ROUTE_ENCODER_LEFT_CPR_X10,MT_ROUTE_ENCODER_RIGHT_CPR_X10,MT_ROUTE_ENCODER_LEFT_FORWARD_SIGN,MT_ROUTE_ENCODER_RIGHT_FORWARD_SIGN);}
/* One monitor owns command parsing and all serial output. Neither Control nor
 * ISR enters libc or FatFs. STOP remains an original-controller stop request. */
static int command(b2_line_t *line)
{
 uint8_t data;unsigned budget=32;int ready=0;
 if(app_link_take_error(APP_LINK_USB)){line->used=0;line->discard=1;joint_stop_requested=1;}
 while(budget--&&app_link_read(APP_LINK_USB,&data,1)){if(data=='!'||data==3){joint_stop_requested=1;line->used=0;line->discard=1;}else if(b2_line_feed(line,data)){ready=1;break;}}
 app_link_poll();return ready;
}
static void monitor(void *unused)
{
 b2_line_t line={0};unsigned phase=0,started=0,c26_test=0;uint32_t phase_begin=0,writer_begin=0;(void)unused;
 for(;;){uint32_t now=timer_get(GPT_TIM_1);
  if(command(&line)){
   if(!strcmp(line.text,"JOINT STATUS"))identity();
   else if(!strcmp(line.text,"JOINT RUN")||!strcmp(line.text,"JOINT ARM")){
    if(started||!gpio_get_level(C26)||!app_mode_allows(APP_MODE_OBSERVE))printf("JOINT ERROR require_unused_run_C26_OFF\r\n");
    else{c26_test=!strcmp(line.text,"JOINT ARM");started=1;phase=1;phase_begin=now;joint_phase(1);printf("JOINT BEGIN phase=1 recorder=OFF duration_ms=5000\r\n");}
   }else if(!strcmp(line.text,"JOINT STOP")){
    joint_stop_requested=1;recorder_finish_requested=1;started=1;
    if(phase==1||phase==5||phase==0){joint_phase(0);phase=0;printf("JOINT ERROR operator_cancelled no_new_recording\r\n");}
   }
   else if(!strncmp(line.text,"B2 EXPORT ",10)){
    if(started&&!phase&&recorder_storage_state==4&&gpio_get_level(C26)&&app_mode_allows(APP_MODE_OBSERVE)&&recorder_stopped_drive)(void)b2_export_command(line.text);
    else printf("JOINT ERROR export_requires_closed_C26_OFF\r\n");
   }
  }
  if(phase==1&&(uint32_t)(now-phase_begin)>=5000U){
   joint_phase(c26_test?0:2);joint_report(1);
   if(c26_test){phase=5;phase_begin=now;printf("JOINT WAIT C26_ON max_ms=60000 outputs=forced_zero\r\n");}
   else{phase=2;writer_begin=now;joint_writer_start=1;printf("JOINT BEGIN phase=2 recorder=ON finite=1\r\n");}
  }
  if(phase==5&&!gpio_get_level(C26)&&app_mode_allows(APP_MODE_DRIVE)){joint_phase(2);phase=2;writer_begin=now;joint_writer_start=1;printf("JOINT BEGIN phase=2 recorder=ON C26_ON finite=1\r\n");}
  if((phase==2||phase==3)&&(uint32_t)(now-writer_begin)>=3000U)recorder_finish_requested=1;
  if(phase==2&&recorder_storage_state==3){joint_phase(3);joint_report(2);phase=3;}
  if((phase==2||phase==3)&&(recorder_storage_state==4||recorder_storage_state==5)){
   unsigned previous=phase;joint_phase(c26_test?0:4);joint_report(previous);phase=c26_test?6:4;phase_begin=now;
   printf("JOINT CLOSED storage=%u error=%u dir=%s\r\n",recorder_storage_state,recorder_storage_error,recorder_session_directory());
   if(c26_test)printf("JOINT WAIT C26_OFF max_ms=60000 outputs=forced_zero\r\n");
  }
  if(phase==6&&gpio_get_level(C26)&&app_mode_allows(APP_MODE_OBSERVE)){joint_phase(4);phase=4;phase_begin=now;printf("JOINT BEGIN phase=4 recorder=OFF C26_OFF duration_ms=5000\r\n");}
  if((phase==5||phase==6)&&(uint32_t)(now-phase_begin)>=60000U){phase=0;printf("JOINT ERROR switch_wait_timeout no_complete_comparison\r\n");}
  if(phase==4&&(uint32_t)(now-phase_begin)>=5000U){
   joint_phase(0);joint_report(4);phase=0;
   {vd_last_stop_t stop;vision_drive_last_stop(&stop);printf("JOINT LAST_STOP ms=%lu reason=%u faults=%lu speed_detail=%u\r\n",(unsigned long)stop.ms,stop.reason,(unsigned long)stop.faults,stop.speed_detail);}
   printf("JOINT END state=%u error=%u written=%lu synced=%lu dir=%s source=%s\r\n",recorder_storage_state,recorder_storage_error,(unsigned long)recorder_written,(unsigned long)recorder_synced,recorder_session_directory(),JOINT_INPUT_DIGEST);
   {TaskStatus_t tasks[6];UBaseType_t n=uxTaskGetSystemState(tasks,6,NULL);for(UBaseType_t i=0;i<n;++i)printf("JOINT STACK task=%s free_words=%lu\r\n",tasks[i].pcTaskName,(unsigned long)tasks[i].usStackHighWaterMark);}
  }
  vTaskDelay(1);
 }
}
int main(void)
{
 b2_line_t line={0};clock_init(SYSTEM_CLOCK_600M);
 {extern const uint32_t __Vectors[];SCB->VTOR=(uint32_t)__Vectors;__DSB();__ISB();}
 pwm_init(MOTOR1_PWM,VISION_MOTOR_PWM_HZ,0);gpio_init(MOTOR1_DIR,GPO,GPIO_LOW,GPO_PUSH_PULL);
 pwm_init(MOTOR2_PWM,VISION_MOTOR_PWM_HZ,0);gpio_init(MOTOR2_DIR,GPO,GPIO_LOW,GPO_PUSH_PULL);pwm_init(SERVO_MOTOR_PWM,SERVO_MOTOR_FREQ,0);
 timer_init(GPT_TIM_1,TIMER_MS);timer_start(GPT_TIM_1);board_io_init();debug_init();app_link_init();
 identity();printf("JOINT wait=START C26_OFF motors_servo_power_isolated\r\n");
 for(;;)if(command(&line)){
  if(!strcmp(line.text,"JOINT STATUS"))identity();
  if(!strcmp(line.text,"JOINT START")){if(gpio_get_level(C26))break;printf("JOINT ERROR C26_OFF_required\r\n");}
 }
 /* All clock/pin changes precede camera and the original safety controller. */
 recorder_sd_board_init();board_encoder_init();recorder_capture_prepare();
 CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
 recorder_camera_ok=(uint8_t)(mt9v03x_init()==0);if(!recorder_camera_ok){printf("JOINT ERROR camera_init\r\n");recorder_panic();}
 recorder_runtime_init();joint_control_prepare();
 configASSERT(xTaskCreateStatic(joint_control_task,"Control",2048,NULL,5,control_stack,&tcbs[0]));
 configASSERT(xTaskCreateStatic(recorder_capture_task,"Capture",1024,NULL,4,capture_stack,&tcbs[1]));
 configASSERT(xTaskCreateStatic(recorder_vision_task,"Vision",4096,NULL,3,vision_stack,&tcbs[2]));
 configASSERT(xTaskCreateStatic(recorder_storage_task,"Writer",4096,NULL,2,writer_stack,&tcbs[3]));
 configASSERT(xTaskCreateStatic(monitor,"Monitor",2048,NULL,1,monitor_stack,&tcbs[4]));
 vTaskStartScheduler();recorder_panic();return 0;
}
