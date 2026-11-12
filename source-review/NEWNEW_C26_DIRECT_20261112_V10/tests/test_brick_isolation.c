#include "IMG_DEAL.h"
#include "Threshold.h"
#include "obstacle_path.h"
#include "brick_mask.h"
#include <stdio.h>
static uint8_t raw[120][188],mask[120][188],saved[120][188];
static obstacle_path_t a;
static unsigned checks,failures;
#define CHECK(c) do{checks++;if(!(c)){failures++;printf("ISOLATION_FAIL %d\n",__LINE__);}}while(0)
void tft180_show_int(int x,int y,int n,int l){(void)x;(void)y;(void)n;(void)l;}
void tft180_show_string(int x,int y,const char *s){(void)x;(void)y;(void)s;}
static void road(void){for(int y=0;y<120;y++)for(int x=0;x<188;x++)raw[y][x]=(uint8_t)(x>=22&&x<=166?220:30);}
int main(void){
for(int side=-1;side<=1;side+=2)for(int level=30;level<=190;level+=40){
 road();for(int y=65;y<=88;y++)for(int x=100;x<=128;x++)raw[y][side>0?x:188-x]=(uint8_t)level;
 memcpy(IMG_DATA,raw,sizeof(raw));memcpy(saved,IMG_DATA,sizeof(saved));
 {uint8_t h[sizeof(HistoGram)];memcpy(h,HistoGram,sizeof(h));CHECK(brick_mask(raw,mask));CHECK(!memcmp(HistoGram,h,sizeof(h)));}
 CHECK(!memcmp(IMG_DATA,saved,sizeof(saved)));
 Img_Deal(); /* Exercise the actual, protected donor route. */
 obstacle_path(mask,&a);CHECK(a.active&&!a.blocked);CHECK(a.rows>=60);
 for(int y=65;y<=88;y++){int x=a.center[y],l=side>0?100:60,rr=side>0?128:88;CHECK(x+34<l||x-34>rr);}
 printf("PRIVATE_BRICK side=%d gray=%d active=%u blocked=%u\n",side,level,a.active,a.blocked);
}
for(int before=0;before<=8;before++)for(int after=0;after<=8;after++)CHECK(obstacle_may_override(before,after,8)==(before==8&&after==8));
for(int kind=0;kind<6;kind++){
 road();if(kind==1)for(int y=65;y<90;y++)memset(raw[y],30,188);
 if(kind==2)for(int y=45;y<100;y++)for(int x=75;x<110;x++)raw[y][x]=30;
 if(kind==3)memset(raw,150,sizeof(raw));
 if(kind==4)for(int y=65;y<90;y++)for(int x=68;x<121;x++)raw[y][x]=30;
 if(kind==5)for(int y=40;y<=110;y++)for(int x=30;x<150;x++){
  float dx=x-85.5f,dy=y-75.5f;if(dx*dx/(20.0f*20.0f)+dy*dy/(10.2f*10.2f)<=1)raw[y][x]=30;
 }
 int valid=brick_mask(raw,mask);obstacle_path(mask,&a);
 if(kind!=4)CHECK(!a.active);else CHECK(a.active&&a.blocked);
 if(kind==3)CHECK(!valid);
}
/* A confirmed object must keep its currently visible bypass when clipped near camera. */
for(int stage=0;stage<3;stage++){
 road();for(int y=stage==0?65:stage==1?88:108;y<(stage==0?89:120);y++)for(int x=stage==0?100:126;x<=(stage==0?128:152);x++)raw[y][x]=180;
 CHECK(brick_mask(raw,mask));obstacle_path(mask,&a);CHECK(a.active&&!a.blocked);
}
printf("BRICK_ISOLATION %u checks, %u failures; current-mask body clearance and all ring priority states\n",checks,failures);return failures?1:0;}
