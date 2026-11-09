#include "main.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned owner,beeps;static uint32_t tick;static uint8_t tx[22656],gray[22560];static unsigned used;
uint32_t timer_get(unsigned x){(void)x;return tick;}void st_test_marker(uint32_t n){(void)n;beeps++;}unsigned app_transport_owner(void){return owner;}
void vision_drive_get_profile(vd_profile_t *p){p->control.near_y=.78f;p->control.far_y=.46f;}
const vc_output_t *vision_drive_output(void){static vc_output_t o;return &o;}
int vc_diagnostic_band(const vision_result_t *r,const vc_config_t *c,float y){(void)r;(void)c;return y<.5f;}
void uart_write_buffer(unsigned u,const uint8_t *p,unsigned n){(void)u;assert(used+n<=sizeof(tx));memcpy(tx+used,p,n);used+=n;}
int main(void){char out[768];vision_result_t r={0,0,3};unsigned i;FILE *f;
vq_command("vq_start",out,sizeof out,0,1000);assert(strstr(out,"REJECT"));owner=1;vq_command("vq_start",out,sizeof out,1,1000);assert(strstr(out,"REJECT"));owner=0;
vq_command("vq_start",out,sizeof out,1,1000);assert(strstr(out,"START"));vq_record(gray,&r,0,999,999,999,999,0);vq_command("vq_info",out,sizeof out,1,1000);assert(strstr(out,"count=0 active=1"));
vq_command("vq_frame 0",out,sizeof out,1,1000);assert(strstr(out,"REJECT"));vq_command("vq_clear 1",out,sizeof out,1,1000);assert(strstr(out,"REJECT"));
for(i=0;i<750;i++){tick=1004+i*20;vq_poll(tick,1);memset(gray,i%255,sizeof gray);vq_record(gray,&r,i+1,1000+i*20,1001+i*20,1002+i*20,1003+i*20,0);}
vq_command("vq_info",out,sizeof out,1,tick);assert(strstr(out,"count=750 active=0 reason=1"));assert(beeps==3);vq_record(gray,&r,9999,tick,tick,tick,tick,0);
vq_command("vq_frame 0x",out,sizeof out,1,tick);assert(strstr(out,"REJECT"));vq_command("vq_frame 750",out,sizeof out,1,tick);assert(strstr(out,"REJECT"));
vq_command("vq_frame 0",out,sizeof out,1,tick);assert(used==22656);f=fopen("native-frame0.vqf","wb");assert(f);assert(fwrite(tx,1,used,f)==used);fclose(f);
vq_command("vq_clear 2",out,sizeof out,1,tick);assert(strstr(out,"REJECT"));vq_command("vq_clear 1",out,sizeof out,1,tick);assert(strstr(out,"CLEARED"));
vq_command("vq_start",out,sizeof out,1,0xfffffff0U);vq_poll(0x7520U,1);vq_command("vq_info",out,sizeof out,1,0);assert(strstr(out,"reason=2"));vq_command("vq_clear 2",out,sizeof out,1,0);vq_command("vq_start",out,sizeof out,1,0);vq_poll(1,0);vq_command("vq_info",out,sizeof out,1,0);assert(strstr(out,"reason=3"));
puts("PASS: gates, capacity, cues, retention, strict indices, generation clear, wrap timeout, mode abort, frame export");return 0;}
