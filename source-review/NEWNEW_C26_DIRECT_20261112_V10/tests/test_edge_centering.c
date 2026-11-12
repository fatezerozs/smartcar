#include "IMG_DEAL.h"
#include "lane_centering.h"
#include "lane_geometry.h"
#include <math.h>
#include <stdio.h>
struct APEX{int Apex_Row,Apex_Col,Mark;};extern struct APEX left_apex,right_apex;
void Racing_Line(void);
void tft180_show_int(int x,int y,int n,int l){(void)x;(void)y;(void)n;(void)l;}
void tft180_show_string(int x,int y,const char*s){(void)x;(void)y;(void)s;}
static uint8_t mask[120][188];static lane_width_model_t model;
static lane_geometry_t g;static unsigned checks,failures;
#define CHECK(c) do{checks++;if(!(c)){failures++;printf("EDGE_FAIL %d\n",__LINE__);}}while(0)
static void scene(int shift,int side){
 for(int i=0;i<240;i++){left.Row[i]=right.Row[i]=254;left.Col[i]=right.Col[i]=254;middleline[i]=mid_row[i]=254;}
 for(int i=0;i<110;i++){
  if(side!=2){left.Row[i]=115-i;left.Col[i]=44+shift;}
  if(side!=1){right.Row[i]=115-i;right.Col[i]=144+shift;}
 }
 for(int i=0;i<120;i++)weight[i]=1;
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(x>=44+shift&&x<=144+shift?255:0);
 for(int i=0;i<70;i++){middleline[i]=94+shift;mid_row[i]=110-i;}
}
int main(void){
scene(0,0);for(int f=0;f<5;f++)lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,1,&g);
CHECK(g.valid&&model.samples[100]==5);CHECK(fabsf(model.width[100]-100)<.01f);
for(int side=1;side<=2;side++){
 int shift=side==1?-10:10;scene(shift,side);
 left_flag=side!=2;right_flag=side!=1;left_apex.Mark=right_apex.Mark=95;
 memset(IMG_DATA,255,sizeof(IMG_DATA));Racing_Line();
 float sum=0;int n=0;
 for(int i=0;i<120;i++)if(mid_row[i]>=90&&mid_row[i]<=110&&middleline[i]!=254){sum+=94-middleline[i];n++;}
 float old=n?sum/n:0;
 lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);
 printf("CURRENT_EDGE side=%d donor_near=%.2f new_error=%.2f near=%.2f model=%d\n",side,(double)old,(double)g.error,(double)g.near_error,g.model_used);
 CHECK(g.valid&&g.model_used);CHECK(fabsf(g.error+shift)<.01f);CHECK(g.error*shift<0);
}
{lane_width_model_t blank={0};scene(-10,1);lane_geometry(&blank,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);CHECK(!g.valid);}
{lane_width_model_t saved=model;scene(0,0);lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);CHECK(!memcmp(&model,&saved,sizeof(model)));}
scene(0,0);for(int i=0;i<240;i++){left.Row[i]=right.Row[i]=254;}lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);CHECK(!g.valid);
scene(0,1);for(int y=40;y<=110;y++)for(int x=80;x<110;x++)mask[y][x]=0;lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);CHECK(!g.valid);
/* Alternating disconnected row centers must not become a valid road. */
scene(0,0);
for(int i=0;i<110;i++){int y=left.Row[i],d=(y%2)?10:-10;left.Col[i]+=d;right.Col[i]+=d;}
for(int y=0;y<120;y++){int d=(y%2)?10:-10;for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(x>=44+d&&x<=144+d?255:0);}
lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);CHECK(!g.valid);
scene(0,0);for(int i=0;i<70;i++)middleline[i]=188;lane_geometry(&model,left.Row,left.Col,right.Row,right.Col,mask,weight,mid_row,middleline,0,&g);CHECK(g.valid&&!g.same_corridor);
CHECK(ring_center_servo(1430,50,-50,g.valid,g.same_corridor)==1430);
for(int base=1430;base<=1670;base+=10)for(int near=-100;near<=100;near+=5){
 float v=ring_center_servo((float)base,(float)near,0,1,1),d=(float)base-1550;
 CHECK(v>=1430&&v<=1670);CHECK(fabsf(v-base)<=15.001f);
 if(d!=0){CHECK((v-1550)*d>0);CHECK(fabsf(v-1550)+.001f>=fabsf(d)*.85f);}else CHECK(v==1550);
 CHECK(ring_center_servo((float)base,(float)near,0,0,1)==base);
 CHECK(ring_center_servo((float)base,(float)near,0,1,0)==base);
}
{lane_geometry_t valid={0};valid.valid=1;valid.clearance=.5f;CHECK(edge_speed_limit(1,&valid)==1);valid.clearance=.2f;CHECK(fabsf(edge_speed_limit(1,&valid)-.6f)<.001f);CHECK(edge_speed_limit(.55f,&valid)==.55f);CHECK(edge_speed_limit(0,&valid)==0);valid.valid=0;CHECK(edge_speed_limit(1,&valid)==1);}
printf("EDGE_CENTERING %u checks, %u failures; measured width / single-edge / corridor / bounded ring trim\n",checks,failures);return failures?1:0;}
