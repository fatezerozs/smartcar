#include "IMG_DEAL.h"
#include "lane_centering.h"
#include "lane_geometry.h"
#include "ring_transition.h"
#include "drive_tuning.h"
#include <stdio.h>
#include <math.h>
void RaceLine(void);
extern Roundabout_position_enmum Roundabout_flag_position;
struct APEX{int Apex_Row,Apex_Col,Mark;};extern struct APEX left_apex,right_apex;
void tft180_show_int(int x,int y,int n,int l){(void)x;(void)y;(void)n;(void)l;}
void tft180_show_string(int x,int y,const char*s){(void)x;(void)y;(void)s;}
static unsigned checks,fails;
#define CHECK(c) do{checks++;if(!(c)){fails++;printf("TRANSITION_FAIL line=%d\n",__LINE__);}}while(0)
static uint8_t mask[120][188];static lane_width_model_t model;static lane_geometry_t geo;
static lane_centering_t pair;
static float error(void){long sum=0,ws=0;if(middleline[0]==254)return 0;for(int i=0;i<120;i++)if(middleline[i]!=254&&mid_row[i]>=0&&mid_row[i]<120){int w=weight[119-mid_row[i]];sum+=(94-middleline[i])*w;ws+=w;}return ws?(float)sum/ws:0;}
static void scene(int width,int shift){
 for(int i=0;i<240;i++){left.Row[i]=right.Row[i]=mid_row[i]=254;left.Col[i]=right.Col[i]=middleline[i]=254;}
 for(int i=0;i<21;i++){left.Row[i]=right.Row[i]=110-i;left.Col[i]=94+shift-width/2;right.Col[i]=94+shift+width/2;}
 L_lenth=R_lenth=21;
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(x>=94+shift-width/2&&x<=94+shift+width/2?255:0);
 memset(IMG_DATA,255,sizeof(IMG_DATA));left_flag=right_flag=1;left_apex.Mark=right_apex.Mark=95;
}
int main(void){
 for(int side=0;side<2;side++)for(int stage=GET_OUT_ROUND;stage<=OUT_ROUND;stage++)for(int shift=-8;shift<=8;shift+=8){
 ring_transition_t s={0};scene(60,shift);Roundabout_flag=(Roundabout_enmum)stage;Roundabout_flag_position=side==0?ROUND_L:ROUND_R;RaceLine();float old=error();
 int pv=paired_lane_center(left.Row,left.Col,right.Row,right.Col,mask,weight,&pair);
 lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&geo);
 CHECK(pv&&geo.valid);
 ring_target_t a=ring_transition_target(&s,IN_ROUND,stage,side,0,pv,pair.error,1,1,&geo,old);CHECK(a.source==9&&fabsf(a.error+shift)<.01f);
 a=ring_transition_target(&s,stage,stage,side,0,pv,pair.error,1,1,&geo,old);CHECK(a.source==4);CHECK(fabsf(a.error+shift)<.01f);
 if(!shift){CHECK(steer_target_us(a.error)==1550);if(stage==OUT_ROUND)printf("ACTUAL_EXIT side=%d donor_error=%.2f old_us=%.0f new_error=%.2f new_us=%.0f\n",side,(double)old,(double)steer_target_us(old),(double)a.error,(double)steer_target_us(a.error));}
 else CHECK(a.error*shift<0);
 }
 /* Tentative ring and direct 8->0 transition keep the measured tangent center. */
 for(int before=0;before<=8;before+=8){ring_transition_t s={0};scene(100,0);Roundabout_flag=ROUND_READY;RaceLine();float old=error();int pv=paired_lane_center(left.Row,left.Col,right.Row,right.Col,mask,weight,&pair);lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&geo);
 ring_target_t a=ring_transition_target(&s,before,ROUND_READY,0,0,pv,pair.error,1,1,&geo,old);CHECK(a.source==3&&fabsf(a.error)<.01f);printf("ACTUAL_TANGENT before=%d old=%.2f new=%.2f\n",before,(double)old,(double)a.error);}
 /* Every committed entry case retains the exact legacy command, including
  * the intentional 0/188 guide extremes and cases with a straight pair. */
 for(int before=0;before<=8;before++)for(int stage=GET_IN_ROUND;stage<=IN_ROUND;stage++)for(int err=-94;err<=94;err++){
 ring_transition_t s={0};geo.valid=1;ring_target_t a=ring_transition_target(&s,before,stage,0,0,1,0,1,1,&geo,(float)err);CHECK(a.source==0&&a.error==(float)err);}
 /* Never acquire an exit without current geometry, stable pairs or the
  * original FSM exit state. Avoidance keeps its priority. */
 for(int stage=0;stage<=8;stage++){
 ring_transition_t s={0};geo.valid=0;
 for(int f=0;f<5;f++){ring_target_t a=ring_transition_target(&s,stage,stage,0,0,1,0,1,1,&geo,42);CHECK(a.source==0&&a.error==42);}
 geo.valid=1;ring_target_t a=ring_transition_target(&s,stage,stage,0,1,1,0,1,1,&geo,42);CHECK(a.source==0&&a.error==42);
 }
 {ring_transition_t s={0};geo.valid=1;geo.model_used=0;ring_target_t a;
 for(int f=0;f<2;f++)a=ring_transition_target(&s,OUTTING,OUTTING,ROUND_L,0,1,0,1,1,&geo,40);CHECK(a.source==4);
 geo.model_used=1;geo.error=geo.near_error=-4;a=ring_transition_target(&s,OUTTING,OUT_ROUND,ROUND_L,0,0,0,0,1,&geo,40);CHECK(a.source==5&&a.error==-4);
 a=ring_transition_target(&s,OUT_ROUND,OUT_ROUND,ROUND_L,0,0,0,1,0,&geo,40);CHECK(a.source==0&&a.error==40);
 geo.valid=0;a=ring_transition_target(&s,OUT_ROUND,OUT_ROUND,ROUND_L,0,0,0,0,1,&geo,40);CHECK(a.source==0&&a.error==40);
 geo.valid=1;ring_transition_target(&s,OUT_ROUND,NO_ROUND,ROUND_L,0,1,0,1,1,&geo,40);CHECK(!s.exit_current);
 a=ring_transition_target(&s,NO_ROUND,OUT_ROUND,ROUND_L,0,1,0,1,1,&geo,40);CHECK(a.source==0);
 }
 /* OUT_WAY/OUT_ROUND already qualify as donor exit. A current outer
  * edge plus learned width need not wait for both sides to reappear. */
 for(int side=0;side<2;side++)for(int stage=OUT_WAY;stage<=OUT_ROUND;stage++){
 ring_transition_t s={0};geo.valid=1;geo.model_used=1;geo.near_error=0;geo.error=60;
 ring_target_t a=ring_transition_target(&s,OUTTING,stage,side,0,0,0,side==ROUND_R,side==ROUND_L,&geo,60);
 CHECK(a.source==5&&a.error==0);CHECK(fabsf(steer_target_us(a.error)-1550)<25);
 }
 for(int near=-80;near<=80;near++)for(int far=-100;far<=100;far++){
 geo.near_error=(float)near;geo.error=(float)far;float v=ring_exit_single_error(&geo);
 CHECK(fabsf(v-near)<=8.001f);
 }
 /* Expansion no longer drops approach to .70; rate changes are bounded;
  * OFF and zebra request zero immediately; normal startup remains 1 m/s. */
 {ring_transition_t s={0};geo.valid=0;float v=ring_transition_speed(&s,1,&geo,NO_ROUND,0);CHECK(v==1);
 geo.valid=1;geo.clearance=.30f;
 for(uint32_t now=10;now<=1000;now+=10){float prev=v;v=ring_transition_speed(&s,1,&geo,ROUND_READY,now);CHECK(v>=.8499f&&v<=1);CHECK(prev-v<=.0081f);}
 printf("APPROACH_MARGIN_030 final_mps=%.2f V8_mps=%.2f\n",(double)v,(double)edge_speed_limit(1,&geo));
 CHECK(ring_transition_speed(&s,0,&geo,ROUND_READY,1010)==0);geo.valid=0;CHECK(ring_transition_speed(&s,1,&geo,NO_ROUND,1020)==1);
 geo.valid=1;geo.clearance=.10f;
 for(uint32_t now=1030;now<=3030;now+=10){float prev=s.speed;v=ring_transition_speed(&s,1,&geo,IN_ROUND,now);CHECK(v>=.5999f&&v<=1);CHECK(prev-v<=.0251f);}
 CHECK(fabsf(v-.6f)<.0001f);geo.valid=0;
 for(uint32_t now=3040;now<=5040;now+=10){float prev=s.speed;v=ring_transition_speed(&s,1,&geo,IN_ROUND,now);CHECK(v-prev<=.0061f);}
 CHECK(fabsf(v-1)<.0001f);CHECK(ring_transition_speed(&s,0,&geo,OUTTING,5050)==0);
 CHECK(ring_transition_speed(&s,.55f,&geo,ROUND_READY,5060)<=.55f);
 }
 printf("RING_TRANSITION %u checks, %u failures; actual donor entry/exit/tangent and approach speed\n",checks,fails);return fails?1:0;}
