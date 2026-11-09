#include "speed_planner.h"
#include <float.h>
#include <math.h>
#include <string.h>
static int finite_f(float x){return x==x&&x>=-FLT_MAX&&x<=FLT_MAX;}
static float abs_f(float x){return x<0?-x:x;}
static float min_f(float a,float b){return a<b?a:b;}
static float clamp_f(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static int forward(uint32_t a,uint32_t b){uint32_t d=a-b;return d&&d<UINT32_C(0x80000000);}
static int fresh(uint32_t now,uint32_t capture,uint32_t age){return age&&age<UINT32_C(0x80000000)&&(uint32_t)(now-capture)<age;}
static float median(float a,float b,float c){return a>b?(b>c?b:(a>c?c:a)):(a>c?a:(b>c?c:b));}
int sp_geometry(sp_geometry_history_t *h,const sp_point_t *p,unsigned n,sp_units_t units,
                uint32_t seq,uint32_t capture,uint32_t now,uint32_t age,float max_step,sp_preview_t *out)
{
    sp_point_t filtered[SP_CAPACITY];sp_preview_t candidate;unsigned i;
    if(!h||!p||!out||n<5||n>SP_CAPACITY||(units!=SP_NORMALIZED&&units!=SP_METRES)||
       !finite_f(max_step)||max_step<=0||!fresh(now,capture,age))return 0;
    if(h->have_frame&&(!forward(seq,h->sequence)||!forward(capture,h->capture_ms)))return 0;
    if(h->have_frame&&h->units!=units)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.count=n;candidate.units=units;
    candidate.sequence=seq;candidate.capture_ms=capture;
    for(i=0;i<n;++i){
        if(!finite_f(p[i].x)||!finite_f(p[i].y)||abs_f(p[i].x)>10000||abs_f(p[i].y)>10000||
           (i&&p[i].y<=p[i-1].y))return 0;
        filtered[i]=p[i];
    }
    for(i=1;i+1<n;++i)filtered[i].x=median(p[i-1].x,p[i].x,p[i+1].x);
    for(i=0;i<n;++i){
        if(h->have_frame&&h->count==n&&fresh(capture,h->capture_ms,age)&&
           abs_f(filtered[i].x-h->points[i].x)>max_step)return 0;
        if(i){float dx=filtered[i].x-filtered[i-1].x,dy=filtered[i].y-filtered[i-1].y;
            candidate.distance[i]=candidate.distance[i-1]+sqrtf(dx*dx+dy*dy);}
    }
    for(i=1;i+1<n;++i){
        float ax=filtered[i].x-filtered[i-1].x,ay=filtered[i].y-filtered[i-1].y;
        float bx=filtered[i+1].x-filtered[i].x,by=filtered[i+1].y-filtered[i].y;
        float cx=ax+bx,cy=ay+by;
        float denom=sqrtf((ax*ax+ay*ay)*(bx*bx+by*by)*(cx*cx+cy*cy));
        if(!finite_f(denom)||denom<1e-9f)return 0;
        /* Positive means right turn in x-right, y-forward coordinates. */
        candidate.curvature[i]=2*(ay*bx-ax*by)/denom;
        if(!finite_f(candidate.curvature[i]))return 0;
    }
    candidate.curvature[0]=candidate.curvature[1];candidate.curvature[n-1]=candidate.curvature[n-2];
    h->sequence=seq;h->capture_ms=capture;h->count=n;h->have_frame=1;h->units=units;
    memcpy(h->points,filtered,n*sizeof(*filtered));*out=candidate;return 1;
}
int sp_limits_valid(const sp_limits_t *c)
{
    return c&&(c->provenance==SP_SIMULATION||c->provenance==SP_PROJECT_CALIBRATION)&&
      finite_f(c->maximum_speed)&&c->maximum_speed>0&&c->maximum_speed<=100&&
      finite_f(c->near_cap)&&c->near_cap>0&&c->near_cap<=c->maximum_speed&&
      finite_f(c->lateral_accel)&&c->lateral_accel>0&&c->lateral_accel<=100&&
      finite_f(c->accel)&&c->accel>0&&c->accel<=100&&finite_f(c->brake)&&c->brake>0&&c->brake<=100&&
      finite_f(c->jerk_accel)&&c->jerk_accel>=.000001f&&c->jerk_accel<=1000&&
      finite_f(c->jerk_brake)&&c->jerk_brake>=.000001f&&c->jerk_brake<=1000&&
      finite_f(c->latency_s)&&c->latency_s>=0&&c->latency_s<=10&&
      finite_f(c->distance_margin)&&c->distance_margin>=0&&c->distance_margin<=10000&&
      finite_f(c->curvature_epsilon)&&c->curvature_epsilon>0&&c->curvature_epsilon<=1&&
      finite_f(c->max_dt_s)&&c->max_dt_s>=.00001f&&c->max_dt_s<=1;
}
static void limit(float cap,uint32_t reason,float *value,uint32_t *why)
{if(cap<*value){*value=cap;*why=reason;}else if(cap==*value)*why|=reason;}
int sp_plan(const sp_preview_t *p,const sp_limits_t *c,float measured,int near_only,float terminal,sp_plan_t *out)
{
    sp_plan_t plan;unsigned i;
    if(!p||!out||!sp_limits_valid(c)||p->units!=SP_METRES||p->count<2||p->count>SP_CAPACITY||
       !finite_f(measured)||measured<0||measured>100||!finite_f(terminal)||terminal<0||terminal>c->maximum_speed)return 0;
    memset(&plan,0,sizeof(plan));plan.count=p->count;
    for(i=0;i<p->count;++i){
        float usable,bt,brake_cap,curve;
        if(!finite_f(p->distance[i])||!finite_f(p->curvature[i])||p->distance[i]<0||p->distance[i]>10000||
           (i&&p->distance[i]<=p->distance[i-1])||(!i&&p->distance[i]!=0))return 0;
        curve=sqrtf(c->lateral_accel/(abs_f(p->curvature[i])+c->curvature_epsilon));
        plan.speed[i]=c->maximum_speed;plan.reason[i]=SP_LIMIT_MAX;
        limit(curve,SP_LIMIT_CURVE,&plan.speed[i],&plan.reason[i]);
        if(near_only)limit(c->near_cap,SP_LIMIT_NEAR,&plan.speed[i],&plan.reason[i]);
        usable=p->distance[p->count-1]-p->distance[i]-c->distance_margin;
        if(!finite_f(usable))return 0;
        if(usable<0)usable=0;
        bt=c->brake*c->latency_s;
        /* Conservative terminal: full stop inside horizon, also when caller
         * supplies a nonzero terminal. No unseen infinite straight is assumed. */
        brake_cap=(2*c->brake*usable)/(sqrtf(bt*bt+2*c->brake*usable)+bt+1e-12f);
        limit(brake_cap,SP_LIMIT_HORIZON,&plan.speed[i],&plan.reason[i]);
    }
    limit(terminal,SP_LIMIT_HORIZON,&plan.speed[p->count-1],&plan.reason[p->count-1]);
    for(i=p->count-1;i>0;--i){float ds=p->distance[i]-p->distance[i-1];
        limit(sqrtf(plan.speed[i]*plan.speed[i]+2*c->brake*ds),SP_LIMIT_BACKWARD,&plan.speed[i-1],&plan.reason[i-1]);}
    plan.current_cap=plan.speed[0]; /* Feed shaper this cap, not the measured boundary. */
    limit(measured,SP_LIMIT_FORWARD,&plan.speed[0],&plan.reason[0]);
    for(i=1;i<p->count;++i){float ds=p->distance[i]-p->distance[i-1];
        float initial=i==1?min_f(measured,plan.speed[0]):plan.speed[i-1];
        limit(sqrtf(initial*initial+2*c->accel*ds),SP_LIMIT_FORWARD,&plan.speed[i],&plan.reason[i]);}
    for(i=p->count-1;i>0;--i){float ds=p->distance[i]-p->distance[i-1];
        limit(sqrtf(plan.speed[i]*plan.speed[i]+2*c->brake*ds),SP_LIMIT_BACKWARD,&plan.speed[i-1],&plan.reason[i-1]);}
    *out=plan;return 1;
}
static float endpoint_delta(float a,float jerk_up,float jerk_down,float dt)
{
    float magnitude=abs_f(a),q=(a>=0?jerk_down:jerk_up)*dt;
    float n=ceilf(magnitude/q)-1;
    float remaining;
    if(n<0)n=0;
    remaining=dt*(n*magnitude-q*n*(n+1)*.5f);
    return a*dt+(a>=0?remaining:-remaining);
}
int sp_shape(sp_trajectory_t *s,const sp_limits_t *c,float target,float cap,float dt,int hard)
{
    float lo,hi,next,old,error;unsigned iteration;
    if(!s)return 0;
    if(hard){memset(s,0,sizeof(*s));return 1;}
    if(!sp_limits_valid(c)||!finite_f(dt)||dt<.00001f||dt>c->max_dt_s||c->jerk_accel<.000001f||c->jerk_brake<.000001f||!finite_f(target)||target<0||
       target>c->maximum_speed||!finite_f(cap)||cap<0||cap>c->maximum_speed||
       !finite_f(s->speed)||s->speed<0||s->speed>c->maximum_speed||!finite_f(s->accel)||
       s->accel< -c->brake||s->accel>c->accel)return 0;
    old=s->speed;s->brake_override=s->infeasible=0;target=min_f(target,cap);
    /* Solve the discrete velocity needed to unwind acceleration to zero.
     * A fixed 24-step scalar bisection avoids a sqrt/Euler limit cycle at
     * constant target. Every future unwind increment is an exact jerk step. */
    lo=clamp_f(s->accel-c->jerk_brake*dt,-c->brake,c->accel);
    hi=clamp_f(s->accel+c->jerk_accel*dt,-c->brake,c->accel);error=target-old;
    for(iteration=0;iteration<24;++iteration){float mid=(lo+hi)*.5f;
        if(endpoint_delta(mid,c->jerk_accel,c->jerk_brake,dt)>error)hi=mid;else lo=mid;}
    s->accel=(lo+hi)*.5f;
    next=old+s->accel*dt;
    if(next<0){next=0;s->brake_override=1;}
    if(next>cap){next=cap;s->brake_override=1;}
    if(s->brake_override){s->accel=(next-old)/dt;if(s->accel< -c->brake-1e-5f)s->infeasible=1;}
    s->speed=next;return 1;
}
int sp_wheel_targets(float v,float k,float b,int compensation,int calibrated,float l,float r,float *left,float *right)
{
    float factor=0;
    if(!left||!right||!finite_f(v)||v<0||v>100||!finite_f(k)||!finite_f(l)||!finite_f(r)||l<=0||r<=0||l>1e6f||r>1e6f)return 0;
    if(compensation){if(!calibrated||!finite_f(b)||b<=0||b>10)return 0;factor=k*b*.5f;if(abs_f(factor)>=1)return 0;}
    *left=v*(1+factor)*l;*right=v*(1-factor)*r;return 1;
}
int sp_servo_map(const sp_servo_point_t *map,unsigned n,float angle,float *pulse)
{
    unsigned i;int direction;
    if(!map||!pulse||n<3||n>SP_CAPACITY||!finite_f(angle))return 0;
    direction=map[n-1].pulse_us>map[0].pulse_us?1:-1;
    for(i=0;i<n;++i)if(!finite_f(map[i].angle_rad)||!finite_f(map[i].pulse_us)||
       map[i].pulse_us<500||map[i].pulse_us>2500||abs_f(map[i].angle_rad)>1.5f||
       (i&&(map[i].angle_rad<=map[i-1].angle_rad||(map[i].pulse_us-map[i-1].pulse_us)*direction<=0)))return 0;
    angle=clamp_f(angle,map[0].angle_rad,map[n-1].angle_rad);
    for(i=1;i<n;++i)if(angle<=map[i].angle_rad){
        *pulse=map[i-1].pulse_us+(map[i].pulse_us-map[i-1].pulse_us)*(angle-map[i-1].angle_rad)/
            (map[i].angle_rad-map[i-1].angle_rad);return 1;}
    return 0;
}
static int policy_valid(const sp_policy_t *p)
{return p&&p->frame_timeout_ms&&p->frame_timeout_ms<UINT32_C(0x80000000)&&
 p->encoder_timeout_ms&&p->encoder_timeout_ms<UINT32_C(0x80000000)&&
 p->stop_timeout_ms&&p->stop_timeout_ms<UINT32_C(0x80000000)&&p->near_window_ms&&
 p->near_window_ms<UINT32_C(0x80000000)&&p->far_budget_ms&&p->far_budget_ms<UINT32_C(0x80000000)&&
 p->near_frames&&p->far_frames&&p->zero_samples&&finite_f(p->far_distance_m)&&p->far_distance_m>0&&
 finite_f(p->zero_speed_m_s)&&p->zero_speed_m_s>=0;}
void sp_supervisor_init(sp_supervisor_t *s){memset(s,0,sizeof(*s));s->mode=SP_DISARMED;s->motor_inhibit=1;}
static void latch(sp_supervisor_t *s,sp_mode_t mode,uint32_t fault)
{if(s->authorized)++s->generation;s->authorized=0;s->motor_inhibit=1;s->faults|=fault;s->mode=mode;}
int sp_supervisor_arm(sp_supervisor_t *s,const sp_policy_t *p,const sp_input_t *i)
{
    if(!s||!i||!policy_valid(p)||s->mode!=SP_DISARMED||i->hard_stop||i->severe_fault||!i->new_frame||
       !i->near_valid||!i->far_valid||!i->encoder_valid||!finite_f(i->odometer_m)||i->odometer_m<0||
       !finite_f(i->measured_speed)||!fresh(i->now_ms,i->capture_ms,p->frame_timeout_ms)||
       !fresh(i->now_ms,i->encoder_ms,p->encoder_timeout_ms))return 0;
    s->mode=SP_NORMAL_TRACK;s->authorized=1;s->motor_inhibit=0;++s->generation;
    s->have_frame=s->have_encoder=1;s->last_frame_seq=i->frame_seq;s->last_capture_ms=i->capture_ms;
    s->last_now_ms=i->now_ms;s->last_odometer_m=i->odometer_m;s->last_encoder_seq=i->encoder_seq;
    s->last_encoder_ms=i->encoder_ms;return 1;
}
void sp_supervise(sp_supervisor_t *s,const sp_policy_t *p,const sp_input_t *i)
{
    int accepted=0;sp_mode_t previous;
    if(!s||!i)return;
    if(i->hard_stop||i->severe_fault){latch(s,(i->severe_fault||s->faults)?SP_FAULT_LATCHED:SP_STOP_LATCHED,i->severe_fault?SP_FAULT_CONTROL:0);return;}
    if(!s->authorized)return;
    if(!policy_valid(p)){latch(s,SP_FAULT_LATCHED,SP_FAULT_CONFIG);return;}
    if((uint32_t)(i->now_ms-s->last_now_ms)>=UINT32_C(0x80000000)){latch(s,SP_FAULT_LATCHED,SP_FAULT_CONTROL);return;}
    s->last_now_ms=i->now_ms;previous=s->mode;
    if(!i->encoder_valid||!finite_f(i->odometer_m)||i->odometer_m<s->last_odometer_m||
       !finite_f(i->measured_speed)||!fresh(i->now_ms,i->encoder_ms,p->encoder_timeout_ms)){
        latch(s,SP_FAULT_LATCHED,SP_FAULT_ENCODER);return;}
    if(i->encoder_seq!=s->last_encoder_seq){
        if(!forward(i->encoder_seq,s->last_encoder_seq)||!forward(i->encoder_ms,s->last_encoder_ms)){
            latch(s,SP_FAULT_LATCHED,SP_FAULT_ENCODER);return;}
        if(s->far_active)s->far_distance_m+=i->odometer_m-s->last_odometer_m;
        s->last_odometer_m=i->odometer_m;s->last_encoder_seq=i->encoder_seq;s->last_encoder_ms=i->encoder_ms;
        if(s->mode==SP_CONTROLLED_STOP){
            if(abs_f(i->measured_speed)<=p->zero_speed_m_s)++s->zero_streak;else s->zero_streak=0;
            if(s->zero_streak>=p->zero_samples){latch(s,SP_STOP_LATCHED,0);return;}
        }
    }else if(i->odometer_m!=s->last_odometer_m||i->encoder_ms!=s->last_encoder_ms){latch(s,SP_FAULT_LATCHED,SP_FAULT_ENCODER);return;}
    if(s->stop_active&&(uint32_t)(i->now_ms-s->stop_started_ms)>=p->stop_timeout_ms){
        latch(s,SP_FAULT_LATCHED,SP_FAULT_CONTROL);return;}
    /* Existing deadline takes priority over a frame arriving exactly at expiry. */
    if(!fresh(i->now_ms,s->last_capture_ms,p->frame_timeout_ms)){latch(s,SP_FAULT_LATCHED,SP_FAULT_FRAME);return;}
    if(s->near_active&&(uint32_t)(i->now_ms-s->near_started_ms)>=p->near_window_ms){latch(s,SP_FAULT_LATCHED,SP_FAULT_NEAR);return;}
    if(i->new_frame&&fresh(i->now_ms,i->capture_ms,p->frame_timeout_ms)&&forward(i->frame_seq,s->last_frame_seq)&&forward(i->capture_ms,s->last_capture_ms)){
        accepted=1;s->last_frame_seq=i->frame_seq;s->last_capture_ms=i->capture_ms;
        if(!i->near_valid){
            if(!s->near_active){s->near_active=1;s->near_started_ms=i->capture_ms;++s->generation;}
            s->near_streak=s->far_streak=0;s->mode=SP_RECOVERY_NO_LANE;s->motor_inhibit=1;
        }else{
            if(!i->far_valid&&!s->far_active){
                /* Processing latency consumes distance too. A capture-aligned
                 * odometer is required; starting at current odometer would
                 * silently omit all travel between capture and control. */
                if(!i->capture_odometer_valid||!finite_f(i->odometer_at_capture_m)||i->odometer_at_capture_m<0||
                   i->odometer_at_capture_m>i->odometer_m){latch(s,SP_FAULT_LATCHED,SP_FAULT_ENCODER);return;}
                s->far_active=1;s->far_started_ms=i->capture_ms;
                s->far_distance_m=i->odometer_m-i->odometer_at_capture_m;
            }
            if(s->near_active){
                if(++s->near_streak>=p->near_frames){s->near_active=0;s->near_streak=0;}
            }
            /* A near-only episode survives intervening no-lane recovery. */
            if(s->far_active){if(i->far_valid)++s->far_streak;else s->far_streak=0;}
            if(!s->near_active){
                s->mode=(s->stop_active||previous==SP_CONTROLLED_STOP)?SP_CONTROLLED_STOP:
                    (s->far_active?SP_DEGRADED_FAR_LOSS:SP_NORMAL_TRACK);
                s->motor_inhibit=0;
            }
        }
    }
    if(s->near_active){
        s->mode=SP_RECOVERY_NO_LANE;s->motor_inhibit=1;
        if((uint32_t)(i->now_ms-s->near_started_ms)>=p->near_window_ms)latch(s,SP_FAULT_LATCHED,SP_FAULT_NEAR);
        return;
    }
    if(s->far_active&&((uint32_t)(i->now_ms-s->far_started_ms)>=p->far_budget_ms||s->far_distance_m>=p->far_distance_m)){
        if(!s->stop_active){s->stop_started_ms=i->now_ms;s->stop_active=1;}
        s->mode=SP_CONTROLLED_STOP;return;
    }
    if(!s->stop_active&&s->mode!=SP_CONTROLLED_STOP&&s->far_active&&accepted&&s->far_streak>=p->far_frames){
        s->far_active=0;s->far_distance_m=0;s->far_streak=0;s->mode=SP_NORMAL_TRACK;
    }
}
int sp_candidate_allowed(const sp_supervisor_t *s,uint32_t epoch)
{return s&&s->authorized&&!s->motor_inhibit&&s->generation==epoch;}
