#ifndef SMARTCAR_SPEED_IO_H
#define SMARTCAR_SPEED_IO_H
#include "main.h"
#include "app_mode.h"
#include "vision_drive.h"
#include "vision_speed.h"
#include "wheel_speed.h"
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

#endif
