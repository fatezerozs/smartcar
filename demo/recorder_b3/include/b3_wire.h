#ifndef B3_WIRE_H
#define B3_WIRE_H
#include <stdint.h>
#define B3_CAPACITY 3072U
#define B3_WINDOW_MS 5000U
enum {B3_RAW=1,B3_SAMPLE,B3_PI,B3_COMMIT,B3_MODE,B3_VISION,B3_STATE,B3_PWM_SNAPSHOT,B3_PI_WORK};
typedef struct {uint32_t ordinal,kind,ms,cycles;int32_t v[12];} b3_event_t;
typedef char b3_event_must_be_64_bytes[(sizeof(b3_event_t)==64)?1:-1];
static inline uint32_t b3_static_duty(uint32_t pin,uint32_t request,uint32_t m1,uint32_t m2,uint32_t servo)
{return pin==m1||pin==m2||pin==servo?0U:request;}
#endif
