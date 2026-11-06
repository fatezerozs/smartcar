#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H
#include <assert.h>
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(n) (n)
#define configASSERT(n) assert(n)
#endif
