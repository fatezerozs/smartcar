/* Actual phase metrics with mocked clock/IRQ/counters. No hardware IO. */
#define printf joint_test_printf
int joint_test_printf(const char *,...);
#include "../../demo/recorder_joint/joint_metrics.c"
#include <assert.h>
#include <stdarg.h>
uint32_t SystemCoreClock=600000000;
dwt_t mock_dwt;
debug_t mock_debug;
PWM_Type mock_pwm[5];
static uint32_t clock_ms,irq_mask;
static vd_speed_info_t sample;
static char output[8192];
static unsigned used;
int joint_test_printf(const char *format,...)
{va_list args;va_start(args,format);int n=vsnprintf(output+used,sizeof(output)-used,format,args);va_end(args);assert(n>=0&&(unsigned)n<sizeof(output)-used);used+=(unsigned)n;return n;}
uint32_t timer_get(int n){(void)n;return clock_ms;}
uint32_t __get_PRIMASK(void){return irq_mask;}
void __disable_irq(void){irq_mask=1;}
void __set_PRIMASK(uint32_t m){irq_mask=m;}
void vision_drive_speed_get(vd_speed_info_t *out){*out=sample;}
void recorder_stats(rec_stats_t *out){memset(out,0,sizeof(*out));}
int main(void)
{
 clock_ms=100;joint_phase(1);
 sample.samples=1;sample.sample_ms=100;b3_pit_sample();
 sample.samples=2;sample.sample_ms=110;b3_pit_sample();
 sample.samples=3;sample.sample_ms=130;b3_pit_sample();
 joint_control_begin(100);mock_dwt.CYCCNT+=60;joint_control_end(100);
 joint_control_begin(112);mock_dwt.CYCCNT+=120;joint_control_end(112);
 b3_pi_begin(112);b3_pi_end(0,7,&sample,1);
 sample.target_right=5;b3_pi_end(1,8,&sample,1);b3_pi_end(1,9,&sample,2);b3_pi_end(1,10,&sample,2);
 joint_queue_depth(5);joint_queue_depth(2);
 b2_io_end(0,mock_dwt.CYCCNT-30,1,0);b3_pwm_write(MOTOR1_PWM,10,1,0,0,0,0,0);
 clock_ms=150;joint_phase(2);
 assert(phases[1].pi_consumed==1&&phases[1].control_budget_miss==1);
 assert(phases[1].sample_count==3&&phases[1].sample_gap_min==10&&phases[1].sample_gap_max==20&&phases[1].sample_not_10ms==1);
 assert(phases[1].queue_max==5&&phases[1].queue_last==2&&phases[1].io_errors==1&&phases[1].pwm_nonzero==1);
 assert(phases[2].queue_max==2&&phases[2].control_calls==0&&phases[2].io_calls==0);
 joint_control_begin(150);joint_control_end(150);joint_queue_depth(0);
 clock_ms=160;joint_phase(0);joint_report(1);joint_report(2);
 assert(strstr(output,"service_budget_misses=1")&&strstr(output,"JOINT QUEUE phase=2 last=0 max=2"));
 assert(phases[2].control_gap_max==0&&phases[2].pwm_nonzero==0&&!irq_mask);
 mock_dwt.CYCCNT=0xfffffff0U;b2_runtime_init();mock_dwt.CYCCNT=0x248U;assert(b2_runtime_us()==1&&!irq_mask);
 puts("JOINT METRICS PASS: phase isolation, unique PI consumption, sample/service budget violations, queue drain, IO errors, nonzero-output detection and cycle wrap; synthetic only");return 0;
}
