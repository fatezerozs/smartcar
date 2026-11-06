#include "speed_planner.h"
#include "vision_adapter.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static sp_limits_t limits(void)
{sp_limits_t c={SP_SIMULATION,3,1,2,1,2,4,6,.02f,.02f,.0001f,.05f};return c;}
static sp_policy_t policy(void)
{sp_policy_t p={100,30,300,1000,1000,5,3,3,2,.01f};return p;}
static sp_input_t input(void)
{sp_input_t i;memset(&i,0,sizeof(i));i.new_frame=i.near_valid=i.far_valid=i.encoder_valid=i.capture_odometer_valid=1;return i;}
static void step(sp_input_t *i,int near_ok,int far_ok,float distance)
{i->now_ms+=20;i->capture_ms=i->encoder_ms=i->now_ms;++i->frame_seq;++i->encoder_seq;
 i->near_valid=(uint8_t)near_ok;i->far_valid=(uint8_t)far_ok;i->odometer_m+=distance;i->odometer_at_capture_m=i->odometer_m;}
static void geometry_tests(void)
{
    sp_geometry_history_t h={0};sp_preview_t out;sp_point_t points[SP_CAPACITY];
    const float map[9]={.01f,0,-.94f,0,-.01f,1.2f,0,0,1}; /* SIMULATION ONLY */
    uint8_t rows[SP_CAPACITY];vision_result_t r;sp_limits_t c=limits();sp_plan_t plan;
    memset(&r,0,sizeof(r));r.valid=1;r.status=VISION_TRACK;r.confidence=90;
    for(unsigned j=0;j<SP_CAPACITY;++j){points[j].x=0;points[j].y=(float)j*.1f;
        rows[j]=(uint8_t)(110-j*5);r.left[rows[j]]=60;r.right[rows[j]]=128;
        r.center[rows[j]]=94;r.row_flags[rows[j]]=3;}
    CHECK(sp_geometry(&h,points,SP_CAPACITY,SP_METRES,1,20,20,100,.1f,&out));
    for(unsigned j=0;j<SP_CAPACITY;++j)CHECK(fabsf(out.curvature[j])<1e-5f);
    points[8].x=100;CHECK(sp_geometry(&h,points,SP_CAPACITY,SP_METRES,2,40,40,100,.1f,&out));
    CHECK(fabsf(out.curvature[8])<1e-5f); /* interior spike rejected by median */
    CHECK(!sp_geometry(&h,points,SP_CAPACITY,SP_METRES,2,40,50,100,.1f,&out));
    CHECK(!sp_geometry(&h,points,SP_CAPACITY,SP_METRES,1,10,50,100,.1f,&out));
    CHECK(!sp_geometry(&h,points,SP_CAPACITY,SP_METRES,3,60,160,100,.1f,&out));
    CHECK(!sp_geometry(&h,points,SP_CAPACITY,SP_METRES,3,200,160,100,.1f,&out));
    CHECK(h.sequence==2&&h.capture_ms==40);
    for(unsigned j=0;j<SP_CAPACITY;++j)points[j].x=1;
    CHECK(!sp_geometry(&h,points,SP_CAPACITY,SP_METRES,3,60,60,100,.1f,&out));
    memset(&h,0,sizeof(h));
    for(unsigned j=0;j<SP_CAPACITY;++j){float a=(float)j*.04f;points[j].x=2*(1-cosf(a));points[j].y=2*sinf(a);}
    CHECK(sp_geometry(&h,points,SP_CAPACITY,SP_METRES,1,20,20,100,.1f,&out));
    for(unsigned j=2;j+2<SP_CAPACITY;++j)CHECK(fabsf(out.curvature[j]-.5f)<.001f);
    memset(&h,0,sizeof(h));
    CHECK(sp_vision_preview(&h,&r,rows,SP_CAPACITY,50,NULL,1,20,20,100,.1f,&out));
    CHECK(out.units==SP_NORMALIZED&&!sp_plan(&out,&c,1,0,0,&plan));
    memset(&h,0,sizeof(h));CHECK(sp_vision_preview(&h,&r,rows,SP_CAPACITY,50,map,1,20,20,100,.1f,&out));
    CHECK(out.units==SP_METRES);
    r.row_flags[rows[4]]=1;CHECK(!sp_vision_preview(&h,&r,rows,SP_CAPACITY,50,map,2,40,40,100,.1f,&out));
    CHECK(h.sequence==1); /* partial never renews geometry */
    memset(&h,0,sizeof(h));
    CHECK(sp_geometry(&h,points,SP_CAPACITY,SP_METRES,UINT32_MAX,UINT32_MAX-10,UINT32_MAX-10,100,1,&out));
    CHECK(sp_geometry(&h,points,SP_CAPACITY,SP_METRES,0,9,9,100,1,&out));
}
static void planner_tests(FILE *csv)
{
    sp_preview_t p;sp_limits_t c=limits();sp_plan_t plan,again;unsigned case_id,j;
    memset(&p,0,sizeof(p));p.units=SP_METRES;p.count=SP_CAPACITY;
    for(case_id=0;case_id<6;++case_id){
        for(j=0;j<SP_CAPACITY;++j){
            p.distance[j]=(float)j*.25f;
            p.curvature[j]=case_id==0?0:case_id==1?.2f:case_id==2?2.0f:
                case_id==3?2*sinf((float)j*.6f):case_id==4?(j>=8?3.0f:0):.2f+.001f*sinf((float)j*2.0f);
        }
        CHECK(sp_plan(&p,&c,2,0,0,&plan));CHECK(sp_plan(&p,&c,2,0,0,&again));
        CHECK(memcmp(&plan,&again,sizeof(plan))==0);
        for(j=0;j<SP_CAPACITY;++j){
            float speed=plan.speed[j],distance=p.distance[p.count-1]-p.distance[j]-c.distance_margin;
            if(distance<0)distance=0;
            CHECK(speed>=0&&speed<=c.maximum_speed);
            CHECK(speed*speed*(fabsf(p.curvature[j])+c.curvature_epsilon)<=c.lateral_accel+1e-4f);
            CHECK(speed*c.latency_s+speed*speed/(2*c.brake)<=distance+1e-4f);
            CHECK(plan.reason[j]!=0);
            if(j){float ds=p.distance[j]-p.distance[j-1];
                CHECK(speed*speed<=plan.speed[j-1]*plan.speed[j-1]+2*c.accel*ds+1e-4f);
                CHECK(plan.speed[j-1]*plan.speed[j-1]<=speed*speed+2*c.brake*ds+1e-4f);}
            fprintf(csv,"spatial,%u,%u,%.9g,%.9g,%.9g,%u\n",case_id,j,(double)p.distance[j],(double)p.curvature[j],(double)speed,plan.reason[j]);
        }
        if(case_id==4){CHECK(plan.speed[6]<2);CHECK(plan.reason[6]&SP_LIMIT_BACKWARD);}
        CHECK(sp_plan(&p,&c,2,1,0,&again));for(j=0;j<SP_CAPACITY;++j)CHECK(again.speed[j]<=c.near_cap);
    }
    CHECK(sp_plan(&p,&c,0,0,0,&plan)&&plan.speed[0]==0&&plan.current_cap>0); /* can launch */
    p.distance[2]=p.distance[1];CHECK(!sp_plan(&p,&c,2,0,0,&plan));p.distance[2]=.5f;
    p.curvature[1]=NAN;CHECK(!sp_plan(&p,&c,2,0,0,&plan));p.curvature[1]=0;
    p.count=SP_CAPACITY+1;CHECK(!sp_plan(&p,&c,2,0,0,&plan));
    /* Analytic curvature-noise Lipschitz bound. min/cap and propagation
     * cannot amplify the speed perturbation; this is synthetic, not data. */
    p.count=SP_CAPACITY;
    for(j=0;j<SP_CAPACITY;++j){p.distance[j]=(float)j*10;p.curvature[j]=.8f;}
    CHECK(sp_plan(&p,&c,2,0,0,&plan));
    for(j=0;j<SP_CAPACITY;++j)p.curvature[j]+=.001f*sinf((float)j*2);
    CHECK(sp_plan(&p,&c,2,0,0,&again));
    {float noise_bound=.5f*sqrtf(c.lateral_accel)/powf(.799f+c.curvature_epsilon,1.5f)*.001f;
     for(j=0;j<SP_CAPACITY;++j)CHECK(fabsf(again.speed[j]-plan.speed[j])<=noise_bound+1e-5f);}
}
static void trajectory_tests(FILE *csv)
{
    sp_limits_t c=limits();sp_trajectory_t t={0},copy;float old_a=0,old_v=0;const float dt=.02f;
    for(unsigned j=0;j<500;++j){
        float target=j<150?2:j<320?1:2.5f;
        copy=t;CHECK(sp_shape(&t,&c,target,c.maximum_speed,dt,0));
        CHECK(sp_shape(&copy,&c,target,c.maximum_speed,dt,0));CHECK(memcmp(&copy,&t,sizeof(t))==0);
        CHECK(!t.brake_override&&!t.infeasible);
        CHECK(t.accel<=c.accel+1e-5f&&t.accel>=-c.brake-1e-5f);
        CHECK((t.accel-old_a)/dt<=c.jerk_accel+.001f&&(t.accel-old_a)/dt>=-c.jerk_brake-.001f);
        CHECK(fabsf((t.speed-old_v)/dt-t.accel)<1e-4f);
        if((j>=125&&j<150)||(j>=230&&j<320)||j>=450){
            CHECK(fabsf(t.speed-target)<1e-5f&&fabsf(t.accel)<1e-4f);}
        fprintf(csv,"temporal,0,%u,%.9g,%.9g,%.9g,%u\n",j,(double)((float)j*dt),(double)t.accel,(double)t.speed,t.brake_override);
        old_a=t.accel;old_v=t.speed;
    }
    copy=t;CHECK(!sp_shape(&t,&c,1,3,0,0)&&memcmp(&t,&copy,sizeof(t))==0);
    CHECK(!sp_shape(&t,&c,1,3,.1f,0));
    CHECK(sp_shape(&t,&c,0,.2f,dt,0)&&t.speed<=.2f&&t.brake_override&&t.infeasible);
    CHECK(sp_shape(&t,NULL,NAN,NAN,NAN,1)&&t.speed==0&&t.accel==0);
    /* A feasible safety cap can override jerk without hiding infeasibility. */
    memset(&t,0,sizeof(t));t.speed=1;t.accel=1;
    CHECK(sp_shape(&t,&c,.98f,.98f,dt,0)&&t.brake_override&&!t.infeasible&&t.speed==.98f);
}
static void supervisor_tests(void)
{
    sp_policy_t p=policy();sp_supervisor_t s;sp_input_t i=input();uint32_t generation;
    sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));generation=s.generation;
    step(&i,1,0,.02f);sp_supervise(&s,&p,&i);
    CHECK(s.mode==SP_DEGRADED_FAR_LOSS&&s.authorized&&!s.motor_inhibit&&s.generation==generation);
    for(unsigned j=0;j<10;++j){step(&i,1,0,.02f);sp_supervise(&s,&p,&i);CHECK(s.far_started_ms==20);}
    step(&i,0,0,.02f);sp_supervise(&s,&p,&i);
    CHECK(s.mode==SP_RECOVERY_NO_LANE&&s.motor_inhibit&&!sp_candidate_allowed(&s,generation));
    generation=s.generation;
    for(unsigned j=0;j<p.near_frames-1;++j){step(&i,1,0,0);sp_supervise(&s,&p,&i);CHECK(s.motor_inhibit);}
    step(&i,1,0,0);sp_supervise(&s,&p,&i);CHECK(s.mode==SP_DEGRADED_FAR_LOSS&&!s.motor_inhibit&&s.far_started_ms==20);
    for(unsigned j=0;j<p.far_frames-1;++j){step(&i,1,1,0);sp_supervise(&s,&p,&i);CHECK(s.far_active);}
    step(&i,1,1,0);sp_supervise(&s,&p,&i);CHECK(s.mode==SP_NORMAL_TRACK&&!s.far_active);
    CHECK(sp_candidate_allowed(&s,generation));i.hard_stop=1;sp_supervise(&s,&p,&i);
    CHECK(s.mode==SP_STOP_LATCHED&&!sp_candidate_allowed(&s,generation));i.hard_stop=0;
    CHECK(!sp_supervisor_arm(&s,&p,&i));step(&i,1,1,0);sp_supervise(&s,&p,&i);CHECK(!s.authorized);
    /* Time, distance and simultaneous budgets. No new image needed to expire. */
    for(unsigned which=0;which<3;++which){
        p=policy();p.far_budget_ms=which==1?1000:which==2?80:100;p.far_distance_m=which==0?20:.125f;
        i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
        step(&i,1,0,0);sp_supervise(&s,&p,&i);
        for(unsigned j=0;j<5;++j){step(&i,1,0,.03125f);sp_supervise(&s,&p,&i);}
        CHECK(s.mode==SP_CONTROLLED_STOP&&s.authorized&&!s.motor_inhibit);
        for(unsigned j=0;j<p.zero_samples;++j){step(&i,1,0,0);i.measured_speed=0;sp_supervise(&s,&p,&i);}
        CHECK(s.mode==SP_STOP_LATCHED&&!s.authorized);
    }
    /* Deadline priority, repeated and reordered frames never count as recovery. */
    p=policy();i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    step(&i,0,0,0);sp_supervise(&s,&p,&i);uint32_t lost=s.near_started_ms;
    for(unsigned j=0;j<3;++j){step(&i,1,0,0);--i.frame_seq;sp_supervise(&s,&p,&i);CHECK(s.near_streak==0);}
    CHECK(s.near_started_ms==lost);
    for(unsigned j=0;j<16&&s.authorized;++j){step(&i,0,0,0);--i.frame_seq;sp_supervise(&s,&p,&i);}
    CHECK(s.mode==SP_FAULT_LATCHED&&(s.faults&SP_FAULT_FRAME)); /* duplicate stream expired */
    i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));step(&i,0,0,0);sp_supervise(&s,&p,&i);
    for(unsigned j=0;j<14;++j){step(&i,0,0,0);sp_supervise(&s,&p,&i);}
    step(&i,1,1,0);sp_supervise(&s,&p,&i);CHECK(s.mode==SP_FAULT_LATCHED&&(s.faults&SP_FAULT_NEAR));
    /* Frame camera watchdog is independent of near recovery window. */
    i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    for(unsigned j=0;j<5;++j){step(&i,1,1,0);i.new_frame=0;sp_supervise(&s,&p,&i);}
    CHECK(s.faults&SP_FAULT_FRAME);
    /* Control updates expiry between images; it does not wait for new_frame. */
    p=policy();p.far_budget_ms=40;i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    step(&i,1,0,0);sp_supervise(&s,&p,&i);
    step(&i,1,0,0);i.new_frame=0;sp_supervise(&s,&p,&i);CHECK(s.mode==SP_DEGRADED_FAR_LOSS);
    step(&i,1,0,0);i.new_frame=0;sp_supervise(&s,&p,&i);CHECK(s.mode==SP_CONTROLLED_STOP);
    p=policy();
    i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    step(&i,1,1,0);--i.encoder_seq;sp_supervise(&s,&p,&i);CHECK(s.faults&SP_FAULT_ENCODER);
    /* uint32 wraps retain forward ordering. */
    i=input();i.now_ms=i.capture_ms=i.encoder_ms=UINT32_MAX-30;i.frame_seq=i.encoder_seq=UINT32_MAX-1;
    sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    for(unsigned j=0;j<4;++j){step(&i,1,0,.01f);sp_supervise(&s,&p,&i);CHECK(s.mode==SP_DEGRADED_FAR_LOSS);}
    generation=s.generation;i.severe_fault=1;sp_supervise(&s,&p,&i);CHECK(s.mode==SP_FAULT_LATCHED&&!sp_candidate_allowed(&s,generation));
    /* Near recovery cannot reset expired far episode. */
    p=policy();p.far_budget_ms=100;i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    step(&i,1,0,0);sp_supervise(&s,&p,&i);step(&i,0,0,0);sp_supervise(&s,&p,&i);
    for(unsigned j=0;j<p.near_frames;++j){step(&i,1,0,0);sp_supervise(&s,&p,&i);}
    CHECK(s.mode==SP_CONTROLLED_STOP&&s.far_started_ms==20);
    /* No-wheel-zero controlled stop has a separate finite failure deadline. */
    i.measured_speed=1;
    for(unsigned j=0;j<51&&s.authorized;++j){step(&i,1,0,.001f);sp_supervise(&s,&p,&i);}
    CHECK(s.mode==SP_FAULT_LATCHED&&(s.faults&SP_FAULT_CONTROL));
    p=policy();p.far_distance_m=.25f;i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    step(&i,1,0,.5f);i.capture_ms=10;i.odometer_at_capture_m=0;sp_supervise(&s,&p,&i);
    CHECK(s.mode==SP_CONTROLLED_STOP&&s.far_distance_m==.5f);
    i=input();sp_supervisor_init(&s);CHECK(sp_supervisor_arm(&s,&p,&i));
    step(&i,1,0,.01f);i.capture_odometer_valid=0;sp_supervise(&s,&p,&i);CHECK(s.faults&SP_FAULT_ENCODER);
}
static void actuator_model_tests(void)
{
    float left,right,pulse;
    const sp_servo_point_t map[3]={{1700,-.4f},{1550,0},{1400,.3f}}; /* fictitious angles */
    CHECK(sp_wheel_targets(1,0,0,0,0,1000,1100,&left,&right)&&left==1000&&right==1100);
    CHECK(!sp_wheel_targets(1,1,.2f,1,0,1000,1000,&left,&right));
    CHECK(sp_wheel_targets(1,1,.2f,1,1,1000,1000,&left,&right)&&left>right&&fabsf((left+right)*.5f-1000)<.001f);
    CHECK(sp_wheel_targets(1,-1,.2f,1,1,1000,1000,&left,&right)&&left<right);
    CHECK(!sp_wheel_targets(1,20,.2f,1,1,1000,1000,&left,&right));
    CHECK(sp_servo_map(map,3,0,&pulse)&&fabsf(pulse-1550)<.001f);
    CHECK(sp_servo_map(map,3,.15f,&pulse)&&fabsf(pulse-1475)<.001f);
    CHECK(sp_servo_map(map,3,1,&pulse)&&pulse==1400);
    CHECK(sp_servo_map(map,3,-1,&pulse)&&pulse==1700);
}
int main(void)
{
    FILE *csv=fopen("planning-replay.csv","w");clock_t start;sp_preview_t p;sp_plan_t plan;sp_limits_t c=limits();
    CHECK(csv!=NULL);fprintf(csv,"track,case,index,coordinate,feature,speed,reason\n");
    geometry_tests();planner_tests(csv);trajectory_tests(csv);supervisor_tests();actuator_model_tests();fclose(csv);
    memset(&p,0,sizeof(p));p.units=SP_METRES;p.count=SP_CAPACITY;
    for(unsigned j=0;j<SP_CAPACITY;++j){p.distance[j]=(float)j*.25f;p.curvature[j]=j<8?0:2;}
    start=clock();for(unsigned j=0;j<100000;++j)CHECK(sp_plan(&p,&c,1,0,0,&plan));
    printf("HOST SIMULATION PASS: %u checks; planner 100000 calls CPU %.6f s; sizeof preview=%zu history=%zu plan=%zu trajectory=%zu supervisor=%zu bytes.\n",
        checks,(double)(clock()-start)/CLOCKS_PER_SEC,sizeof(p),sizeof(sp_geometry_history_t),sizeof(plan),sizeof(sp_trajectory_t),sizeof(sp_supervisor_t));
    puts("No calibration, measured tracking, MCU integration or physical safety claim.");return 0;
}
