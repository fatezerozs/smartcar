#ifndef JOINT_H
#define JOINT_H
#include <stdint.h>
#include "b3_trace.h"
extern volatile uint8_t joint_writer_start,joint_stop_requested;
void joint_control_task(void *);
void joint_control_prepare(void);
void joint_phase(unsigned);
void joint_report(unsigned);
void joint_control_begin(uint32_t);
void joint_control_end(uint32_t);
void joint_queue_depth(uint32_t);
#endif
