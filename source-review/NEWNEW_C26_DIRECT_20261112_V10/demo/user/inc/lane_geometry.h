#ifndef LANE_GEOMETRY_H
#define LANE_GEOMETRY_H
#include <stdint.h>
#include <string.h>
typedef struct {float width[120];uint8_t samples[120];} lane_width_model_t;
typedef struct {int valid,model_used,same_corridor;unsigned rows,near_rows;float error,near_error,clearance;} lane_geometry_t;
static inline float lg_abs(float x){return x<0?-x:x;}
/* Width history contains scalar camera geometry only. CURRENT edges and white
 * interior are mandatory on every frame; a previous path is never replayed. */
static inline void lane_geometry(lane_width_model_t *model,
 const int lr[240],const int lc[240],const int rr[240],const int rc[240],
 const uint8_t mask[120][188],const int weights[120],
 const int path_row[240],const int path_col[240],int allow_learning,lane_geometry_t *out)
{
 int l[120],r[120],good[120]={0},real_pair[120]={0};
 float center[120]={0},width[120]={0},near_sum=0,far_sum=0,sum=0,ws=0,clear=0;
 int near_n=0,far_n=0,longest=0,chain=0,anchors=0,inside=0;
 memset(out,0,sizeof(*out));
 for(int y=0;y<120;y++)l[y]=r[y]=-1;
 for(int i=0;i<240;i++){
  int y=lr[i],x=lc[i];if(y>=40&&y<=110&&x>=2&&x<=185){if(l[y]==-1)l[y]=x;else if(l[y]>=0&&lg_abs((float)(x-l[y]))>4)l[y]=-2;}
  y=rr[i];x=rc[i];if(y>=40&&y<=110&&x>=2&&x<=185){if(r[y]==-1)r[y]=x;else if(r[y]>=0&&lg_abs((float)(x-r[y]))>4)r[y]=-2;}
 }
 /* First collect measured pairs only, for conservative straight-road learning. */
 for(int y=40;y<=110;y++)if(l[y]>=0&&r[y]>=0){
  int w=r[y]-l[y],bad=0,run=0,maxrun=0,ld=0,rd=0;
  if(w<24||w>174)continue;
  for(int dx=1;dx<=3;dx++){if(l[y]-dx>=0&&!mask[y][l[y]-dx])ld++;if(r[y]+dx<188&&!mask[y][r[y]+dx])rd++;}
  for(int x=l[y]+2;x<r[y]-1;x++){if(!mask[y][x]){bad++;run++;if(run>maxrun)maxrun=run;}else run=0;}
  if(ld<2||rd<2||bad*20>w||maxrun>3)continue;
  center[y]=(l[y]+r[y])*.5f;width[y]=(float)w;real_pair[y]=1;
  if(y>=90){near_sum+=94-center[y];near_n++;}else if(y>=60){far_sum+=94-center[y];far_n++;}
 }
 if(allow_learning&&near_n>=8&&far_n>=8&&lg_abs(near_sum/near_n)<15&&lg_abs(near_sum/near_n-far_sum/far_n)<8){
  for(int y=40;y<=110;y++)if(real_pair[y]&&model->samples[y]<5){
   unsigned n=model->samples[y];
   if(n&&lg_abs(width[y]-model->width[y])>model->width[y]*.20f)n=0;
   model->width[y]=(model->width[y]*n+width[y])/(n+1);model->samples[y]=(uint8_t)(n+1);
  }
 }
 near_sum=0;near_n=0;
 for(int y=40;y<=110;y++){
  int inferred=0;
  if(real_pair[y]){
   if(model->samples[y]>=5&&lg_abs(width[y]-model->width[y])>model->width[y]*.30f)continue;
  }else{
   float w=model->width[y],c;int a,b,bad=0,total=0,run=0,maxrun=0,ed=-1;
   if(model->samples[y]<5||w<24||w>174)continue;
   /* Do not infer through two conflicting edges or an interior obstacle. */
   if(l[y]>=0&&r[y]>=0)continue;
   if(l[y]>=0){ed=l[y];c=ed+w*.5f;}else if(r[y]>=0){ed=r[y];c=ed-w*.5f;}else continue;
   a=(int)(c-w*.5f+2);b=(int)(c+w*.5f-2);
   for(int x=a;x<=b;x++)if(x>=0&&x<188){total++;if(!mask[y][x]){bad++;run++;if(run>maxrun)maxrun=run;}else run=0;}
   if(total<12||bad*20>total||maxrun>3)continue;
   {int dark=0;for(int dx=1;dx<=3;dx++){int x=l[y]>=0?ed-dx:ed+dx;if(x>=0&&x<188&&!mask[y][x])dark++;}if(dark<2)continue;}
   center[y]=c;width[y]=w;inferred=1;
  }
  good[y]=inferred?2:1;
 }
 /* Start at the current near road; do not join unrelated far branches. */
 {int last=-1,gap=0;
 for(int y=110;y>=40;y--){
  if(good[y]&&last>=0&&(lg_abs(center[y]-center[last])>6||lg_abs(width[y]-width[last])>8))good[y]=0;
  if(!good[y]){if(last>=0&&++gap>3)break;continue;}
  gap=0;last=y;out->rows++;if(good[y]==2)out->model_used=1;
  {float w=(float)weights[119-y];if(w>0){sum+=(94-center[y])*w;ws+=w;}}
  if(y>=90){near_sum+=94-center[y];near_n++;clear+=.5f-lg_abs(center[y]-94)/width[y];}
 }
 /* Rows beyond the connected support must not qualify as path anchors. */
 for(int y=40;y<=110;y++)if(last>=0&&y<last)good[y]=0;
 }
 chain=longest=0;for(int y=90;y<=110;y++){if(good[y]){chain++;if(chain>longest)longest=chain;}else chain=0;}
 if(near_n<8||longest<8||ws<=0)return;
 out->valid=1;out->near_rows=(unsigned)near_n;out->error=sum/ws;out->near_error=near_sum/near_n;out->clearance=clear/near_n;
 /* Ring trimming is allowed only when its CURRENT selected near targets
  * lie inside this same white road; 0/188 virtual entry guides are excluded. */
 for(int i=0;i<120;i++){
  int y=path_row[i],x=path_col[i];if(y<90||y>110||!good[y])continue;
  anchors++;if(x>=2&&x<=185&&lg_abs(x-center[y])<width[y]*.40f&&mask[y][x])inside++;
 }
 out->same_corridor=anchors>=8&&inside==anchors;
}
static inline float ring_center_servo(float original,float measured_near_error,float legacy_near_error,
 int geometry_valid,int same_corridor)
{
 float delta,limit,deflection=original-1550.0f,result;
 if(!geometry_valid||!same_corridor)return original;
 delta=(measured_near_error-legacy_near_error)*(1000.0f/90.0f)*.22f;
 limit=lg_abs(deflection)*.15f;if(limit>15)limit=15;
 if(delta>limit)delta=limit;if(delta<-limit)delta=-limit;
 result=original+delta;if(result<1430)result=1430;if(result>1670)result=1670;return result;
}
static inline float edge_speed_limit(float requested,const lane_geometry_t *g)
{
 float t,cap;if(!g->valid)return requested;
 t=(g->clearance-.25f)/.20f;if(t<0)t=0;if(t>1)t=1;
 cap=.60f+(requested-.60f)*t;return requested>cap?cap:requested;
}
#endif
