#ifndef APP_MODE_H
#define APP_MODE_H
#include <stdint.h>
typedef enum { APP_MODE_OBSERVE=0, APP_MODE_SERVO=1,
               APP_MODE_MOTOR=2, APP_MODE_DRIVE=3 } app_mode_t;
/* Legacy selector builds: input levels are open=1, ON/ground=0;
 * C26 is bit 0. A raw change inhibits drive, and 30ms stability commits
 * a new mode. In C31 auto-track these pins are not sampled and the input
 * selector has no start, stop, or fault-clear authority. */
void app_mode_init(int c26_high, int c27_high, uint32_t now);
void app_mode_sample(int c26_high, int c27_high, uint32_t now);
app_mode_t app_mode_current(void);
int app_mode_allows(app_mode_t requested);
int app_mode_take_change(void);
int app_mode_change_pending(void);
const char *app_mode_name(app_mode_t mode);
#endif
