#include "../support/fixtures/response_io.h"
#include "app_link.h"
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned channel,uint8_t byte){(void)channel;(void)byte;return 1;}
int main(void)
{
    vf_parser_t parser;char reply[VF_RESPONSE_CAPACITY];st_row_t row;
    setup(1);CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));
    setup(0);
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1501,1600,now));
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1550,now));
    CHECK(!vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1560,now));
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
    tick(-61,61,1);CHECK(info().fault_detail==12&&!pwm[0]&&!pwm[1]);freeze();
    setup(0);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));run_ticks(60,0,0,0);
    CHECK(!vision_drive_motion_active()&&!pwm[0]&&!pwm[1]);freeze();
    setup(0);CHECK(vision_drive_motion_start(VD_MOTION_CIRCLE,200,1000,1600,now));
    app_mode_sample(1,1,now);vision_drive_safety_tick();CHECK(!pwm[0]&&!pwm[1]);
    printf("TURN RESPONSE PASS %u assertions; simulated IO only\n",checks);return 0;
}
