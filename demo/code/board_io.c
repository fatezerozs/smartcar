#include "main.h"
#include "board_io.h"
#include "app_mode.h"
#define BOARD_BUZZER_PIN B11
#define BOARD_MODE_BIT0 C26
#define BOARD_MODE_BIT1 C27
void board_buzzer_set(int on)
{gpio_set_level(BOARD_BUZZER_PIN,on?GPIO_HIGH:GPIO_LOW);}
void board_io_init(void)
{
    board_io_hw_claim();
    gpio_init(BOARD_BUZZER_PIN,GPO,GPIO_LOW,GPO_PUSH_PULL);
    gpio_init(BOARD_MODE_BIT0,GPI,GPIO_HIGH,GPI_PULL_UP);
    gpio_init(BOARD_MODE_BIT1,GPI,GPIO_HIGH,GPI_PULL_UP);
    app_mode_init(gpio_get_level(BOARD_MODE_BIT0),gpio_get_level(BOARD_MODE_BIT1),timer_get(GPT_TIM_1));
}
void board_mode_sample(void)
{app_mode_sample(gpio_get_level(BOARD_MODE_BIT0),gpio_get_level(BOARD_MODE_BIT1),timer_get(GPT_TIM_1));}
void board_io_read(board_io_state_t *s)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    s->dip26=(uint8_t)gpio_get_level(BOARD_MODE_BIT0);
    s->dip27=(uint8_t)gpio_get_level(BOARD_MODE_BIT1);
    s->buzzer_level=(uint8_t)gpio_get_level(BOARD_BUZZER_PIN);
    board_io_hw_capture(s);__set_PRIMASK(mask);
}
