#include "FreeRTOS.h"
typedef struct {unsigned available;} StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *p);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *p);
BaseType_t xSemaphoreTake(SemaphoreHandle_t p,TickType_t n);
BaseType_t xSemaphoreGive(SemaphoreHandle_t p);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t p,BaseType_t *wake);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t p,TickType_t n);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t p);
void vSemaphoreDelete(SemaphoreHandle_t p);
