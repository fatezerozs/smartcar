#ifndef TURN_CONFIG_H
#define TURN_CONFIG_H
#include <stdint.h>
typedef struct {unsigned request, duration, speed, minimum_us, maximum_us;} turn_config_t;
extern turn_config_t turn_config;
extern volatile unsigned turn_effort_cap;
void turn_governor_sample(unsigned peak,unsigned requested);
void turn_config_capture(void);
int turn_config_set(unsigned request,unsigned duration,unsigned speed,unsigned minimum,unsigned maximum);
void turn_config_format(char *text,unsigned capacity,int frozen);
#endif
