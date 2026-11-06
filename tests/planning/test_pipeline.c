#include "pipeline.h"
#include "reference_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"pipeline:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static FILE *open_csv(const char *name)
{
    FILE *stream=NULL;
#ifdef _MSC_VER
    if(fopen_s(&stream,name,"w")!=0)return NULL;
#else
    stream=fopen(name,"w");
#endif
    return stream;
}
static sp_pipeline_config_t config(float factor)
{
    sp_pipeline_config_t c;memset(&c,0,sizeof(c));
    {sp_limits_t limits={SP_SIMULATION,REF_MAX_SPEED,REF_NEAR_CAP,REF_LAT_ACCEL,REF_ACCEL,REF_BRAKE,
        REF_JERK_ACC,REF_JERK_BRAKE,REF_LATENCY,REF_MARGIN,.0001f,.05f};c.limits=limits;}
    {sp_policy_t policy={100,30,REF_NEAR_MS,REF_FAR_MS,REF_STOP_MS,REF_NEAR_FRAMES,REF_FAR_FRAMES,3,
        REF_FAR_DISTANCE,.02f};c.policy=policy;}
    c.ticks_per_metre_left=REF_TICKS_PER_M_LEFT*factor;
    c.ticks_per_metre_right=REF_TICKS_PER_M_RIGHT*factor;
    c.feedforward_full_speed=REF_PLANT_SPEED;c.planning_brake=REF_PLAN_BRAKE;c.kp_q8=REF_KP;c.ki_q8=REF_KI;return c;
}
static void preview(sp_preview_t *p,const sp_input_t *i,float horizon,unsigned scenario,unsigned tick)
{
    memset(p,0,sizeof(*p));p->units=SP_METRES;p->count=SP_CAPACITY;p->sequence=i->frame_seq;p->capture_ms=i->capture_ms;
    for(unsigned j=0;j<SP_CAPACITY;++j){
        p->distance[j]=horizon*(float)j/(SP_CAPACITY-1);
        if(scenario==1){
            float gap=.8f-i->odometer_m;
            if(gap>0&&gap<horizon){
                p->distance[j]=j<8?gap*(float)j/8:gap+(horizon-gap)*(float)(j-8)/(SP_CAPACITY-1-8);
                p->curvature[j]=j>=8?5.0f:0;
            }else if(gap<=0&&i->odometer_m<1.2f)p->curvature[j]=5;
        }
        if(scenario==11&&tick>=100)p->curvature[j]=10;
    }
}
static uint32_t mix(uint32_t hash,const sp_telemetry_t *s)
{
    const unsigned char *bytes=(const unsigned char *)s;
    for(unsigned j=0;j<sizeof(*s);++j)hash=(hash^bytes[j])*16777619U;return hash;
}
static uint32_t run(unsigned scenario,float factor,int slow_storage,FILE *csv,FILE *queue_csv)
{
    sp_pipeline_t p;sp_pipeline_config_t cfg=config(factor);sp_pipeline_input_t input;sp_preview_t near_p,full_p;
    sp_telemetry_queue_t queue;sp_telemetry_t record,discard;sp_pipeline_candidate_t candidate,stale,ignored;
    float velocity[2]={0,0},position[2]={0,0},last_odom=0;int32_t last_count[2]={0,0};
    uint32_t hash=2166136261U;unsigned seen_degraded=0,seen_recovery=0,seen_controlled=0,near_zero_checks=0;
    memset(&input,0,sizeof(input));memset(&queue,0,sizeof(queue));
    input.observation.new_frame=input.observation.near_valid=input.observation.far_valid=input.observation.encoder_valid=1;
    input.observation.capture_odometer_valid=1;
    /* Isolate the time deadline from the independent distance deadline. */
    if(scenario==3)cfg.policy.far_distance_m=1.0f;
    if(scenario==4){cfg.policy.far_budget_ms=2000;cfg.policy.far_distance_m=.05f;}
    CHECK(sp_pipeline_init(&p,&cfg,(uint64_t)scenario+1));
    preview(&near_p,&input.observation,REF_NEAR_DISTANCE,scenario,0);preview(&full_p,&input.observation,REF_FULL_DISTANCE,scenario,0);
    CHECK(sp_pipeline_arm(&p,&input,&near_p,&full_p));
    for(unsigned tick=1;tick<=500;++tick){
        sp_input_t *i=&input.observation;int prepared;
        for(unsigned side=0;side<2;++side){
            float gain=side==0?.92f:1.0f;
            float pwm=(float)(side==0?p.left_pwm:p.right_pwm)/1000;
            velocity[side]+=.01f/REF_PLANT_TAU*(gain*REF_PLANT_SPEED*pwm-velocity[side]);
            position[side]+=velocity[side]*.01f;
        }
        i->now_ms=tick*10U;i->new_frame=(uint8_t)(tick%2U==0);
        if(!(scenario==8&&tick>=100)){
            i->encoder_ms=i->now_ms;++i->encoder_seq;
            {int32_t left=(int32_t)lroundf(position[0]*cfg.ticks_per_metre_left),right=(int32_t)lroundf(position[1]*cfg.ticks_per_metre_right);
             input.measured_left_ticks10ms=left-last_count[0];input.measured_right_ticks10ms=right-last_count[1];
             last_count[0]=left;last_count[1]=right;}
            i->measured_speed=.5f*((float)input.measured_left_ticks10ms/cfg.ticks_per_metre_left+
                (float)input.measured_right_ticks10ms/cfg.ticks_per_metre_right)*100;
            i->odometer_m=.5f*(position[0]+position[1]);last_odom=i->odometer_m;
        }else i->odometer_m=last_odom;
        if(scenario==7&&tick>=100)i->new_frame=0;
        if(i->new_frame){
            ++i->frame_seq;i->capture_ms=i->now_ms;i->odometer_at_capture_m=i->odometer_m;
            i->near_valid=1;i->far_valid=1;
            if((scenario==2&&tick>=100&&tick<110)||((scenario==3||scenario==4)&&tick>=100)||
               (scenario==5&&tick>=80&&tick<130))i->far_valid=0;
            if((scenario==5&&tick>=100&&tick<104)||(scenario==6&&tick>=100))i->near_valid=0;
            preview(&near_p,i,REF_NEAR_DISTANCE,scenario,tick);preview(&full_p,i,REF_FULL_DISTANCE,scenario,tick);
            if(scenario==10&&tick>=100&&tick<104)near_p.sequence=i->frame_seq-1;
        }
        prepared=sp_pipeline_prepare(&p,&input,&near_p,&full_p,&candidate);
        if(scenario==9&&tick==110){
            CHECK(prepared);stale=candidate;i->hard_stop=1;
            CHECK(!sp_pipeline_prepare(&p,&input,&near_p,&full_p,&ignored));CHECK(!sp_pipeline_commit(&p,&stale));
        }else if(prepared){CHECK(sp_pipeline_commit(&p,&candidate));CHECK(!sp_pipeline_commit(&p,&candidate));}
        if(p.supervisor.motor_inhibit||!p.supervisor.authorized){
            CHECK(!p.left_pwm&&!p.right_pwm&&!p.left_target&&!p.right_target);
            CHECK(!p.pi[0].integral_q8&&!p.pi[1].integral_q8);++near_zero_checks;
        }
        if(p.supervisor.mode==SP_DEGRADED_FAR_LOSS){++seen_degraded;CHECK(p.supervisor.authorized&&!p.supervisor.motor_inhibit);}
        if((scenario==2||scenario==3||scenario==4)&&tick==100)CHECK(p.left_pwm&&p.right_pwm&&p.left_target&&p.right_target);
        if(p.supervisor.mode==SP_RECOVERY_NO_LANE)++seen_recovery;
        if(p.supervisor.mode==SP_CONTROLLED_STOP){
            if(!seen_controlled&&scenario==3){
                CHECK(i->now_ms-p.supervisor.far_started_ms==REF_FAR_MS);
                CHECK(p.supervisor.far_distance_m<cfg.policy.far_distance_m);
            }
            if(!seen_controlled&&scenario==4){
                CHECK(i->now_ms-p.supervisor.far_started_ms<cfg.policy.far_budget_ms);
                CHECK(p.supervisor.far_distance_m>=cfg.policy.far_distance_m);
            }
            ++seen_controlled;
        }
        CHECK(p.left_pwm<=1000&&p.right_pwm<=1000&&p.trajectory.speed>=0&&p.trajectory.speed<=REF_MAX_SPEED+.00001f);
        CHECK(fabsf((float)p.left_target-p.trajectory.speed*cfg.ticks_per_metre_left*.01f)<=.5001f);
        CHECK(fabsf((float)p.right_target-p.trajectory.speed*cfg.ticks_per_metre_right*.01f)<=.5001f);
        if(scenario==1&&tick>=150){
            CHECK(p.trajectory.speed>=sqrtf(REF_LAT_ACCEL/(5.0f+.0001f))-.00001f);
            CHECK(!p.trajectory.brake_override);
        }
        sp_pipeline_snapshot(&p,&input,&record);hash=mix(hash,&record);
        (void)sp_telemetry_push(&queue,&record);
        if(tick%(slow_storage?30U:2U)==0)while(sp_telemetry_pop(&queue,&discard)){}
        if(csv)fprintf(csv,"%u,%u,%u,%u,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%d,%d,%u,%u,%ld,%ld,%u,%u,%u,%u,%.9g,%u,%u,%u,%u,%u\n",
            scenario,tick*10U,(unsigned)p.supervisor.mode,p.supervisor.generation,p.commit_sequence,record.near_valid,record.far_valid,
            (double)p.trajectory.speed,(double)p.trajectory.accel,(double)i->measured_speed,(double)i->odometer_m,
            p.left_target,p.right_target,p.left_pwm,p.right_pwm,(long)p.pi[0].integral_q8,(long)p.pi[1].integral_q8,
            (unsigned)queue.dropped,(unsigned)p.supervisor.faults,p.supervisor.far_started_ms,(unsigned)record.brake_override,(double)record.current_cap,
            record.frame_sequence,record.capture_ms,record.encoder_sequence,record.raw_near_valid,record.raw_far_valid);
    }
    if(slow_storage)CHECK(queue.dropped>0&&queue.high_water==SP_TELEMETRY_CAPACITY);else CHECK(!queue.dropped);
    if(scenario<=2||scenario==5||scenario==10){
        if(p.supervisor.faults)fprintf(stderr,"scenario=%u factor=%.2f fault=%u\n",scenario,(double)factor,p.supervisor.faults);
        CHECK(!p.supervisor.faults&&p.supervisor.mode==SP_NORMAL_TRACK);
    }
    if(scenario==2)CHECK(seen_degraded>0);
    if(scenario==3||scenario==4){CHECK(seen_controlled>0);CHECK(p.supervisor.mode==SP_STOP_LATCHED);}
    if(scenario==5)CHECK(seen_degraded&&seen_recovery);
    if(scenario==6)CHECK((p.supervisor.faults&SP_FAULT_NEAR)&&near_zero_checks);
    if(scenario==7)CHECK(p.supervisor.faults&SP_FAULT_FRAME);
    if(scenario==8)CHECK(p.supervisor.faults&SP_FAULT_ENCODER);
    if(scenario==9)CHECK(p.supervisor.mode==SP_STOP_LATCHED&&near_zero_checks);
    if(scenario==10)CHECK(seen_recovery);
    if(scenario==11)CHECK(p.supervisor.faults&SP_FAULT_CONTROL);
    if(queue_csv)fprintf(queue_csv,"%u,%d,%u,%u,%u\n",scenario,slow_storage,(unsigned)queue.dropped,queue.high_water,hash);
    return hash;
}
static void interleaving(void)
{
    sp_pipeline_t p;sp_pipeline_config_t c=config(1);sp_pipeline_input_t i;sp_preview_t n,f;sp_pipeline_candidate_t a,b;
    memset(&i,0,sizeof(i));i.observation.new_frame=i.observation.near_valid=i.observation.far_valid=i.observation.encoder_valid=1;
    i.observation.capture_odometer_valid=1;
    CHECK(sp_pipeline_init(&p,&c,100));preview(&n,&i.observation,REF_NEAR_DISTANCE,0,0);preview(&f,&i.observation,REF_FULL_DISTANCE,0,0);
    CHECK(sp_pipeline_arm(&p,&i,&n,&f));i.observation.new_frame=0;i.observation.now_ms=i.observation.encoder_ms=10;i.observation.encoder_seq=1;
    CHECK(sp_pipeline_prepare(&p,&i,&n,&f,&a));
    /* A second request sharing an unconsumed sample invalidates the first. */
    CHECK(sp_pipeline_prepare(&p,&i,&n,&f,&b));CHECK(!sp_pipeline_commit(&p,&a));CHECK(sp_pipeline_commit(&p,&b));
    CHECK(!sp_pipeline_commit(&p,&b));
    i.observation.now_ms=i.observation.encoder_ms=20;i.observation.encoder_seq=2;
    CHECK(sp_pipeline_prepare(&p,&i,&n,&f,&a));
    CHECK(sp_pipeline_init(&p,&c,101));CHECK(!sp_pipeline_commit(&p,&a));
    memset(&i,0,sizeof(i));i.observation.new_frame=i.observation.near_valid=i.observation.far_valid=i.observation.encoder_valid=1;
    i.observation.capture_odometer_valid=1;CHECK(sp_pipeline_arm(&p,&i,&n,&f));
    i.observation.new_frame=0;i.observation.now_ms=i.observation.encoder_ms=10;i.observation.encoder_seq=1;
    CHECK(sp_pipeline_prepare(&p,&i,&n,&f,&a));
    i.observation.now_ms=i.observation.encoder_ms=20;i.observation.encoder_seq=2;
    CHECK(!sp_pipeline_prepare(&p,&i,&n,&f,&b));CHECK(!sp_pipeline_commit(&p,&a));CHECK(!p.left_pwm&&!p.right_pwm);
    {sp_preview_t shifted;
     CHECK(sp_preview_advance(&n,.005f,&shifted)&&shifted.sequence==n.sequence&&shifted.capture_ms==n.capture_ms);
     CHECK(fabsf(shifted.distance[shifted.count-1]-(REF_NEAR_DISTANCE-.005f))<.00001f);
     CHECK(!sp_preview_advance(&n,REF_NEAR_DISTANCE,&shifted));
     n.distance[2]=n.distance[1];CHECK(!sp_preview_advance(&n,.005f,&shifted));}
}
int main(int argc,char **argv)
{
    float factor=argc==2?(float)strtod(argv[1],NULL):1;FILE *csv,*queue_csv;char name[80];
    CHECK(factor==.5f||factor==1||factor==2);
    snprintf(name,sizeof(name),"pipeline-replay-%.1f.csv",(double)factor);csv=open_csv(name);CHECK(csv!=NULL);
    fprintf(csv,"case,timeMs,mode,generation,commitSequence,nearValid,farValid,speedRefMps,accelRefMps2,measuredMps,odometerM,leftTargetTicks10ms,rightTargetTicks10ms,leftPwmPermille,rightPwmPermille,leftIntegralQ8,rightIntegralQ8,queueDrops,faults,farStartedMs,brakeOverride,currentCapMps,frameSequence,captureMs,encoderSequence,rawNearValid,rawFarValid\n");
    snprintf(name,sizeof(name),"pipeline-queue-%.1f.csv",(double)factor);queue_csv=open_csv(name);CHECK(queue_csv!=NULL);
    fprintf(queue_csv,"case,slowStorage,dropped,highWater,controlHash\n");
    for(unsigned scenario=0;scenario<12;++scenario){uint32_t fast=run(scenario,factor,0,csv,queue_csv),slow=run(scenario,factor,1,NULL,queue_csv);CHECK(fast==slow);}
    interleaving();fclose(csv);fclose(queue_csv);printf("PIPELINE SIMULATION PASS: %u checks; sensor resolution factor=%.1f; 12 scenarios; queue backpressure leaves all control snapshots identical.\n",checks,(double)factor);return 0;
}
