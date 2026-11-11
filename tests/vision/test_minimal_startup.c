/* Actual C26 admission -> encoder sampling -> PI -> final writer, simulated IO.
 * Signed samples below are constructed adversarial inputs, not physical noise
 * distributions. The field trigger was near -5 counts/10ms at launch. */
#include "../support/fixtures/switch_io.h"
#include "vision_drive_internal.h"
static unsigned scenarios;
static uint32_t next_boot_ms;
static int explicit_camera;
static void frame_step(void)
{
    switch_step(1);
    /* The generic fixture schedules by absolute %20. Across uint32 rollover
     * that phase changes, so publish explicit current captures in this case. */
    if(explicit_camera&&now%20U)vision_drive_observe(&route_lane,++route_sequence,now,now);
}
static void sample(int left,int right)
{
    switch_delta_left=-left;switch_delta_right=right;frame_step();
    CHECK(pwm[0]<=10000&&pwm[1]<=10000);
}
static void launch(void)
{
    explicit_camera=0;switch_boot(1);switch_delta_left=switch_delta_right=0;switch_wait(10,1);
    if(next_boot_ms){
        now=next_boot_ms;next_boot_ms=0;app_mode_init(1,1,now);vision_drive_init(now);
        explicit_camera=1;for(unsigned i=0;i<10;++i)frame_step();
    }
    route_lane.measured_rows=VISION_ROI_BOTTOM-VISION_ROI_TOP+1;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)route_lane.row_width[y]=81;
    switch_a=0;
    for(unsigned i=0;i<80;++i){
        frame_step();
        if(vision_drive_is_local()&&info().target_right>=6&&info().target_left>=6&&
           info().pwm_right>=45&&info().pwm_left>=45)break;
    }
    CHECK(vision_drive_is_local()&&!info().faults);
    CHECK(info().pwm_right>=45&&info().pwm_left>=45);
    ++scenarios;
}
static void healthy(void)
{
    if(!vision_drive_is_local()||info().faults||vd.guard_faults)
        fprintf(stderr,"adverse scenario=%u ms=%lu faults=%lu detail=%u raw=%d/%d target=%d/%d pwm=%u/%u\n",
            scenarios,(unsigned long)now,(unsigned long)info().faults,info().fault_detail,
            info().raw3,info().raw4,info().target_right,info().target_left,info().pwm_right,info().pwm_left);
    CHECK(vision_drive_is_local()&&!info().faults&&!vd.guard_faults);
}
static void sample_ms(int left,int right,unsigned elapsed)
{
    raw3=(uint16_t)(raw3-left*(int)elapsed/10);raw4=(uint16_t)(raw4+right*(int)elapsed/10);now+=elapsed;
    app_mode_sample(switch_a,switch_b,now);vision_drive_safety_tick();
    if(app_mode_take_change())vd_route_switch_mode_stop();
    vision_drive_set_local_allowed(app_mode_allows(APP_MODE_DRIVE));vision_drive_poll(now);
    vision_drive_observe(&route_lane,++route_sequence,now,now);
}
static void stopped_fault(unsigned detail)
{
    CHECK((info().faults&VC_FAULT_ENCODER)&&info().fault_detail==detail);
    CHECK(!pwm[0]&&!pwm[1]);
    switch_wait(5,1);CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
}
int main(void)
{
    /* Field-sized launch fluctuation: 40ms at -5, then forward feedback. */
    launch();
    for(unsigned i=0;i<4;++i){sample(-5,-5);healthy();}
    for(unsigned i=0;i<20;++i){sample(15,15);healthy();}
    puts("PASS launch: four weak reverse samples recover without a latch");

    launch();sample(-20,-20);healthy();sample(15,15);healthy();
    for(unsigned i=0;i<8;++i){sample(i%2?-5:5,i%2?5:-5);healthy();}
    puts("PASS isolated strong spike and alternating quantized signs");

    /* A large, sustained reverse must keep the original fast fault. */
    launch();for(unsigned i=0;i<4&&!info().faults;++i)sample(-20,-20);
    stopped_fault(VS_FAULT_REVERSE_RIGHT);
    launch();for(unsigned i=0;i<4&&!info().faults;++i)sample(-20,15);
    stopped_fault(VS_FAULT_REVERSE_LEFT);

    /* Weak reverse cannot remain tolerated: bounded startup allowance only. */
    launch();unsigned weak=0;
    while(!info().faults&&weak<12){sample(-5,-5);++weak;}
    CHECK(weak>4&&weak<=10);stopped_fault(VS_FAULT_REVERSE_RIGHT);
    puts("PASS persistent reverse bounded; both wheel fault identities retained");

    launch();for(unsigned i=0;i<30;++i){sample(15,15);healthy();}
    for(unsigned i=0;i<4&&!info().faults;++i)sample(-5,-5);
    stopped_fault(VS_FAULT_REVERSE_RIGHT);
    puts("PASS established motion retains fast weak-reverse protection");

    /* Duplicate foreground work cannot count one sample more than once. */
    launch();
    for(unsigned i=0;i<4;++i){
        sample(-5,-5);
        for(unsigned j=0;j<30;++j){vision_drive_poll(now);healthy();}
    }
    sample(15,15);healthy();

    /* Explicit physical OFF/STOP resets a partial noise run, never arms. */
    launch();sample(-5,-5);sample(-5,-5);
    switch_a=1;switch_delta_left=switch_delta_right=0;switch_wait(10,1);
    CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_is_local());
    switch_a=0;
    for(unsigned i=0;i<80;++i){switch_step(1);if(info().pwm_right>=45&&info().pwm_left>=45)break;}
    for(unsigned i=0;i<4;++i){sample(-5,-5);healthy();}
    vision_drive_stop();
    for(unsigned i=0;i<20;++i)sample(-5,-5);
    CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
    puts("PASS duplicate work, OFF re-entry and explicit STOP");

    /* Large counter increments and stale samples must never be noise-filtered. */
    launch();sample(30000,30000);stopped_fault(VS_FAULT_DELTA);
    launch();now+=100;vision_drive_safety_tick();vision_drive_poll(now);
    CHECK(info().faults&VC_FAULT_ENCODER);CHECK(!pwm[0]&&!pwm[1]);
    puts("PASS impossible counter delta and missing-sample interval still stop");

    launch();for(unsigned i=0;i<3;++i){sample(-4,-4);healthy();}
    launch();for(unsigned i=0;i<3;++i){sample(-11,-11);healthy();}
    launch();for(unsigned i=0;i<4&&!info().faults;++i)sample(-12,-12);
    stopped_fault(VS_FAULT_REVERSE_RIGHT);
    puts("PASS weak (-4/-5) and strong (-11/-12) threshold boundaries");

    /* Legal interval variation is elapsed time, not an assumed 10ms tick. */
    launch();sample_ms(-5,-5,20);healthy();sample_ms(-5,-5,20);healthy();
    for(unsigned i=0;i<8;++i){sample_ms(15,15,i%2?20:10);healthy();}
    launch();unsigned reverse_ms=0;
    while(!info().faults&&reverse_ms<120){sample_ms(-5,-5,20);reverse_ms+=20;}
    CHECK(reverse_ms<=100);stopped_fault(VS_FAULT_REVERSE_RIGHT);
    puts("PASS 10/20ms jitter does not double the startup allowance");

    /* Exact duplicate observations are ignored. An out-of-order invalid
     * capture keeps the existing conservative zero-output response. */
    launch();
    {uint32_t sequence=vd.control.last_sequence,capture=vd.control.last_frame_ms;
     vision_result_t invalid=route_lane;invalid.valid=0;invalid.confidence=0;
     memset(invalid.row_flags,0,sizeof(invalid.row_flags));
     for(unsigned i=0;i<20;++i){vision_drive_observe(&invalid,sequence,capture,now);healthy();}
     vision_drive_observe(&invalid,sequence-1,capture-20,now);
     CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);}
    next_boot_ms=UINT32_MAX-180U;launch();
    for(unsigned i=0;i<4;++i){sample(-5,-5);healthy();}
    for(unsigned i=0;i<15;++i){sample(12,12);healthy();}
    puts("PASS duplicate/old frames and uint32 clock rollover");

    /* Deterministic, bounded startup disturbances across many independent
     * public-drive sessions. Includes zero/take-up, both signs and wrap at 0. */
    for(unsigned seed=1;seed<=64;++seed){
        unsigned random=seed;launch();
        for(unsigned i=0;i<4;++i){
            random=random*1664525U+1013904223U;int left=(int)(random%11U)-5;
            random=random*1664525U+1013904223U;int right=(int)(random%11U)-5;
            sample(left,right);healthy();
        }
        for(unsigned i=0;i<12;++i){sample(12,12);healthy();}
        CHECK(info().total3<0&&info().total4>0);
    }
    printf("ADVERSE INPUT PASS scenarios=%u checks=%u; constructed Host data, not a physical noise or vehicle acceptance model\n",scenarios,checks);
    return 0;
}
