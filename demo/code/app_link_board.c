#include "main.h"
#include "app_link.h"
void app_link_board_init(void)
{
    /* Module is supplied with 5V, UART/RTS logic is 3.3V. CMD remains pulled up. */
    gpio_init(D26,GPI,GPIO_HIGH,GPI_PULL_UP);
    uart_init(UART_8,115200,UART8_TX_D16,UART8_RX_D17);
    uart_rx_interrupt(UART_8,1);
}
int app_link_board_try_tx(unsigned channel,uint8_t byte)
{
    LPUART_Type *uart=channel==APP_LINK_USB?LPUART1:LPUART8;
    if(channel==APP_LINK_RADIO && gpio_get_level(D26))return 0;
    if(!(LPUART_GetStatusFlags(uart)&kLPUART_TxDataRegEmptyFlag))return 0;
    LPUART_WriteByte(uart,byte);return 1;
}
void app_link_board_irq(unsigned channel)
{
    uint8_t byte;unsigned budget=32U;
    uart_index_enum index=channel==APP_LINK_USB?UART_1:UART_8;
    LPUART_Type *uart=channel==APP_LINK_USB?LPUART1:LPUART8;
    uint32_t errors=LPUART_GetStatusFlags(uart)&
        (kLPUART_RxOverrunFlag|kLPUART_NoiseErrorFlag|kLPUART_FramingErrorFlag|kLPUART_ParityErrorFlag);
    if(errors){app_link_error_isr(channel);LPUART_ClearStatusFlags(uart,errors);}
    while(budget-- && uart_query_byte(index,&byte))app_link_rx_isr(channel,byte);
    __DSB();
}
