#include "main.h"
#include "board_io.h"
#include "board_encoder.h"
#include "recorder_runtime.h"
#include "recorder_sd.h"
#include "app_link.h"
#include "recorder_diagnostics.h"
#include "task.h"
/* Separate training entry: no call to the bare-metal app_runtime_run. */
uint8 image_two_value[MT9V03X_H][MT9V03X_W];
uint8_t Left_Line[MT9V03X_H],Right_Line[MT9V03X_H],Mid_Line[MT9V03X_H];
static StaticTask_t tcbs[5],idle_tcb;
static StackType_t control_stack[2048],capture_stack[1024],vision_stack[4096],
    storage_stack[4096],link_stack[2048],idle_stack[configMINIMAL_STACK_SIZE];
void vApplicationGetIdleTaskMemory(StaticTask_t **t,StackType_t **s,uint32_t *n)
{*t=&idle_tcb;*s=idle_stack;*n=configMINIMAL_STACK_SIZE;}
void recorder_panic(void)
{
    __disable_irq();board_buzzer_set(0);pwm_set_duty(MOTOR1_PWM,0);pwm_set_duty(MOTOR2_PWM,0);
    pwm_set_duty(SERVO_MOTOR_PWM,0);__DSB();
    for(;;){volatile unsigned wait;recorder_panic_report(NULL,0);for(wait=0;wait<30000000U;++wait){}}
}
void recorder_assert_fail(const char *file,uint32_t line)
{
    __disable_irq();board_buzzer_set(0);pwm_set_duty(MOTOR1_PWM,0);pwm_set_duty(MOTOR2_PWM,0);
    pwm_set_duty(SERVO_MOTOR_PWM,0);__DSB();
    for(;;){volatile unsigned wait;recorder_panic_report(file,line);for(wait=0;wait<30000000U;++wait){}}
}
void vApplicationStackOverflowHook(TaskHandle_t task,char *name)
{(void)task;(void)name;recorder_panic();}
int main(void)
{
    clock_init(SYSTEM_CLOCK_600M);
    /* The RTOS port restores MSP and validates SVC/PendSV through VTOR.
     * Use the linked Flash vector table directly in this isolated target. */
    {extern const uint32_t __Vectors[];SCB->VTOR=(uint32_t)__Vectors;__DSB();__ISB();}
    pwm_init(MOTOR1_PWM,VISION_MOTOR_PWM_HZ,0);gpio_init(MOTOR1_DIR,GPO,GPIO_LOW,GPO_PUSH_PULL);
    pwm_init(MOTOR2_PWM,VISION_MOTOR_PWM_HZ,0);gpio_init(MOTOR2_DIR,GPO,GPIO_LOW,GPO_PUSH_PULL);
    pwm_init(SERVO_MOTOR_PWM,SERVO_MOTOR_FREQ,0);
    timer_init(GPT_TIM_1,TIMER_MS);timer_start(GPT_TIM_1);
    board_io_init();debug_init();app_link_init();
    recorder_boot_stage(10U);
    /* PLL/PFD and SD pads configured exactly once, BEFORE camera/motion. */
    recorder_sd_board_init();board_encoder_init();
    recorder_boot_stage(20U);
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    recorder_capture_prepare();
    printf("[BOOT] RECORDER_TRAIN_1 rev=5 training-only RTOS; motion_reviewed=%u qualify_ms=%u; SD FAT32 1-bit 3.3V; commands=@rec status/@rec stop/@export/!\r\n",MIDTERM_VISUAL_REVIEWED,(unsigned)MIDTERM_BOOT_DELAY_MS);
    /* Camera failures leave the normal frame/boot safety gate closed.
     * No blocking camera retry is performed after the control task starts. */
    recorder_boot_stage(30U);
    recorder_camera_ok=(uint8_t)(mt9v03x_init()==0);
    recorder_boot_stage(40U);
    /* FreeRTOS queue/task creation leaves BASEPRI masked until the first
     * task starts. Never create OS objects before camera/UART initialization. */
    recorder_runtime_init();recorder_boot_stage(50U);
    configASSERT(xTaskCreateStatic(recorder_control_task,"Control",2048,NULL,5,control_stack,&tcbs[0]));
    configASSERT(xTaskCreateStatic(recorder_capture_task,"Capture",1024,NULL,4,capture_stack,&tcbs[1]));
    configASSERT(xTaskCreateStatic(recorder_vision_task,"Vision",4096,NULL,3,vision_stack,&tcbs[2]));
    configASSERT(xTaskCreateStatic(recorder_storage_task,"Storage",4096,NULL,2,storage_stack,&tcbs[3]));
    configASSERT(xTaskCreateStatic(recorder_link_task,"Link",2048,NULL,1,link_stack,&tcbs[4]));
    recorder_boot_stage(60U);vTaskStartScheduler();recorder_panic();return 0;
}
