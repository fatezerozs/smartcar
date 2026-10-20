#ifndef LINK_BOARD_TEST_MAIN_H
#define LINK_BOARD_TEST_MAIN_H
#include <stdint.h>
typedef struct {uint32_t flags;} LPUART_Type;
extern LPUART_Type test_uart[2];
#define LPUART1 (&test_uart[0])
#define LPUART8 (&test_uart[1])
typedef enum {UART_1=1,UART_8=8} uart_index_enum;
#define D26 122
#define GPI 0
#define GPIO_HIGH 1
#define GPI_PULL_UP 2
#define UART8_TX_D16 101
#define UART8_RX_D17 202
#define kLPUART_TxDataRegEmptyFlag 1U
#define kLPUART_RxOverrunFlag 2U
#define kLPUART_NoiseErrorFlag 4U
#define kLPUART_FramingErrorFlag 8U
#define kLPUART_ParityErrorFlag 16U
void gpio_init(int pin,int direction,int level,int mode);
int gpio_get_level(int pin);
void uart_init(uart_index_enum index,uint32_t baud,int tx,int rx);
void uart_rx_interrupt(uart_index_enum index,uint32_t enabled);
int uart_query_byte(uart_index_enum index,uint8_t *byte);
uint32_t LPUART_GetStatusFlags(LPUART_Type *uart);
void LPUART_ClearStatusFlags(LPUART_Type *uart,uint32_t flags);
void LPUART_WriteByte(LPUART_Type *uart,uint8_t byte);
void __DSB(void);
#endif
