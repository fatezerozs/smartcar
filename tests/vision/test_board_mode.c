#include "main.h"
#include "app_mode.h"
#include "board_io.h"
#include "board_camera.h"
#include "board_diagnostics.h"
#include <stdarg.h>
#include "vision_drive.h"
#include <stdlib.h>
#undef printf

static uint32_t now, mask, pwm[3];
static int claimed;
void board_io_hw_claim(void) {claimed=1;}
void board_io_hw_capture(board_io_state_t *s) {s->buzzer_mux=5;s->buzzer_direction=1;s->buzzer_output=0;s->buzzer_fast_bank=0;s->buzzer_pad=0;}
static char diagnostic[2048];
int app_console_printf(const char *format,...) {int n;va_list args;size_t used=strlen(diagnostic);va_start(args,format);n=vsnprintf(diagnostic+used,sizeof(diagnostic)-used,format,args);va_end(args);return n;}
static int pins[128], init_count, failures, checks, camera_attempts;
uint32_t (*mt9v03x_capture_clock_ms)(void);
int mt9v03x_init(void) {return ++camera_attempts<2;}
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr,"FAIL mode:%d %s\n",__LINE__,#x); } } while (0)
uint32_t timer_get(int c) { (void)c; return now; }
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask=1; }
void __set_PRIMASK(uint32_t m) { mask=m; }
void __DMB(void) { }
void gpio_init(int pin,int direction,int value,int mode)
{
    if (!init_count) CHECK(claimed && pin==B11 && direction==GPO && value==0 && mode==GPO_PUSH_PULL);
    else CHECK((pin==C26 || pin==C27) && direction==GPI && value==1 && mode==GPI_PULL_UP);
    ++init_count; pins[pin]=value;
}
int gpio_get_level(int pin) { return pins[pin]; }
void gpio_set_level(int pin,int value) { pins[pin]=value; }
void pwm_set_duty(int c,uint32_t duty) { CHECK(c>=0 && c<3 && duty<=10000);pwm[c]=duty; }
void pit_ms_init(int c,uint32_t ms) { CHECK(c==1 && ms==10); }
void interrupt_set_priority(int irq,uint8_t priority) { CHECK(irq==7 && priority==0); }
static void select_mode(unsigned mode)
{
    pins[C26]=(mode&1U)==0; pins[C27]=(mode&2U)==0;
    board_mode_sample(); now+=30; board_mode_sample();
    CHECK(app_mode_current()==(app_mode_t)mode && app_mode_allows((app_mode_t)mode));
    (void)app_mode_take_change();
}
static void ready_lane(vision_result_t *lane)
{
    unsigned y;memset(lane,0,sizeof(*lane));
    lane->valid=1;lane->status=VISION_TRACK;lane->confidence=90;
    for(y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y) {
        lane->left[y]=64;lane->right[y]=123;lane->center[y]=94;lane->row_flags[y]=3;
    }
}
int main(void)
{
    unsigned from,to,i; vd_profile_t profile;vision_result_t lane;
    board_camera_reset();
    CHECK(!board_camera_poll(0,0) && !camera_attempts);
    CHECK(!board_camera_poll(1,0) && camera_attempts==1);
    CHECK(!board_camera_poll(1,999) && camera_attempts==1);
    CHECK(!board_camera_poll(0,1000) && camera_attempts==1);
    CHECK(board_camera_poll(1,1000) && camera_attempts==2);
    CHECK(board_camera_poll(1,1001) && camera_attempts==2);
    board_io_init();CHECK(init_count==3 && pins[B11]==0);
    board_diagnostics_report();CHECK(strstr(diagnostic,"[BOARD_IO]") && strstr(diagnostic,"mux=5 dir=1 out=0 fast=0"));diagnostic[0]=0;
    board_buzzer_set(1);CHECK(pins[B11]==1);board_buzzer_set(0);CHECK(pins[B11]==0);
    now=30;board_mode_sample();CHECK(app_mode_allows(APP_MODE_OBSERVE));
    /* All 16 transitions, bounce, immediate inhibition and millis wrap. */
    for(from=0;from<4;++from)for(to=0;to<4;++to) {
        select_mode(from);now+=1;
        pins[C26]=(to&1U)==0;pins[C27]=(to&2U)==0;board_mode_sample();
        if(from!=to) {
            CHECK(!app_mode_allows((app_mode_t)from));
            now+=29;board_mode_sample();CHECK(!app_mode_allows((app_mode_t)to));
        }
        now+=1;board_mode_sample();CHECK(app_mode_allows((app_mode_t)to));
        for(i=0;i<4;++i)CHECK(app_mode_allows((app_mode_t)i)==(i==to));
    }
    select_mode(APP_MODE_DRIVE);
    pins[C26]=1;board_mode_sample();now+=20;pins[C26]=0;board_mode_sample();
    CHECK(!app_mode_allows(APP_MODE_DRIVE));
    now+=29;board_mode_sample();CHECK(!app_mode_allows(APP_MODE_DRIVE));
    now+=1;board_mode_sample();CHECK(app_mode_allows(APP_MODE_DRIVE));
    app_mode_init(0,1,UINT32_MAX-10U);app_mode_sample(0,1,18);CHECK(!app_mode_allows(APP_MODE_SERVO));
    app_mode_sample(0,1,19);CHECK(app_mode_allows(APP_MODE_SERVO));
    vision_drive_init(now);vision_drive_switch_sample(1);now+=30;vision_drive_poll(now);
    select_mode(APP_MODE_OBSERVE);
    CHECK(!vision_drive_command('a',now,1));
    CHECK(!vision_drive_servo_probe(1500,200,now));CHECK(!vision_drive_motor_probe(1,10,1,200,now));
    select_mode(APP_MODE_SERVO);
    CHECK(!vision_drive_command('a',now,1));CHECK(!vision_drive_motor_probe(1,10,1,200,now));
    CHECK(vision_drive_servo_probe(1500,200,now));CHECK(pwm[2]>0 && !pwm[0] && !pwm[1]);
    /* ISR cancels a pulse on RAW selector change, before the 30ms debounce. */
    pins[C26]=1;pins[C27]=0;board_mode_sample();vision_drive_safety_tick();
    CHECK(!pwm[0] && !pwm[1] && !pwm[2]);vision_drive_poll(now);
    now+=30;board_mode_sample();CHECK(app_mode_allows(APP_MODE_MOTOR));
    if(app_mode_take_change())vision_drive_stop();
    CHECK(!vision_drive_servo_probe(1500,200,now));CHECK(!vision_drive_command('a',now,1));
    CHECK(vision_drive_motor_probe(1,10,1,200,now));CHECK(pwm[0]>0 && !pwm[1] && !pwm[2]);
    pins[C26]=0;pins[C27]=0;board_mode_sample();vision_drive_safety_tick();
    CHECK(!pwm[0] && !pwm[1] && !pwm[2]);vision_drive_poll(now);
    now+=30;board_mode_sample();CHECK(app_mode_allows(APP_MODE_DRIVE));
    if(app_mode_take_change())vision_drive_stop();
    CHECK(!vision_drive_command('a',now,1)); /* Mode selection is not calibration. */
    CHECK(!vision_drive_motor_probe(1,10,1,200,now));CHECK(!vision_drive_servo_probe(1500,200,now));
    vision_drive_get_profile(&profile);
    profile.servo_center_us=1500;profile.servo_min_us=1200;profile.servo_max_us=1800;profile.right_sign=1;
    profile.hardware_confirmed=profile.motor_direction_confirmed=profile.control.calibration_confirmed=1;
    profile.control.cruise_duty=.1f;profile.control.duty_cap=.2f;
    CHECK(vision_drive_configure(&profile));ready_lane(&lane);
    now+=2300;
    for(i=1;i<=6;++i) {now+=20;vision_drive_observe(&lane,i,now,now);}
    CHECK(vision_drive_command('a',now,1));now+=20;vision_drive_observe(&lane,7,now,now);CHECK(pwm[0]>0);
    pins[C26]=1;pins[C27]=1;board_mode_sample();vision_drive_safety_tick();
    CHECK(!pwm[0] && !pwm[1] && !pwm[2]);vision_drive_poll(now);CHECK(vision_drive_output()->state==VC_DISARMED);
    select_mode(APP_MODE_DRIVE);CHECK(!pwm[0] && !pwm[1] && !pwm[2]); /* Never resume old run. */
    CHECK(mask==0);
    printf("BOARD + MODE: %d checks, %d failures (mock GPIO only)\n",checks,failures);
    return failures?1:0;
}
