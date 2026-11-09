/* Run existing response protections against the actual instrumented adapters. */
#define main baseline_response_main
#include "test_speed_response.c"
#undef main
#include "heartbeat_diag.h"
#include "app_link.h"
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned channel,uint8_t byte){(void)channel;(void)byte;return 1;}
int main(void)
{
    char out[VF_RESPONSE_CAPACITY],saved[VF_RESPONSE_CAPACITY];
    hd_event_t e={0};hd_state_t live={0},snapshot={0};
    CHECK(baseline_response_main()==0);
    /* Modulo timer rollover and frozen snapshot independence. */
    hd_event(&e,0xfffffff0U);hd_event(&e,0x10U);CHECK(e.count==2&&e.max_gap_ms==32);
    live.accepted=e;hd_capture(&snapshot,&live,0x20,0x10,3);
    hd_event(&live.accepted,0x30);CHECK(snapshot.accepted.count==2&&snapshot.reason==3);
    app_link_init();setup(0);
#ifdef HD_QUIET_CONTROL
    CHECK(hd_quiet==0);
    {vf_parser_t parser;vf_init(&parser);
     for(const char *s="@heartbeat_quiet 1\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,out);}
    CHECK(hd_quiet==1&&strstr(out,"[HB_QUIET] value=1"));
    CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_motion_active());
#endif
    CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));
    app_link_rx_isr(APP_LINK_USB,'h');CHECK(hd_live.rx.count==0);
    app_link_rx_isr(APP_LINK_RADIO,'h');CHECK(hd_live.rx.count==1);
    app_link_rx_isr(APP_LINK_RADIO,'H');CHECK(hd_live.rx.count==2);
    /* Actual lease write, then natural timeout. No diagnostics extend a lease. */
    now+=10;CHECK(vision_drive_command('h',now,1));CHECK(hd_live.accepted.count==1);
    run_ticks(60,0,0,0);CHECK(!vision_drive_motion_active());
    hd_format(out,sizeof(out));CHECK(strstr(out,"frozen=1 reason=3"));
#ifdef HD_QUIET_CONTROL
    CHECK(strstr(out,"quiet=1"));
    {vf_parser_t parser;vf_init(&parser);
     for(const char *s="@heartbeat_quiet 0\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,out);}
    CHECK(!hd_quiet);hd_format(out,sizeof(out));CHECK(strstr(out,"quiet=1"));
#endif
    CHECK(strstr(out,"accepted_h=1/"));strcpy(saved,out);
    CHECK(!pwm[0]&&!pwm[1]);vision_drive_stop();hd_format(out,sizeof(out));CHECK(!strcmp(saved,out));
    /* Query through the real parser preserves the completed snapshot. */
    {vf_parser_t parser;vf_init(&parser);
     for(const char *s="@heartbeat_diag\n";*s;++s)(void)vf_feed(&parser,(uint8_t)*s,now,out);}
    CHECK(!strcmp(saved,out));
    /* A new admission resets session counters; frozen response must first release. */
    freeze();CHECK(st_release(trace_info().generation));now+=2100;
    CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now)==0); /* stale encoder sample */
    tick(0,0,0);CHECK(vision_drive_motion_start(VD_MOTION_LINE,200,3000,1550,now));
    CHECK(!hd_live.rx.count&&!hd_live.accepted.count);hd_format(out,sizeof(out));CHECK(strstr(out,"frozen=0"));
    vision_drive_stop();printf("HEARTBEAT DIAG PASS %u assertions; simulated IO only\n",checks);return 0;
}
