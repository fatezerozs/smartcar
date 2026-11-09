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
    unsigned budget=32U;
    LPUART_Type *uart=channel==APP_LINK_USB?LPUART1:LPUART8;
    uint32_t errors=LPUART_GetStatusFlags(uart)&
        (kLPUART_RxOverrunFlag|kLPUART_NoiseErrorFlag|kLPUART_FramingErrorFlag|kLPUART_ParityErrorFlag);
    while(budget-- && LPUART_GetRxFifoCount(uart)) {
        /* Read DATA once with the NXP per-character error tags intact.
         * uart_query_byte / LPUART_ReadByte truncate these tags to uint8_t. */
        uint32_t word=*(volatile uint32_t *)(uintptr_t)LPUART_GetDataRegisterAddress(uart);
        if(word&LPUART_DATA_RXEMPT_MASK)break;
        if(word&(LPUART_DATA_FRETSC_MASK|LPUART_DATA_PARITYE_MASK|LPUART_DATA_NOISY_MASK)) {
            if(word&LPUART_DATA_FRETSC_MASK)errors|=kLPUART_FramingErrorFlag;
            if(word&LPUART_DATA_PARITYE_MASK)errors|=kLPUART_ParityErrorFlag;
            if(word&LPUART_DATA_NOISY_MASK)errors|=kLPUART_NoiseErrorFlag;
            app_link_reject_rx_isr(channel,word);
        } else app_link_rx_isr(channel,(uint8_t)word);
    }
    if(errors){app_link_uart_error_isr(channel,errors);LPUART_ClearStatusFlags(uart,errors);}
    __DSB();
}
