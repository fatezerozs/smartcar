#ifndef RING_TRANSITION_H
#define RING_TRANSITION_H
#include <stdint.h>
#include "lane_geometry.h"
/* State topology and entry paths remain donor-owned. Only current measured
 * road targets at tentative approach and qualified exit can replace bias. */
typedef struct {
 unsigned exit_pairs,recover_frames;int exit_current;
 float speed;uint32_t speed_ms;int speed_initialized,speed_recovery;
} ring_transition_t;
typedef struct {float error;unsigned source;} ring_target_t;
static inline float ring_exit_single_error(const lane_geometry_t *g)
{
 /* Far single-edge rows may still follow the island arc. Use the current
  * exit throat as primary reference, with at most 8px heading correction. */
 float heading=.25f*(g->error-g->near_error);
 if(heading>8)heading=8;if(heading<-8)heading=-8;
  /* Do not let a far remnant arc add a turn to a centered exit throat. */
 {float blend=(lg_abs(g->near_error)-3.0f)/9.0f;if(blend<0)blend=0;if(blend>1)blend=1;heading*=blend;}
 return g->near_error+heading;
}
static inline ring_target_t ring_transition_target(ring_transition_t *s,
 int before,int after,int side,int avoiding,int pair_valid,float pair_error,
 int left_seen,int right_seen,const lane_geometry_t *g,float legacy)
{
 ring_target_t out={legacy,0};
 if(after==NO_ROUND&&(before==OUT_WAY||before==OUT_ROUND))s->recover_frames=16;
 else if(after!=NO_ROUND)s->recover_frames=0;
 if(after<GET_OUT_ROUND||after>OUT_ROUND){s->exit_pairs=0;s->exit_current=0;}
 if(avoiding){s->recover_frames=0;return out;}
 if(after==NO_ROUND&&s->recover_frames){
  s->recover_frames--;
  if(g->valid){out.error=pair_valid?pair_error:ring_exit_single_error(g);out.source=8;return out;}
 }
 /* Current approach centering stops at committed GET_IN_ROUND. */
 if((after==ROUND_READY||after==GET_IN_READY)&&
    (before==NO_ROUND||before==ROUND_READY||before==GET_IN_READY)&&pair_valid&&g->valid){
  out.error=pair_error;out.source=3;return out;
 }
 if(after<GET_OUT_ROUND||after>OUT_ROUND)return out;
 if(before<IN_ROUND||before>OUT_ROUND){s->exit_pairs=0;s->exit_current=0;return out;}
 if(pair_valid&&g->valid){
  if(s->exit_pairs<2)s->exit_pairs++;
  if(s->exit_pairs>=2){s->exit_current=1;out.error=pair_error;out.source=4;}
  else {out.error=ring_exit_single_error(g);out.source=9;}
 }else{
  s->exit_pairs=0;
  /* The donor FSM already qualifies the exit. A current outer edge and
   * learned width can bridge missing pairs from phase 4 onward. */
  if(g->valid&&g->model_used&&
    ((side==ROUND_L&&right_seen)||(side==ROUND_R&&left_seen))){
   s->exit_current=1;out.error=ring_exit_single_error(g);out.source=5;
  }
 }
 return out;
}
static inline float route_cruise_speed(float requested,int stage,float servo)
{
 float t=lg_abs(servo-1550.0f),cap;
 if(stage!=NO_ROUND)return requested>1.0f?1.0f:requested;
 t=(t-25.0f)/55.0f;if(t<0)t=0;if(t>1)t=1;
 cap=requested-(requested>.9f?(requested-.9f)*t:0);
 return cap;
}
static inline float ring_transition_raw_speed(float requested,const lane_geometry_t *g,int stage)
{
 if(stage!=NO_ROUND&&requested>1.0f)requested=1.0f;
 float raw=edge_speed_limit(requested,g);
 /* Junction expansion is not a calibrated body clearance. Keep ordinary
  * edge deceleration, but avoid creeping at tentative approach alone. */
 if(stage==ROUND_READY||stage==GET_IN_READY){float floor=requested<.85f?requested:.85f;if(raw<floor)raw=floor;}
 return raw;
}
static inline float ring_transition_speed(ring_transition_t *s,float requested,
 const lane_geometry_t *g,int stage,uint32_t now)
{
 float raw=ring_transition_raw_speed(requested,g,stage),delta,dt;
 float fall=(stage==ROUND_READY||stage==GET_IN_READY)?.8f:2.5f;
 if(requested<=0){s->speed=0;s->speed_initialized=0;s->speed_recovery=0;s->speed_ms=now;return 0;}
 if(s->recover_frames&&stage==NO_ROUND)s->speed_recovery=1;
 if(stage!=NO_ROUND)s->speed_recovery=0;
 if(stage>=GET_IN_ROUND&&stage<=OUT_ROUND&&s->speed>1.0f)s->speed=1.0f;
 if(!s->speed_initialized||(stage==NO_ROUND&&!s->speed_recovery)){s->speed=raw;s->speed_ms=now;s->speed_initialized=1;return raw;}
 dt=(float)(uint32_t)(now-s->speed_ms)*.001f;if(dt>.1f)dt=.1f;s->speed_ms=now;
 delta=raw-s->speed;if(delta>.6f*dt)delta=.6f*dt;if(delta<-fall*dt)delta=-fall*dt;
 s->speed+=delta;if(lg_abs(s->speed-raw)<.001f)s->speed_recovery=0;return s->speed;
}
#endif
