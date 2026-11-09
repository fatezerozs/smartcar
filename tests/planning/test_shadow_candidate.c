/* Real private candidate/field/PI modules; timers, counters and PWM are fake. */
#include "../support/fixtures/switch_io.h"
#include "vision_drive_internal.h"
#include "vision_planner_shadow_diag.h"
#include "phase3_shadow_profile.h"
static uint32_t fake_cycles;
uint32_t vision_test_shadow_cycles(void){fake_cycles+=100;return fake_cycles;}
void vision_test_output_hook(unsigned stage){(void)stage;}
static char reply[VF_RESPONSE_CAPACITY];
static void command(const char *s)
{
    vf_parser_t parser;vf_init(&parser);CHECK(vf_feed(&parser,'@',now,reply)==1);
    while(*s){CHECK(vf_feed(&parser,(uint8_t)*s++,now,reply)==1);}
    CHECK(vf_feed(&parser,'\n',now,reply)==2);
}
static void boot(void)
{
    switch_boot(1);switch_wait(10,1);set_lane(17);switch_delta_left=-1;switch_delta_right=1;
    CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]);
}
static void waveform(uint16_t *out,int load)
{
    boot();if(load){command("planner_load");CHECK(strstr(reply,"OK")&&strstr(reply,"mode=SHADOW"));}
    CHECK(vd_speed_source()==VD_SPEED_LEGACY);
    CHECK(info().kp_q8==256&&info().ki_q8==12); /* Original PI, not measured test gains. */
    switch_wait(10,1); /* Field commands STOP; observe the original stable OFF before ON. */
    switch_a=0;switch_wait(40,1);CHECK(vision_drive_is_local());
    for(unsigned j=0;j<100;++j){switch_step(1);out[j*2]=(uint16_t)pwm[0];out[j*2+1]=(uint16_t)pwm[1];}
}
int main(void)
{
    uint16_t baseline[200],shadow[200];waveform(baseline,0);waveform(shadow,1);
    CHECK(!memcmp(baseline,shadow,sizeof(baseline)));
    CHECK(vp_config_valid(&phase3_shadow_configuration.planning.planner));
    vdr_info_t s;vision_drive_planner_info(&s);CHECK(s.configured&&s.mode==VDR_SHADOW&&s.plans>0);
    printf("SHADOW CANDIDATE plans=%lu source=%u; identical legacy PWM sequence\n",(unsigned long)s.plans,vd_speed_source());
    /* Direct service calls cannot sneak in a query/load during a run. */
    CHECK(vd_shadow_diag_field("planner_load",reply,sizeof(reply))&&strstr(reply,"REJECT stop first"));
    CHECK(vd_shadow_diag_field("planner_bench",reply,sizeof(reply))&&strstr(reply,"REJECT stop first"));
    CHECK(vision_drive_is_local()&&vd_speed_source()==VD_SPEED_LEGACY);
    for(unsigned j=0;j<300;++j)switch_step(1);
    command("planner");CHECK(!pwm[0]&&!pwm[1]&&strstr(reply,"configured=1")&&strstr(reply,"metric_active=0"));
    CHECK(strstr(reply,"count=256")&&strstr(reply,"frozen=1")&&!strstr(reply,"overwritten=0 "));
    CHECK(strstr(reply,"cycles=1")&&strstr(reply,"source=0"));
    command("planner_row 0");CHECK(strstr(reply,"[PLANNER_ROW] run=1 i=0")&&strstr(reply,"poll_cycles="));
    char saved[VF_RESPONSE_CAPACITY];strcpy(saved,reply);
    command("planner_load");CHECK(strstr(reply,"REJECT"));
    command("planner_row 0");CHECK(!strcmp(saved,reply));
    vd_speed_info_t before=info();vdr_info_t planner_before;vision_drive_planner_info(&planner_before);
    CHECK(vd_shadow_diag_field("planner_bench",reply,sizeof(reply))&&strstr(reply,"[PLANNER_BENCH] PASS"));
    CHECK(strstr(reply,"synthetic=1")&&strstr(reply,"cases=4 ticks=256 periods_ms=10,20"));
    CHECK(strstr(reply,"ready=238 zero=18 checks=250 expired_rejected=238"));printf("%s",reply);
    vd_speed_info_t after=info();vdr_info_t planner_after;vision_drive_planner_info(&planner_after);
    CHECK(!memcmp(&before,&after,sizeof(before))&&!memcmp(&planner_before,&planner_after,sizeof(planner_before)));
    command("planner_row 0");CHECK(!strcmp(saved,reply));
    command("planner_row 256");CHECK(strstr(reply,"REJECT"));
    command("planner_row -1");CHECK(strstr(reply,"REJECT"));
    command("planner_row 0x1");CHECK(strstr(reply,"REJECT"));
    command("planner_clear");CHECK(strstr(reply,"OK"));command("planner");CHECK(strstr(reply,"count=0"));
    CHECK(!vd_shadow_diag_field("status",reply,sizeof(reply)));
    CHECK(vd_speed_source()==VD_SPEED_LEGACY&&!pwm[0]&&!pwm[1]);
    printf("SHADOW CANDIDATE PASS %u checks; simulated IO and cycle counter\n",checks);return 0;
}
