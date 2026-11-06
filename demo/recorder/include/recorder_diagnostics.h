#ifndef RECORDER_DIAGNOSTICS_H
#define RECORDER_DIAGNOSTICS_H
#include <stdint.h>
/* Stage 10 UART, 20 SD pads, 30 camera init, 40 camera returned,
 * 50 kernel objects, 60 scheduler, 70 Control, 80 Link. */
extern volatile uint32_t recorder_boot_stage_id;
extern volatile uint8_t recorder_camera_ok;
void recorder_boot_stage(uint32_t stage);
void recorder_panic_report(const char *file,uint32_t line);
void recorder_assert_fail(const char *file,uint32_t line);
#endif
