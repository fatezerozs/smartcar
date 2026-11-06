#include <stdint.h>
#define MOTOR1_PWM 85U
#define MOTOR2_PWM 95U
#define SERVO_MOTOR_PWM 185U
#define PWM_DUTY_MAX 10000U
#define GPT_TIM_1 1
#define C26 26
typedef struct {uint16_t VAL2,VAL3,VAL4,VAL5;} sm_t;
typedef struct {sm_t SM[4];uint16_t MCTRL;} PWM_Type;
extern PWM_Type mock_pwm[5];
#define PWM_BASE_PTRS {&mock_pwm[0],&mock_pwm[1],&mock_pwm[2],&mock_pwm[3],&mock_pwm[4]}
typedef struct {uint32_t CYCCNT,CTRL;} dwt_t;
typedef struct {uint32_t DEMCR;} debug_t;
extern dwt_t mock_dwt;
extern debug_t mock_debug;
#define DWT (&mock_dwt)
#define CoreDebug (&mock_debug)
#define CoreDebug_DEMCR_TRCENA_Msk 1
#define DWT_CTRL_CYCCNTENA_Msk 1
#define LPUART1 ((void*)0)
extern uint32_t SystemCoreClock;
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
#define __DMB() ((void)0)
uint32_t timer_get(int);
int gpio_get_level(int);
void LPUART_WriteBlocking(void*,const uint8_t*,unsigned long long);
