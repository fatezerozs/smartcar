#ifndef BOARD_CAMERA_H
#define BOARD_CAMERA_H
#include <stdint.h>
/* Foreground only. Bench modes never touch/wait for a camera. Failed init
 * is retried at most once per second; no blocking retry loop. */
void board_camera_reset(void);
int board_camera_poll(int requested,uint32_t now);
#endif
