#include "IMG_DEAL.h"
#include "lane_centering.h"
#include "lane_geometry.h"
#include "ring_transition.h"
#include "brick_mask.h"
#include "obstacle_path.h"
#include "drive_tuning.h"
#include "brick_candidates.h"
#include "exit_lane.h"
#include <stdio.h>
#include <math.h>
void RaceLine(void);extern Roundabout_position_enmum Roundabout_flag_position;
struct APEX{int Apex_Row,Apex_Col,Mark;};extern struct APEX left_apex,right_apex;
void tft180_show_int(int x,int y,int n,int l){(void)x;(void)y;(void)n;(void)l;}
void tft180_show_string(int x,int y,const char*s){(void)x;(void)y;(void)s;}
static uint8_t gray[120][188],mask[120][188],bright[120][188];
static float err(void){long sum=0,ws=0;for(int i=0;i<120;i++)if(middleline[i]!=254&&mid_row[i]>=0&&mid_row[i]<120){int w=weight[119-mid_row[i]];sum+=(94-middleline[i])*w;ws+=w;}return ws?(float)sum/ws:0;}
int main(void){int fails=0;
 for(int i=0;i<240;i++){left.Row[i]=right.Row[i]=mid_row[i]=left.Col[i]=right.Col[i]=middleline[i]=254;}
 for(int i=0;i<21;i++){left.Row[i]=right.Row[i]=110-i;left.Col[i]=44;right.Col[i]=144;}
 L_lenth=R_lenth=21;left_flag=right_flag=1;left_apex.Mark=right_apex.Mark=95;memset(IMG_DATA,255,sizeof(IMG_DATA));
 Roundabout_flag=GET_IN_READY;Roundabout_flag_position=ROUND_L;RaceLine();lane_geometry_t g={0};g.valid=1;ring_transition_t s={0};
 ring_target_t a=ring_transition_target(&s,ROUND_READY,GET_IN_READY,ROUND_L,0,1,0,1,1,&g,err());printf("PRE_ENTRY_CENTERED error=%.2f us=%.0f\n",(double)a.error,(double)steer_target_us(a.error));if(fabsf(a.error)>2)fails++;
 g.model_used=1;g.error=g.near_error=0;Roundabout_flag=GET_OUT_ROUND;RaceLine();a=ring_transition_target(&s,IN_ROUND,GET_OUT_ROUND,ROUND_L,0,0,0,0,1,&g,err());printf("CURRENT_EXIT_OUTER error=%.2f us=%.0f\n",(double)a.error,(double)steer_target_us(a.error));if(fabsf(a.error)>2)fails++;
 g.error=36;g.near_error=0;a=ring_transition_target(&s,OUT_ROUND,NO_ROUND,ROUND_L,0,0,0,0,1,&g,36);printf("EXIT_HANDOFF_NEAR_CENTERED error=%.2f us=%.0f\n",(double)a.error,(double)steer_target_us(a.error));if(fabsf(a.error)>.01f)fails++;
 for(int kind=0;kind<3;kind++){
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)gray[y][x]=(uint8_t)(x>=22&&x<=166?220:30);
 for(int y=kind==2?55:65;y<=88;y++)for(int x=100;x<=128;x++)gray[y][x]=(uint8_t)(kind==1?210:100);
 obstacle_path_reset();int ok=brick_mask(gray,mask),bk=brick_bright_mask(gray,bright);obstacle_path_t op;unsigned variant;brick_paths(mask,ok,bright,bk,&op,&variant);
 int allowed=brick_stage_allowed(kind==0?GET_IN_READY:NO_ROUND,kind==0?GET_IN_READY:NO_ROUND);
 printf("BRICK kind=%d detected=%u allowed=%d\n",kind,op.active,allowed);if(!op.active||!allowed)fails++;
 }
 /* Current white exit road still works when original edge tracer is absent. */
 lane_width_model_t widths={0};
 for(int y=0;y<120;y++){widths.width[y]=100;widths.samples[y]=5;for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(x>=44&&x<=144?255:0);}
 int mask_seen=exit_lane_from_mask(mask,&widths,weight,&g);
 if(!mask_seen||fabsf(g.near_error)>.01f)fails++;
 memset(&s,0,sizeof(s));a=ring_transition_target(&s,IN_ROUND,GET_OUT_ROUND,ROUND_L,0,0,0,mask_seen,mask_seen,&g,25.52f);
 if(a.source!=5||fabsf(a.error)>.01f)fails++;
 /* Recovered real paired bends retain their measured preview. */
 a=ring_transition_target(&s,OUT_ROUND,NO_ROUND,ROUND_L,0,1,15,1,1,&g,40);if(a.source!=8||a.error!=15)fails++;
 for(int f=0;f<20;f++)ring_transition_target(&s,NO_ROUND,NO_ROUND,ROUND_L,0,0,0,1,1,&g,40);
 a=ring_transition_target(&s,NO_ROUND,NO_ROUND,ROUND_L,0,0,0,1,1,&g,40);if(a.source||a.error!=40)fails++;
 g.valid=0;memset(mask,255,sizeof(mask));if(exit_lane_from_mask(mask,&widths,weight,&g))fails++;
 memset(mask,0,sizeof(mask));if(exit_lane_from_mask(mask,&widths,weight,&g))fails++;
 memset(&widths,0,sizeof(widths));if(exit_lane_from_mask(mask,&widths,weight,&g))fails++;
 for(int before=0;before<=8;before++)for(int after=0;after<=8;after++){
 int expected=(before==0||before==1||before==8)&&(after==0||after==1||after==8);
 if(brick_stage_allowed(before,after)!=expected)fails++;
 if(brick_rejects_ring_candidate(before,after,1)!=(expected&&after!=8))fails++;
 }
 if(fabsf(TARGET_SPEED_MPS-1.4f)>.001f)fails++;
 if(route_cruise_speed(1.4f,NO_ROUND,1550)!=1.4f)fails++;
 if(fabsf(route_cruise_speed(1.4f,NO_ROUND,1670)-.9f)>.001f)fails++;
 if(route_cruise_speed(1.4f,IN_ROUND,1550)>1.0f)fails++;
 g.valid=1;g.clearance=.5f;if(fabsf(edge_speed_limit(1.4f,&g)-1.4f)>.001f)fails++;
 g.clearance=.2f;if(fabsf(edge_speed_limit(1.4f,&g)-.6f)>.001f)fails++;
 /* Ring interiors, broad shadow and no-road stay negative in BOTH masks. */
 for(int kind=0;kind<4;kind++){
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)gray[y][x]=(uint8_t)(x>=22&&x<=166?220:30);
 if(kind==0)for(int y=45;y<=110;y++)for(int x=40;x<145;x++){float dx=x-94,dy=y-76;if(dx*dx/(20*20)+dy*dy/(10.2f*10.2f)<=1)gray[y][x]=150;}
 if(kind==1)for(int y=65;y<90;y++)memset(gray[y],30,188);
 if(kind==2)memset(gray,150,sizeof(gray));
 if(kind==3)for(int y=45;y<100;y++)for(int x=75;x<110;x++)gray[y][x]=30;
 obstacle_path_reset();int ok=brick_mask(gray,mask),bk=brick_bright_mask(gray,bright);obstacle_path_t op;unsigned variant;brick_paths(mask,ok,bright,bk,&op,&variant);if(op.active)fails++;
 }
 /* Actual donor image processing followed by the production brick gate. */
 for(int level=100;level<=210;level+=110){
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)gray[y][x]=(uint8_t)(x>=22&&x<=166?220:30);
 for(int y=65;y<=88;y++)for(int x=100;x<=128;x++)gray[y][x]=(uint8_t)level;
 obstacle_path_reset();int ok=brick_mask(gray,mask),bk=brick_bright_mask(gray,bright);
 memcpy(IMG_DATA,gray,sizeof(gray));Roundabout_flag=NO_ROUND;int before=Roundabout_flag;Img_Deal();int after=Roundabout_flag;
 obstacle_path_t op;unsigned variant=0;memset(&op,0,sizeof(op));
 if(brick_stage_allowed(before,after))brick_paths(mask,ok,bright,bk,&op,&variant);
 printf("ACTUAL_BRICK_PIPELINE gray=%d before=%d after=%d active=%u variant=%u\n",level,before,after,op.active,variant);
 if(!op.active||op.blocked)fails++;
 }
 /* The bright-mask fallback must preserve current-object tracking when
  * its primary-mask attempt fails and when the object clips near camera. */
 obstacle_path_reset();
 for(int stage=0;stage<3;stage++){
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)gray[y][x]=(uint8_t)(x>=22&&x<=166?220:30);
 for(int y=stage==0?65:stage==1?88:108;y<(stage==0?89:120);y++)for(int x=stage==0?100:126;x<=(stage==0?128:152);x++)gray[y][x]=210;
 int ok=brick_mask(gray,mask),bk=brick_bright_mask(gray,bright);obstacle_path_t op;unsigned variant;
 brick_paths(mask,ok,bright,bk,&op,&variant);if(!op.active||op.blocked||variant!=2)fails++;
 }
 {ring_transition_t sp={0};g.valid=0;
 float v=ring_transition_speed(&sp,1.4f,&g,NO_ROUND,0);if(v!=1.4f)fails++;
 v=ring_transition_speed(&sp,1,&g,GET_IN_ROUND,10);if(v>1.001f)fails++;
 sp.recover_frames=15;float prev=v;v=ring_transition_speed(&sp,1.4f,&g,NO_ROUND,20);if(v-prev>.0061f)fails++;
 if(ring_transition_speed(&sp,0,&g,NO_ROUND,30)!=0)fails++;
 if(ring_transition_speed(&sp,1.4f,&g,NO_ROUND,40)!=1.4f)fails++;
 }
 printf("SWING_BRICK_AFTER failures=%d\n",fails);return fails?1:0;}
