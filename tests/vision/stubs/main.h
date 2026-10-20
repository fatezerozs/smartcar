#ifndef HOST_MAIN_H
#define HOST_MAIN_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MT9V03X_W 188
#define MT9V03X_H 120
#define DEBUG_UART_USE_INTERRUPT 1
#define DEBUG_UART_BAUDRATE 115200
#define WIRELESS_UART_BUAD_RATE 115200
uint32_t wireless_uart_read_buffer(uint8_t *data, uint32_t capacity);
uint32_t wireless_uart_send_buffer(const uint8_t *data, uint32_t length);
#define B11 43
#define C12 76
#define C13 77
#define C14 78
#define C15 79
typedef int gpio_pin_enum;
#define C26 90
#define C27 91
#define GPO 1
#define GPI 0
#define GPIO_LOW 0
#define GPIO_HIGH 1
#define GPO_PUSH_PULL 1
#define GPI_PULL_UP 2
int mt9v03x_init(void);
void gpio_init(int pin,int direction,int level,int mode);
int gpio_get_level(int pin);
#define GPT_TIM_1 1
#define MOTOR1_PWM 0
#define MOTOR2_PWM 1
#define SERVO_MOTOR_PWM 2
#define MOTOR1_DIR 3
#define MOTOR2_DIR 4
#define MOTOR1_FORWARD_DIR_LEVEL 1
#define MOTOR2_FORWARD_DIR_LEVEL 1
#define PWM_DUTY_MAX 10000U
#define SERVO_MOTOR_FREQ 50U
#define PIT_CH1 1
#define PIT_IRQn 7
void pwm_set_duty(int channel, uint32_t duty);
void gpio_set_level(int pin, int level);
void pit_ms_init(int channel, uint32_t ms);
void interrupt_set_priority(int irq, uint8_t priority);
extern volatile uint8_t mt9v03x_finish_flag;
extern uint8_t (*mt9v03x_image)[MT9V03X_W];
extern uint8_t image_two_value[MT9V03X_H][MT9V03X_W];
extern uint8_t Left_Line[MT9V03X_H], Right_Line[MT9V03X_H], Mid_Line[MT9V03X_H];
uint32_t timer_get(int index);
uint32_t debug_read_ring_buffer(uint8_t *data, uint32_t capacity);
uint32_t debug_send_buffer(const uint8_t *data, uint32_t length);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void __DMB(void);
int vision_test_printf(const char *format, ...);
#define printf vision_test_printf
#endif
