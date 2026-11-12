#ifndef LANE_CENTERING_H
#define LANE_CENTERING_H
#include <stdint.h>
/* Same public linkage layout as the original donor's struct DIV. */
struct DIV { int Row[240]; int Col[240]; };
extern struct DIV left,right;
typedef struct {unsigned paired,near_rows;float error;} lane_centering_t;
static inline int lane_center_may_override(int before,int after,int normal,int avoiding,int left_seen,int right_seen)
{return before==normal&&after==normal&&!avoiding&&left_seen&&right_seen;}
/* Use only paired, same-frame boundary measurements enclosing current white
 * road. No inferred half width or learned trajectory enters this target. */
static inline int paired_lane_center(const int lr[240],const int lc[240],
    const int rr[240],const int rc[240],const uint8_t mask[120][188],
    const int weights[120],lane_centering_t *out)
{
 uint8_t lo[120],hi[120],rlo[120],rhi[120];
 int chain=0,longest=0,last=-1,last_width=0;float last_center=0,sum=0,total=0;
 out->paired=out->near_rows=0;out->error=0;
 for(int y=0;y<120;y++){lo[y]=rlo[y]=255;hi[y]=rhi[y]=0;}
 for(int i=0;i<240;i++){
  int y=lr[i],x=lc[i];if(y>=40&&y<=110&&x>=2&&x<186){if(x<lo[y])lo[y]=(uint8_t)x;if(x>hi[y])hi[y]=(uint8_t)x;}
  y=rr[i];x=rc[i];if(y>=40&&y<=110&&x>=2&&x<186){if(x<rlo[y])rlo[y]=(uint8_t)x;if(x>rhi[y])rhi[y]=(uint8_t)x;}
 }
 for(int y=110;y>=40;y--){
  int width,bad=0,run=0,maxrun=0,ldark=0,rdark=0;float c,w;
  if(last>=0&&last-y>3)break;
  if(lo[y]==255||rlo[y]==255||hi[y]-lo[y]>4||rhi[y]-rlo[y]>4){chain=0;continue;}
  width=rlo[y]-hi[y];if(width<24||width>174){chain=0;continue;}
  c=(lo[y]+hi[y]+rlo[y]+rhi[y])*.25f;
  if(last>=0&&last-y==1){float d=c-last_center;if(d<0)d=-d;
   if(d>6||width-last_width>8||last_width-width>8){chain=0;continue;}}
  for(int dx=1;dx<=3;dx++){
   int xl=(int)lo[y]-dx,xr=(int)rhi[y]+dx;
   if(xl>=0&&!mask[y][xl])ldark++;
   if(xr<188&&!mask[y][xr])rdark++;
  }
  if(ldark<2||rdark<2){chain=0;continue;}
  for(int x=hi[y]+2;x<rlo[y]-1;x++){
   if(!mask[y][x]){bad++;run++;if(run>maxrun)maxrun=run;}else run=0;
  }
  if(bad*20>width||maxrun>3){chain=0;continue;}
  w=(float)weights[119-y];if(w<=0){chain=0;continue;}
  sum+=(94.0f-c)*w;total+=w;out->paired++;if(y>=90)out->near_rows++;
  chain++;if(chain>longest)longest=chain;last=y;last_width=width;last_center=c;
 }
 if(out->paired<18||out->near_rows<8||longest<8||total<=0)return 0;
 out->error=sum/total;return 1;
}
#endif
