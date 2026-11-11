#ifndef APP_LINK_H
#define APP_LINK_H
#include <stdint.h>
#define APP_LINK_USB 0U
#define APP_LINK_RADIO 1U
#define APP_LINK_COUNT 2U
#define APP_LINK_RX_SIZE 512U
#define APP_LINK_TX_SIZE 8192U
typedef struct {
    uint32_t rx_bytes, tx_bytes, rx_overflow, rx_errors, tx_rejected;
} app_link_stats_t;
/* Foreground owns TX; each UART ISR is the sole producer of its RX ring. */
void app_link_init(void);
void app_link_rx_isr(unsigned channel, uint8_t byte);
void app_link_error_isr(unsigned channel);
uint32_t app_link_read(unsigned channel, uint8_t *data, uint32_t capacity);
int app_link_take_error(unsigned channel);
int app_link_send(unsigned channel, const uint8_t *data, uint32_t length);
int app_link_frame(unsigned channel, const uint8_t *header, uint32_t header_length,
                   const uint8_t *pixels, uint32_t pixel_length);
void app_link_poll(void);
void app_link_stats(unsigned channel, app_link_stats_t *stats);
void app_link_mvp_run(void);
/* Platform contract: try_tx must return immediately, including RTS busy. */
void app_link_board_init(void);
int app_link_board_try_tx(unsigned channel, uint8_t byte);
void app_link_board_irq(unsigned channel);
#endif
