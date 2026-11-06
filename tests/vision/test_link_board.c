#include "main.h"
#include "app_link.h"
#include <assert.h>
#include <stdio.h>
LPUART_Type test_uart[2];
static unsigned rts,rx_count[2],rx_pos[2],sent[2],barriers,initialized,enabled;
static uint32_t rx_words[2][64];
void gpio_init(int pin,int direction,int level,int mode)
{assert(pin==D26 && direction==GPI && level==GPIO_HIGH && mode==GPI_PULL_UP);}
int gpio_get_level(int pin){assert(pin==D26);return (int)rts;}
void uart_init(uart_index_enum index,uint32_t baud,int tx,int rx)
{assert(index==UART_8 && baud==115200 && tx==UART8_TX_D16 && rx==UART8_RX_D17);++initialized;}
void uart_rx_interrupt(uart_index_enum index,uint32_t on)
{assert(index==UART_8 && on==1);++enabled;}
int uart_query_byte(uart_index_enum index,uint8_t *byte)
{
    (void)index;(void)byte;assert(!"uint8 reader loses error tags");return 0;
}
uint8_t LPUART_GetRxFifoCount(LPUART_Type *uart)
{unsigned ch=uart==LPUART1?0U:1U;return (uint8_t)(rx_count[ch]-rx_pos[ch]);}
uintptr_t LPUART_GetDataRegisterAddress(LPUART_Type *uart)
{unsigned ch=uart==LPUART1?0U:1U;assert(rx_pos[ch]<rx_count[ch]);return (uintptr_t)&rx_words[ch][rx_pos[ch]++];}
uint32_t LPUART_GetStatusFlags(LPUART_Type *uart){return uart->flags;}
void LPUART_ClearStatusFlags(LPUART_Type *uart,uint32_t flags){uart->flags&=~flags;}
void LPUART_WriteByte(LPUART_Type *uart,uint8_t byte)
{(void)byte;++sent[uart==LPUART1?0:1];}
void __DSB(void){++barriers;}
int main(void)
{
    uint8_t bytes[40];app_link_stats_t stats;
    app_link_init();assert(initialized==1 && enabled==1);
    for(unsigned ch=0;ch<2;++ch)for(unsigned i=0;i<64;++i)rx_words[ch][i]=i;
    rx_count[0]=40;rx_count[1]=5;
    test_uart[0].flags=kLPUART_RxOverrunFlag|kLPUART_TxDataRegEmptyFlag;
    app_link_board_irq(0);assert(rx_pos[0]==32 && app_link_take_error(0));
    assert(test_uart[0].flags==kLPUART_TxDataRegEmptyFlag);
    app_link_board_irq(0);app_link_board_irq(1);
    assert(app_link_read(0,bytes,40)==40 && bytes[39]==39);
    assert(app_link_read(1,bytes,40)==5 && bytes[4]==4 && barriers==3);
    test_uart[1].flags=kLPUART_TxDataRegEmptyFlag;rts=1;
    assert(!app_link_board_try_tx(1,'a') && sent[1]==0);
    assert(app_link_board_try_tx(0,'b') && sent[0]==1);
    rts=0;test_uart[1].flags=0;assert(!app_link_board_try_tx(1,'c'));
    test_uart[1].flags=kLPUART_TxDataRegEmptyFlag;
    assert(app_link_board_try_tx(1,'d') && sent[1]==1);
    app_link_stats(0,&stats);assert(stats.rx_errors==1 && stats.rx_bytes==40);
    rx_pos[1]=0;rx_count[1]=5;
    rx_words[1][0]=LPUART_DATA_FRETSC_MASK|'!';
    rx_words[1][1]=LPUART_DATA_PARITYE_MASK|'s';
    rx_words[1][2]=LPUART_DATA_NOISY_MASK|0;
    rx_words[1][3]='!';rx_words[1][4]='s';
    app_link_board_irq(1);
    assert(app_link_take_error(1));
    assert(app_link_read(1,bytes,40)==2 && bytes[0]=='!' && bytes[1]=='s');
    app_link_stats(1,&stats);
    assert(stats.rx_corrupt==3 && stats.rx_bytes==7 && stats.rx_errors==1);
    assert(stats.last_bad_word==LPUART_DATA_NOISY_MASK);
    assert(stats.last_uart_error==(kLPUART_FramingErrorFlag|kLPUART_ParityErrorFlag|kLPUART_NoiseErrorFlag));
    rx_pos[1]=0;rx_count[1]=1;rx_words[1][0]=LPUART_DATA_RXEMPT_MASK|'a';
    app_link_board_irq(1);assert(app_link_read(1,bytes,40)==0);
    puts("UART IRQ ADAPTER PASS: 32-word budget, per-byte frame/parity/noise rejection, clean STOP preserved, error counters, empty FIFO, TX-ready and RTS gates.");
    return 0;
}
