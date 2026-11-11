#ifndef MIDTERM_BOOT_H
#define MIDTERM_BOOT_H
#include <stdint.h>
typedef enum { MT_BOOT_OFF=0, MT_BOOT_WAIT, MT_BOOT_RUNNING,
    MT_BOOT_BLOCKED, MT_BOOT_STOPPED } mt_boot_state_t;
void midterm_boot_init(uint32_t boot_ms, uint32_t now);
void midterm_boot_poll(uint32_t now);
void midterm_boot_cancel(uint32_t now);
mt_boot_state_t midterm_boot_state(void);
#endif
