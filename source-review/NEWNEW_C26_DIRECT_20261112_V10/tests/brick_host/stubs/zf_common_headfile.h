#ifndef HOST_COMMON_H
#define HOST_COMMON_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
typedef uint8_t uint8;typedef uint16_t uint16;typedef uint32_t uint32;
typedef int8_t int8;typedef int16_t int16;typedef int32_t int32;
#define MT9V03X_H 120
#define MT9V03X_W 188
void tft180_show_int(int x,int y,int n,int len);
void tft180_show_string(int x,int y,const char *s);
#endif
