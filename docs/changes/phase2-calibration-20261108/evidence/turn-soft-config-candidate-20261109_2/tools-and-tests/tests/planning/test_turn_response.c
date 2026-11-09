#include "../support/fixtures/response_io.h"
#include "app_link.h"
#if defined(__has_include)
#if __has_include("turn_config.h")
#include "turn_config.h"
#endif
#endif
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned channel,uint8_t byte){(void)channel;(void)byte;return 1;}
int main(void)
{
    vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];st_row_t row;
#ifdef TURN_CONFIG_H
    CHECK(turn_config.speed==60); /* Operating threshold is soft in configurable build. */
    setup(0);CHECK(turn_config_set(250,1500,60,1500,1600));
#endif
    setup(1);CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));
    setup(0);
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1501,1600,now));
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1550,now));
#ifndef TURN_CONFIG_H
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1560,now));
#endif
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,251,1000,1600,now));
    vf_init(&parser);parser.probe_permitted=1;
    for(const char *s="@motion circle 200 1000 1600\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,reply);
    CHECK(strstr(reply,"STARTED"));CHECK(pwm[0]==0&&pwm[1]==0&&pwm[2]==800);
    for(unsigned i=0;i<125;++i){tick(-10,11,1);CHECK(pwm[0]<=2000&&pwm[1]<=2000);}
    CHECK(!vision_drive_motion_active()&&!pwm[0]&&!pwm[1]&&!pwm[2]);freeze();
    {uint32_t cfg[ST_CONFIG_FIELDS];st_get_config(cfg);CHECK(cfg[9]==1600&&cfg[8]==0&&cfg[0]==200&&cfg[1]==1000);}
    CHECK(st_row(50,&row));CHECK(row.total_left!=row.total_right);
    setup(0);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1500,now));
    CHECK(pwm[2]==750);vision_drive_stop();freeze();
    setup(0);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));run_ticks(25,-5,5,1);
    tick(-61,61,1);
#ifdef TURN_CONFIG_H
    CHECK(!info().faults&&vision_drive_motion_active());CHECK(turn_effort_cap<200);
    CHECK(pwm[0]<=turn_effort_cap*10U&&pwm[1]<=turn_effort_cap*10U);
    tick(-121,61,1);
#endif
    CHECK(info().fault_detail==12&&!pwm[0]&&!pwm[1]);freeze();
    setup(0);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));run_ticks(60,0,0,0);
    CHECK(!vision_drive_motion_active()&&!pwm[0]&&!pwm[1]);freeze();
    setup(0);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));
    app_mode_sample(1,1,now);vision_drive_safety_tick();CHECK(!pwm[0]&&!pwm[1]);
#ifdef TURN_CONFIG_H
    setup(0);
    CHECK(!turn_config_set(601,1500,60,1500,1600));
    CHECK(!turn_config_set(250,3001,60,1500,1600));
    CHECK(!turn_config_set(250,1500,121,1500,1600));
    CHECK(!turn_config_set(250,1500,60,1400,1600));
    CHECK(turn_config_set(220,1800,80,1490,1610));
    CHECK(turn_config.request==220&&turn_config.duration==1800&&turn_config.speed==80);
    CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1700,1610,now));
    CHECK(!turn_config_set(220,1800,90,1490,1610));
    vision_drive_stop();freeze();turn_config_format(reply,sizeof(reply),1);
    CHECK(strstr(reply,"request=220 duration=1800 speed=80 servo=1490/1610"));
    CHECK(turn_config_set(250,1500,60,1500,1600));
    turn_config_format(reply,sizeof(reply),1);CHECK(strstr(reply,"speed=80"));
    vf_init(&parser);for(const char *s="@turn_config 210 1600 70 1500 1600\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,reply);
    CHECK(strstr(reply,"request=210 duration=1600 speed=70"));
    setup(0);CHECK(turn_config_set(250,1500,120,1500,1600));
    CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));
    run_ticks(25,-5,5,1);tick(-65,23,1);
    CHECK(!info().faults&&vision_drive_motion_active()); /* Prior nuisance trip no longer fires. */
    tick(-121,25,1);CHECK(info().fault_detail==12&&!pwm[0]&&!pwm[1]);freeze();
    setup(0);CHECK(turn_config_set(250,1500,60,1500,1600));
    turn_config_capture();turn_governor_sample(65,170);CHECK(turn_effort_cap==160);
    turn_governor_sample(65,170);CHECK(turn_effort_cap==150);
    turn_governor_sample(20,170);CHECK(turn_effort_cap==151);
    for(unsigned i=0;i<30;++i)turn_governor_sample(80,170);
    CHECK(turn_effort_cap==0);turn_governor_sample(20,170);CHECK(turn_effort_cap==1);
#endif
    printf("TURN RESPONSE PASS %u assertions; simulated IO only\n",checks);return 0;
}
