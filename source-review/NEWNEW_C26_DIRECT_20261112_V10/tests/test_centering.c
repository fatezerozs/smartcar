#include "IMG_DEAL.h"
#include <stdio.h>
#include <math.h>
#include "lane_centering.h"

struct APEX{int Apex_Row,Apex_Col,Mark;};extern struct APEX left_apex,right_apex;
void Racing_Line(void);
void tft180_show_int(int x,int y,int n,int l){(void)x;(void)y;(void)n;(void)l;}
void tft180_show_string(int x,int y,const char*s){(void)x;(void)y;(void)s;}
int main(void){int failed=0;
for(int width=80;width<=140;width+=20){
 for(int i=0;i<240;i++){left.Row[i]=right.Row[i]=254;left.Col[i]=right.Col[i]=254;middleline[i]=mid_row[i]=254;}
 for(int i=0;i<110;i++){left.Row[i]=right.Row[i]=115-i;left.Col[i]=94-width/2;right.Col[i]=94+width/2;}
 left_flag=right_flag=1;left_apex.Mark=right_apex.Mark=95;
 memset(IMG_DATA,255,sizeof(IMG_DATA));Racing_Line();
 float sum=0;int n=0;
 for(int i=0;i<120;i++)if(mid_row[i]>=90&&mid_row[i]<=110&&middleline[i]!=254){sum+=middleline[i]-94;n++;}
 lane_centering_t centered;static uint8_t mask[120][188];for(int y=0;y<120;y++)for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(x>=94-width/2&&x<=94+width/2?255:0);
 for(int i=0;i<120;i++)weight[i]=1;
 if(!paired_lane_center(left.Row,left.Col,right.Row,right.Col,mask,weight,&centered))failed++;
 float bias=-centered.error;
 printf("CENTER width=%d selected_bias_px=%.2f donor_support=%d\n",width,(double)bias,n);
 if(n<8||fabsf(bias)>2)failed++;
}
/* Current measured road offset must give the matching centering sign. */
for(int shift=-12;shift<=12;shift+=24){
 lane_centering_t c;static uint8_t m[120][188];
 for(int i=0;i<110;i++){left.Row[i]=right.Row[i]=115-i;left.Col[i]=44+shift;right.Col[i]=144+shift;}
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)m[y][x]=(uint8_t)(x>=44+shift&&x<=144+shift?255:0);
 if(!paired_lane_center(left.Row,left.Col,right.Row,right.Col,m,weight,&c)||fabsf(c.error+shift)>.01f)failed++;
 memset(m,255,sizeof(m));if(paired_lane_center(left.Row,left.Col,right.Row,right.Col,m,weight,&c))failed++;
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)m[y][x]=(uint8_t)(x>=44+shift&&x<=144+shift?255:0);
 for(int y=40;y<=110;y++)for(int x=90+shift;x<=98+shift;x++)m[y][x]=0;
 if(paired_lane_center(left.Row,left.Col,right.Row,right.Col,m,weight,&c))failed++;
}
/* Protected states always reject the overlay. */
for(int before=0;before<=8;before++)for(int after=0;after<=8;after++){
 if(lane_center_may_override(before,after,8,0,1,1)!=(before==8&&after==8))failed++;
 if(lane_center_may_override(before,after,8,1,1,1))failed++;
 if(lane_center_may_override(before,after,8,0,1,0))failed++;
}
printf("CENTER_REGRESSION failures=%d; actual production donor paired-boundary path\n",failed);return failed?1:0;}
