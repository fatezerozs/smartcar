#ifndef APP_MODE_H
#define APP_MODE_H
#include <stdint.h>
typedef enum { APP_MODE_OBSERVE=0, APP_MODE_SERVO=1,
               APP_MODE_MOTOR=2, APP_MODE_DRIVE=3 } app_mode_t;
/* Inputs are electrical levels: open=1, ON/ground=0. C26 is bit 0.
 * Any raw change immediately inhibits motion. Stable 30ms commits a mode;
 * callers must stop/reset foreground state on every transition. */
void app_mode_init(int c26_high, int c27_high, uint32_t now);
void app_mode_sample(int c26_high, int c27_high, uint32_t now);
app_mode_t app_mode_current(void);
int app_mode_allows(app_mode_t requested);
int app_mode_take_change(void);
const char *app_mode_name(app_mode_t mode);
#endif
