#include "main.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "vision_speed.h"
#include "wheel_speed.h"
#include "midterm_boot.h"
#include "board_io.h"
#include "vision_drive_internal.h"
#include "vision_field.h"
#include "board_encoder.h"
#include <stdlib.h>
#undef printf
static uint32_t now,pwm[3],mask;
static uint16_t raw3,raw4;static int dir[2];static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"speed:%d %s\n",__LINE__,#x);exit(1);}}while(0)
uint32_t timer_get(int n){(void)n;return now;}
uint32_t __get_PRIMASK(void){return mask;}
void __disable_irq(void){mask=1;}
void __set_PRIMASK(uint32_t n){mask=n;}
void __DMB(void){}
void pwm_set_duty(int c,uint32_t d){CHECK(c>=0&&c<3&&d<=10000);pwm[c]=d;}
void gpio_set_level(int pin,int v){CHECK(pin==MOTOR1_DIR||pin==MOTOR2_DIR);dir[pin==MOTOR1_DIR?0:1]=v;}
void pit_ms_init(int c,uint32_t ms){CHECK(c==1&&ms==10);}
void interrupt_set_priority(int n,uint8_t p){CHECK(n==7&&p==0);}
void board_encoder_init(void){}
void board_encoder_read(uint16_t *l,uint16_t *r){*l=raw3;*r=raw4;}
void board_encoder_capture(board_encoder_diag_t *out){memset(out,0,sizeof(*out));out->ctrl3=out->ctrl4=5U<<13;for(unsigned i=0;i<4;++i)out->daisy[i]=1;}

static vision_result_t lane;
static unsigned sequence;static int buzzer;
void board_buzzer_set(int on){buzzer=on;}
static void boot(uint32_t start)
{
    vd_profile_t p;
    now=start;raw3=raw4=0;mask=0;sequence=0;
    vision_drive_init(now);app_mode_init(1,1,now);
    (void)app_mode_take_change();
    CHECK(app_mode_allows(APP_MODE_DRIVE)); /* No switch/key needed. */
    CHECK(app_mode_current()==APP_MODE_DRIVE);
    vision_drive_get_profile(&p);
    p.servo_center_us=1550;p.servo_min_us=1500;p.servo_max_us=1600;p.right_sign=1;
    p.hardware_confirmed=p.motor_direction_confirmed=p.control.calibration_confirmed=1;
    p.control.cruise_duty=.1f;p.control.duty_cap=.15f;p.control.max_run_ms=5000;
    p.motor1_forward_level=p.motor2_forward_level=1;
    CHECK(vision_drive_configure(&p));
    memset(&lane,0,sizeof(lane));lane.valid=1;lane.status=VISION_TRACK;lane.confidence=90;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){lane.left[y]=64;lane.right[y]=123;lane.center[y]=94;lane.row_flags[y]=3;}
    vision_drive_set_local_allowed(1);midterm_boot_init(start,now);
}
static void step(int frames)
{
    now+=10;raw3=(uint16_t)(raw3+2);raw4=(uint16_t)(raw4+2);
    vision_drive_safety_tick();vision_drive_poll(now);
    if(frames && now%20U==0)vision_drive_observe(&lane,++sequence,now,now);
    midterm_boot_poll(now);
}
int main(void)
{
    boot(0);
    for(int i=0;i<299;++i){step(1);CHECK(!pwm[0]&&!pwm[1]);}
    CHECK(midterm_boot_state()==MT_BOOT_WAIT);
    step(1);
#if MIDTERM_VISUAL_REVIEWED && MIDTERM_SPEED_CONFIRMED
    CHECK(midterm_boot_state()==MT_BOOT_RUNNING && vision_drive_is_local());
    for(int i=0;i<80;++i)step(1); /* No UART heartbeat for >500ms. */
    CHECK(vision_drive_is_local()&&!vision_drive_output()->faults);
    CHECK(pwm[0]>0&&pwm[1]>0);
    CHECK(!vision_drive_command('a',now,1));
    midterm_boot_cancel(now);vision_drive_stop();CHECK(midterm_boot_state()==MT_BOOT_STOPPED);
    for(int i=0;i<80;++i)step(1);
    CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_is_local());
    boot(0);for(int i=0;i<310;++i)step(1);
    for(int i=0;i<11;++i)step(0); /* Frame freshness retained in PIT. */
    CHECK(!pwm[0]&&!pwm[1]);step(0);CHECK(midterm_boot_state()==MT_BOOT_STOPPED);
    boot(0);lane.valid=0;for(int i=0;i<310;++i)step(1);
    CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);
    lane.valid=1;for(int i=0;i<80;++i)step(1);
    CHECK(!pwm[0]&&!pwm[1]); /* Late valid frames never launch. */
    boot(0);for(int i=0;i<50;++i)step(1);vision_drive_stop();
    for(int i=0;i<310;++i)step(1);
    CHECK(midterm_boot_state()==MT_BOOT_STOPPED&&!pwm[0]&&!pwm[1]);
    boot(0);now=3000;midterm_boot_init(0,now);CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);
    for(int i=0;i<80;++i)step(1);CHECK(!pwm[0]&&!pwm[1]);
    boot(0);for(int i=0;i<299;++i)step(1);
    now+=30;midterm_boot_poll(now);CHECK(midterm_boot_state()==MT_BOOT_BLOCKED); /* Stale encoder sample. */
    boot(0);for(int i=0;i<310;++i)step(1);
    for(int i=0;i<5;++i){now+=10;vision_drive_safety_tick();}
    CHECK(!pwm[0]&&!pwm[1]);midterm_boot_poll(now);CHECK(midterm_boot_state()==MT_BOOT_STOPPED);
    boot(0);for(int i=0;i<810;++i)step(1);
    CHECK(midterm_boot_state()==MT_BOOT_STOPPED&&!pwm[0]&&!pwm[1]); /* Total runtime. */
    boot(0xFFFFFC18U); /* Startup subtraction across millisecond rollover. */
    for(int i=0;i<300;++i)step(1);CHECK(midterm_boot_state()==MT_BOOT_RUNNING);
    vision_drive_stop();
#else
    CHECK(midterm_boot_state()==MT_BOOT_BLOCKED);
    for(int i=0;i<100;++i)step(1);CHECK(!pwm[0]&&!pwm[1]);
#endif
    for(int i=0;i<70;++i)step(0);CHECK(!buzzer);
    printf("autonomous: %u checks passed\n",checks);return 0;
}
