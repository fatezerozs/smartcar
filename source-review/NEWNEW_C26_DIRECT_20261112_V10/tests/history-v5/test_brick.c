#include "IMG_DEAL.h"
#include "obstacle_path.h"
#include "Threshold.h"
#include "drive_tuning.h"
static obstacle_path_t avoidance;
#include <stdio.h>
void tft180_show_int(int x,int y,int n,int len){(void)x;(void)y;(void)n;(void)len;}
void tft180_show_string(int x,int y,const char *s){(void)x;(void)y;(void)s;}
int main(void){int fail=0;
for(int y=0;y<120;y++)for(int x=0;x<188;x++)IMG_DATA[y][x]=(unsigned char)(x>=22&&x<=166?220:30);
zzkLQGetOSTU0200(0,119);unsigned long total=0;for(int i=0;i<256;i++)total+=HistoGram[i];
if(total!=188UL*120UL)fail++;printf("HISTOGRAM pixels=%lu expected22560\n",total);
Deal_Init();int road_white=0,background_white=0;
for(int y=0;y<120;y++)for(int x=0;x<188;x++)if(IMG_DATA[y][x]==255){if(x>=22&&x<=166)++road_white;else ++background_white;}
printf("SEGMENT road_white=%d expected17400 background_white=%d expected0\n",road_white,background_white);
if(road_white!=17400||background_white)++fail;

for(int side=-1;side<=1;side+=2){
 for(int f=0;f<8;f++){
  for(int y=0;y<120;y++)for(int x=0;x<188;x++)IMG_DATA[y][x]=(unsigned char)(x>=22&&x<=166?220:30);
  if(f>=5)for(int y=65;y<=88;y++)for(int x=100;x<=128;x++)IMG_DATA[y][side>0?x:188-x]=30;
  Img_Deal();obstacle_path(current_road_mask,&avoidance);if(f==4||f==7)printf("FRAME f=%d flags=%d/%d start=%d/%d mid=%d row=%d ring=%d\n",f,left_flag,right_flag,left_start,right_start,middleline[0],mid_row[0],Roundabout_flag);
 }
 int points=0,hits=0;
 if(!avoidance.active||avoidance.blocked){printf("AVOIDANCE unavailable active=%d blocked=%d\n",avoidance.active,avoidance.blocked);++fail;}
 for(int i=0;i<120;i++){int y=i,x=avoidance.center[i];if(y<65||y>88||x<0||x>=188)continue;
  ++points;int l=side>0?100:60,rr=side>0?128:88;
  if(x+29>=l&&x-29<=rr)++hits;
 }
 printf("BRICK side=%d measured_points=%d body_hits=%d\n",side,points,hits);
 if(points<8||hits)++fail;
}
/* Real donor pipeline on left/right curves, stable current road. */
for(int side=-1;side<=1;side+=2){
 for(int f=0;f<8;f++){
  for(int y=0;y<120;y++)for(int x=0;x<188;x++){
   int c=94+side*(110-y)/3,w=45+y*3/4;
   IMG_DATA[y][x]=(unsigned char)(x>=c-w/2&&x<=c+w/2?220:30);
  }Img_Deal();
 }
 int good=0;for(int i=0;i<120;i++)if(mid_row[i]>=0&&mid_row[i]<120&&middleline[i]>=0&&middleline[i]<188)good++;
 printf("CURVE side=%d current_points=%d\n",side,good);if(good<12)fail++;
}
/* Near clipping must not cancel an already observed brick before passing it. */
{static uint8_t m[120][188];
 for(int stage=0;stage<3;stage++){
  for(int y=0;y<120;y++)for(int x=0;x<188;x++)m[y][x]=(uint8_t)(x>=22&&x<=166?255:0);
  for(int y=stage==0?65:stage==1?88:108;y<(stage==0?89:120);y++)
   for(int x=stage==0?100:126;x<=(stage==0?128:152);x++)m[y][x]=0;
  obstacle_path(m,&avoidance);
  printf("NEAR_CLIP stage=%d active=%u blocked=%u\n",stage,avoidance.active,avoidance.blocked);
  if(!avoidance.active||avoidance.blocked)fail++;
 }
}
/* Pure current-mask negatives and blocked corridor. */
for(int kind=0;kind<4;kind++){
 static uint8_t mask[120][188];
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(x>=22&&x<=166?255:0);
 if(kind==1)for(int y=65;y<90;y++)memset(mask[y],0,188); /* full road shadow */
 if(kind==2)for(int y=45;y<100;y++)for(int x=75;x<110;x++)mask[y][x]=0; /* large island */
 if(kind==3)for(int y=65;y<90;y++)for(int x=68;x<121;x++)mask[y][x]=0; /* neither side fits body */
 obstacle_path(mask,&avoidance);
 printf("NEGATIVE kind=%d active=%u blocked=%u\n",kind,avoidance.active,avoidance.blocked);
 if(kind<3&&avoidance.active)fail++;
 if(kind==3&&(!avoidance.active||!avoidance.blocked))fail++;
}
if(fabsf(route_speed_mps(0,0,0,50)-1.0f)>.0001f)fail++;
if(route_speed_mps(45,0,0,50)>.70f)fail++;
if(route_speed_mps(20,1,0,50)>.60f)fail++;
if(route_speed_mps(0,1,1,50)!=0)fail++;
printf("VISION_OBSTACLE_REGRESSION failures=%d; synthetic current-image body-clearance case\n",fail);return fail?1:0;}
