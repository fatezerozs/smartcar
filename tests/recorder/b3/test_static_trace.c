#include "main.h"
#include "vision_drive.h"
#include "vision_speed.h"
#include "b3_trace.h"
#include "recorder_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
PWM_Type mock_pwm[5];dwt_t mock_dwt;debug_t mock_debug;uint32_t SystemCoreClock=600000000;
static uint32_t ms,mask,mode;static vc_output_t output;static vd_speed_info_t speed;
static uint8_t bytes[1024*1024];static size_t used;
uint32_t __get_PRIMASK(void){return mask;}
void __disable_irq(void){mask=1;}
void __set_PRIMASK(uint32_t m){mask=m;}
uint32_t timer_get(int n){(void)n;return ms;}
int gpio_get_level(int n){(void)n;return mode==0;}
void LPUART_WriteBlocking(void *p,const uint8_t *b,unsigned long long n){(void)p;assert(used+n+1<sizeof bytes);memcpy(bytes+used,b,(size_t)n);used+=(size_t)n;bytes[used]=0;ms+=10;}
const vc_output_t *vision_drive_output(void){return &output;}
void vision_drive_speed_get(vd_speed_info_t *s){*s=speed;}
unsigned app_mode_current(void){return mode;}
int app_mode_allows(unsigned m){return mode==m;}
int app_mode_change_pending(void){return 0;}
static void command(const char *s){uint8_t b[100];size_t n=strlen(s);assert(n<sizeof b);memcpy(b,s,n);assert(b3_filter(b,(uint32_t)n)==0);b3_poll();}
int main(void){unsigned i;uint8_t stop[3]={'!','\r','\n'};char *header,*end;unsigned count=0,length=0,run=0;unsigned long crc=0;
 for(i=0;i<256;i++){assert(b3_static_duty(MOTOR1_PWM,i*12345,MOTOR1_PWM,MOTOR2_PWM,SERVO_MOTOR_PWM)==0);assert(b3_static_duty(MOTOR2_PWM,0xffffffffU,MOTOR1_PWM,MOTOR2_PWM,SERVO_MOTOR_PWM)==0);assert(b3_static_duty(SERVO_MOTOR_PWM,10000,MOTOR1_PWM,MOTOR2_PWM,SERVO_MOTOR_PWM)==0);assert(b3_static_duty(999,i,MOTOR1_PWM,MOTOR2_PWM,SERVO_MOTOR_PWM)==i);}
 assert(b3_filter(stop,3)==1&&stop[0]=='!');b3_init();
 b3_pwm_write(MOTOR1_PWM,0,0,0,0,0,0,0);b3_pwm_write(MOTOR2_PWM,0,0,0,0,0,0,0);b3_pwm_write(SERVO_MOTOR_PWM,0,0,0,0,0,0,0);
 command("a\r\n");command("@b3 capture\r\n");command("@b3 status\r\n");assert(strstr((char*)bytes,"active=1 frozen=0"));
 for(i=0;i<100;i++){ms+=10;speed.samples++;speed.sample_ms=ms;b3_encoder_read((uint16_t)i,(uint16_t)(0U-i));b3_pit_sample();b3_pi_begin(ms);mock_dwt.CYCCNT+=600;b3_pi_end(0,300,&speed,0);}
 {vd_speed_info_t locked=speed;locked.samples=999;locked.target_right=10;speed.samples=1000;speed.target_right=20;b3_pi_begin(ms);b3_pi_end(1,300,&locked,999);}
 command("@b3 capture\r\n");assert(strstr((char*)bytes,"busy_or_unexported"));
 ms+=6000;b3_poll();assert(strstr((char*)bytes,"reason=1"));mode=3;command("@b3 dump\r\n");assert(strstr((char*)bytes,"require_frozen_C26_OFF_stopped"));
 mode=0;used=0;command("@b3 dump\r\n");header=(char*)bytes;end=strstr(header,"\r\n");assert(end);assert(sscanf(header,"B3TRACE DATA format=B3T1 run=%u count=%u bytes=%u crc32=%lx",&run,&count,&length,&crc)==4);assert(count==305&&length==count*64);assert(rec_crc_update(0,end+2,length)==(uint32_t)crc);assert(strstr(header,"synthetic=1"));assert(!memcmp(end+2+length,"\r\nB3TRACE END\r\n",15));
 {b3_event_t last;memcpy(&last,end+2+(count-1)*64,64);assert(last.kind==B3_PI_WORK&&last.v[0]==999&&last.v[2]==10&&last.v[11]==999);}
 used=0;command("@b3 capture\r\n");for(i=0;i<3100;i++)b3_encoder_read(1,2);b3_poll();assert(strstr((char*)bytes,"lost=1 reason=2"));command("@b3 dump\r\n");assert(strstr((char*)bytes,"count=3072 bytes=196608"));assert(!mask);
 used=0;command("@b3 arm\r\n");command("@b3 status\r\n");assert(strstr((char*)bytes,"waiting=1"));ms+=31000;b3_poll();assert(strstr((char*)bytes,"count=0 lost=0 reason=4"));command("@b3 clear_empty\r\n");
 used=0;command("@b3 arm\r\n");b3_mode(0,1,3,0,0,2);command("@b3 status\r\n");assert(strstr((char*)bytes,"active=1 frozen=0 waiting=0"));ms+=6000;b3_poll();assert(strstr((char*)bytes,"reason=1"));
 puts("PASS actual B3 static trace: forced-zero duties, STOP filter, delayed-ACK start, bounded freeze, unexported protection, OFF dump gate, binary CRC, full-buffer freeze");return 0;
}
