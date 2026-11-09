#ifndef SMARTCAR_VISION_PLANNER_SHADOW_DIAG_H
#define SMARTCAR_VISION_PLANNER_SHADOW_DIAG_H
#include "vision_planner_runtime.h"
/* Explicit candidate-only wrapper. No PWM, PI or drive admission operations. */
void vd_shadow_diag_init(void);
void vd_shadow_diag_poll(uint32_t now);
void vd_shadow_diag_observe(const vision_result_t *road,uint32_t sequence,uint32_t capture,uint32_t now);
int vd_shadow_diag_field(const char *command,char *response,unsigned capacity);
#endif
