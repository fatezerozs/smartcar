#ifndef BOARD_IO_H
#define BOARD_IO_H
#include <stdint.h>
typedef struct {
    uint8_t dip26, dip27, buzzer_level;
    uint32_t buzzer_mux, buzzer_pad, buzzer_direction, buzzer_output, buzzer_fast_bank;
} board_io_state_t;
void board_io_init(void);
void board_mode_sample(void);
void board_buzzer_set(int on);
void board_io_read(board_io_state_t *state);
/* Hardware-only adapter; host tests provide a register model. */
void board_io_hw_claim(void);
void board_io_hw_capture(board_io_state_t *state);
#endif
