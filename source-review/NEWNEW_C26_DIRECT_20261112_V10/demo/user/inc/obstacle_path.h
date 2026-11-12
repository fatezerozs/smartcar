#ifndef OBSTACLE_PATH_H
#define OBSTACLE_PATH_H
#include <stdint.h>
#include <string.h>
#define OP_W 188
#define OP_H 120
typedef struct {uint8_t active,blocked,rows;int8_t side;float error;uint8_t center[120];} obstacle_path_t;
static int8_t op_last_side;
static int op_tracking,op_prev_l,op_prev_r,op_prev_bottom;
static inline void obstacle_path_reset(void){op_last_side=0;op_tracking=0;}
static int8_t op_parent[120][188];
static uint16_t op_cost[2][188];
/* Current grayscale cannot identify red by color. Detect enclosed interior
 * dark components with white on both sides; full-width shadows are excluded. */
static inline void obstacle_path(const uint8_t mask[120][188],obstacle_path_t *out)
{
 int y,x,left[120],right[120],gap_l=-1,gap_r=-1,start=0,last=0,n=0;
 int best_l=-1,best_r=-1,top=0,bottom=0,best_area=0,first_width=0,last_width=0;
 memset(out,0,sizeof(*out));memset(out->center,255,sizeof(out->center));
 for(y=40;y<=110;y++){
  int l=0,r=187;while(l<188&&!mask[y][l])l++;while(r>=0&&!mask[y][r])r--;
  left[y]=l;right[y]=r;
  if(l<2||r>185||r-l<50){left[y]=right[y]=-1;}
 }
 for(y=45;y<=110;y++){
  int a=-1,b=-1;
  if(left[y]>=0)for(x=left[y]+8;x<right[y]-8;x++)if(!mask[y][x]){
   int s=x;while(x<=right[y]&&!mask[y][x])x++;
   if(x-s>=8&&x-s<=60&&s-left[y]>=(op_tracking?4:12)&&right[y]-x>=(op_tracking?4:12)){a=s;b=x-1;break;}
  }
  if(a>=0&&(!n||(a<=gap_r+4&&b>=gap_l-4))){
   if(!n){start=y;gap_l=a;gap_r=b;first_width=b-a+1;}else{if(a<gap_l)gap_l=a;if(b>gap_r)gap_r=b;}
   n++;last=y;last_width=b-a+1;
  }else{
   if(n>=6&&n<=42&&last==y-1&&a<0){
    int closed=1;
    for(x=gap_l;x<=gap_r;x++)if(!mask[start-1][x]||!mask[y][x])closed=0;
    if(closed&&first_width*10>=(gap_r-gap_l+1)*6&&last_width*10>=(gap_r-gap_l+1)*6&&n*(gap_r-gap_l+1)>best_area){best_area=n*(gap_r-gap_l+1);best_l=gap_l;best_r=gap_r;top=start;bottom=last;}
   }
   n=0;
   if(a>=0){start=last=y;gap_l=a;gap_r=b;first_width=last_width=b-a+1;n=1;}
  }
 }
 /* Continue a previously enclosed object only while its CURRENT gap
  * remains visible near the bottom. Never reuse old path pixels. */
 if(n>=3&&last==110&&op_tracking&&start>=75&&last>=op_prev_bottom-4&&
    gap_l<=op_prev_r+12&&gap_r>=op_prev_l-12){
  best_l=gap_l;best_r=gap_r;top=start;bottom=last;
 }
 if(best_l<0){op_last_side=0;op_tracking=0;return;}
 op_tracking=1;op_prev_l=best_l;op_prev_r=best_r;op_prev_bottom=bottom;
 out->active=1;
 /* DP body-envelope path from chassis footprint upward. All centers and
  * their .40*CURRENT outer-road-width + margins must remain white. */
 memset(op_parent,127,sizeof(op_parent));memset(op_cost,255,sizeof(op_cost));
 for(y=110;y>=40;y--){
  int cur=y&1,prev=cur^1,prefix[189],half;
  prefix[0]=0;for(x=0;x<188;x++)prefix[x+1]=prefix[x]+!mask[y][x];
  for(x=0;x<188;x++)op_cost[cur][x]=65535;
  if(left[y]<0)continue;
  half=((right[y]-left[y]+1)*21+99)/100+3;
  for(x=left[y]+half;x<=right[y]-half;x++){
   unsigned best=65535;int dx_best=0;
   if(prefix[x+half+1]!=prefix[x-half])continue;
   if(y==110){int d=x-94;if(d<0)d=-d;if(d<=6)best=(unsigned)(d*d);}
   else for(int dx=-4;dx<=4;dx++){
    int next=x+dx;unsigned c;
    if(next<0||next>=188||op_cost[prev][next]==65535)continue;
    c=op_cost[prev][next]+(unsigned)(dx*dx*3);
    if(c<best){best=c;dx_best=dx;}
   }
   if(best==65535)continue;
   {int d=x-(left[y]+right[y])/2;if(d<0)d=-d;best+=(unsigned)(d/4);
    if(y>=top&&y<=bottom&&((op_last_side<0&&x>best_r)||(op_last_side>0&&x<best_l)))best+=80U;}
   op_cost[cur][x]=(uint16_t)best;op_parent[y][x]=(int8_t)dx_best;
  }
 }
 {int goal=-1;unsigned best=65535;
  for(x=0;x<188;x++)if(op_cost[40&1][x]<best){best=op_cost[0][x];goal=x;}
  if(goal<0){out->blocked=1;return;}
  x=goal;for(y=40;y<=110;y++){out->center[y]=(uint8_t)x;x+=op_parent[y][x];out->rows++;}
 }
 {float sum=0;int count=0;
  for(y=top;y<=bottom;y++){sum+=out->center[y];count++;}
  out->error=94.0f-sum/count;
  out->side=(int8_t)(sum/count<(best_l+best_r)*.5f?-1:1);op_last_side=out->side;
 }
}
#endif
