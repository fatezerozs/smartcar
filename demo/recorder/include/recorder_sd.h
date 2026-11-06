#ifndef RECORDER_SD_H
#define RECORDER_SD_H
#include <stdint.h>
#define RECORDER_SD_BUS_BITS 1U
#define RECORDER_SD_MAX_HZ 25000000U
typedef struct {
    uint32_t clock_hz,host_capability,init_status;
    uint8_t present,initialized,bus_bits,quarantined,mux_radio_tx,mux_radio_rx,mux_servo;
} rec_sd_status_t;
void recorder_sd_board_init(void);
void recorder_sd_pins_init(void);
void recorder_sd_pad_config(uint32_t frequency);
void recorder_sd_status(rec_sd_status_t *out);
void recorder_sd_operation_begin(uint32_t ms);
void recorder_sd_operation_end(void);
int recorder_sd_expired(void);
void recorder_sd_fault(void);
int recorder_sd_failed(void);
#endif
