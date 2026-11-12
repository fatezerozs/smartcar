#include "IMG_DEAL.h"
#include "lane_centering.h"
#include "lane_geometry.h"
#include "ring_transition.h"
#include "brick_mask.h"
#include "obstacle_path.h"
#include "drive_tuning.h"
#include <stdio.h>
#include <math.h>
void RaceLine(void);extern Roundabout_position_enmum Roundabout_flag_position;
struct APEX{int Apex_Row,Apex_Col,Mark;};extern struct APEX left_apex,right_apex;
void tft180_show_int(int x,int y,int n,int l){(void)x;(void)y;(void)n;(void)l;}
void tft180_show_string(int x,int y,const char*s){(void)x;(void)y;(void)s;}
static uint8_t gray[120][188],mask[120][188];
static float err(void){long sum=0,ws=0;for(int i=0;i<120;i++)if(middleline[i]!=254&&mid_row[i]>=0&&mid_row[i]<120){int w=weight[119-mid_row[i]];sum+=(94-middleline[i])*w;ws+=w;}return ws?(float)sum/ws:0;}
int main(void){int fails=0;
 for(int i=0;i<240;i++){left.Row[i]=right.Row[i]=mid_row[i]=left.Col[i]=right.Col[i]=middleline[i]=254;}
 for(int i=0;i<21;i++){left.Row[i]=right.Row[i]=110-i;left.Col[i]=44;right.Col[i]=144;}
 L_lenth=R_lenth=21;left_flag=right_flag=1;left_apex.Mark=right_apex.Mark=95;memset(IMG_DATA,255,sizeof(IMG_DATA));
 Roundabout_flag=GET_IN_READY;Roundabout_flag_position=ROUND_L;RaceLine();lane_geometry_t g={0};g.valid=1;ring_transition_t s={0};
 ring_target_t a=ring_transition_target(&s,ROUND_READY,GET_IN_READY,ROUND_L,0,1,0,1,1,&g,err());printf("PRE_ENTRY_CENTERED error=%.2f us=%.0f\n",(double)a.error,(double)steer_target_us(a.error));if(fabsf(a.error)>2)fails++;
 g.model_used=1;g.error=g.near_error=0;Roundabout_flag=GET_OUT_ROUND;RaceLine();a=ring_transition_target(&s,IN_ROUND,GET_OUT_ROUND,ROUND_L,0,0,0,0,1,&g,err());printf("CURRENT_EXIT_OUTER error=%.2f us=%.0f\n",(double)a.error,(double)steer_target_us(a.error));if(fabsf(a.error)>2)fails++;
 g.error=36;g.near_error=0;a=ring_transition_target(&s,OUT_ROUND,NO_ROUND,ROUND_L,0,0,0,0,1,&g,36);printf("EXIT_HANDOFF_NEAR_CENTERED error=%.2f us=%.0f\n",(double)a.error,(double)steer_target_us(a.error));if(fabsf(a.error)>8.1f)fails++;
 for(int kind=0;kind<3;kind++){
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)gray[y][x]=(uint8_t)(x>=22&&x<=166?220:30);
 for(int y=kind==2?55:65;y<=88;y++)for(int x=100;x<=128;x++)gray[y][x]=(uint8_t)(kind==1?210:100);
 obstacle_path_reset();brick_mask(gray,mask);obstacle_path_t op;obstacle_path(mask,&op);
 int allowed=obstacle_may_override(kind==0?GET_IN_READY:NO_ROUND,kind==0?GET_IN_READY:NO_ROUND,NO_ROUND);
 printf("BRICK kind=%d detected=%u allowed=%d\n",kind,op.active,allowed);if(!op.active||!allowed)fails++;
 }
 printf("SWING_BRICK_BEFORE failures=%d\n",fails);return fails?1:0;}
