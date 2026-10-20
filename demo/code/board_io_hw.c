#include "main.h"
#include "board_io.h"

void board_io_hw_claim(void)
{
    /* The vendor gpio_init configures GPIO1/2, whereas fast_gpio_init selects
     * GPIO6/7 via GPR26/27. Explicitly claim only the board-owned signals. */
    IOMUXC_GPR->GPR26 &= ~(1UL<<11);
    IOMUXC_GPR->GPR27 &= ~((1UL<<26)|(1UL<<27));
    __DSB();
}
void board_io_hw_capture(board_io_state_t *s)
{
    /* Addresses from SDK IOMUXC_GPIO_AD_B0_11_GPIO1_IO11. Read-only diagnostics. */
    s->buzzer_mux=*(volatile const uint32_t *)0x401F80E8U;
    s->buzzer_pad=*(volatile const uint32_t *)0x401F82D8U;
    s->buzzer_direction=(GPIO1->GDIR>>11)&1U;
    s->buzzer_output=(GPIO1->DR>>11)&1U;
    s->buzzer_fast_bank=(IOMUXC_GPR->GPR26>>11)&1U;
}
