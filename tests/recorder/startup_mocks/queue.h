#ifndef RECORDER_STARTUP_TEST_QUEUE_H
#define RECORDER_STARTUP_TEST_QUEUE_H
#include "FreeRTOS.h"
#include <stddef.h>
#define pdPASS 1
typedef struct {uint8_t *bytes;size_t size;unsigned full;} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(UBaseType_t count,UBaseType_t size,uint8_t *bytes,StaticQueue_t *storage);
BaseType_t xQueueOverwrite(QueueHandle_t queue,const void *item);
BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t wait);
#endif
