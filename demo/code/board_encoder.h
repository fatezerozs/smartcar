#ifndef BOARD_ENCODER_H
#define BOARD_ENCODER_H
#include <stdint.h>
void board_encoder_init(void);
/* Raw signed 16-bit free-running samples, encoder3 left / encoder4 right. */
void board_encoder_read(uint16_t *left,uint16_t *right);
typedef struct {uint16_t ctrl3,ctrl4;uint32_t gpr6;uint8_t inputs,daisy[4];} board_encoder_diag_t;
void board_encoder_capture(board_encoder_diag_t *out);
#endif
