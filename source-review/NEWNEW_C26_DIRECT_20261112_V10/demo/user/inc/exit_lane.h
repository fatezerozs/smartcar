#ifndef EXIT_LANE_H
#define EXIT_LANE_H
#include "lane_geometry.h"
/* Current white run connected to the near chassis axis, checked against
 * learned scalar widths. No trace history, far-branch selection or timer path. */
static inline int exit_lane_from_mask(const uint8_t mask[120][188],const lane_width_model_t *model,
 const int weights[120],lane_geometry_t *out)
{
 float sum=0,ws=0,near=0,clear=0,last_c=94,last_w=0;int last=-1,near_n=0,far_n=0,n=0,chain=0,longest=0;
 memset(out,0,sizeof(*out));
 for(int y=110;y>=60;y--){
  int x=(int)(last_c+.5f),l,r;float w,c,weight;
  if(last>=0&&last-y>3)break;
  if(last<0&&y<103)break;
  if(model->samples[y]<5||x<2||x>185||!mask[y][x]){chain=0;continue;}
  l=r=x;while(l>0&&mask[y][l-1])l--;while(r<187&&mask[y][r+1])r++;
  w=(float)(r-l);c=(l+r)*.5f;
  if(l<2||r>185||w<24||w>174||lg_abs(w-model->width[y])>model->width[y]*.25f){chain=0;continue;}
  if(last>=0&&(lg_abs(c-last_c)>6||lg_abs(w-last_w)>8)){chain=0;continue;}
  /* The selected same-frame center and its lateral body margin are white. */
  {int half=(int)(w*.21f)+3;if(c-half<l||c+half>r){chain=0;continue;}}
  weight=(float)weights[119-y];if(weight>0){sum+=(94-c)*weight;ws+=weight;}
  n++;chain++;if(chain>longest)longest=chain;
  if(y>=90){near+=94-c;clear+=.5f-lg_abs(c-94)/w;near_n++;}else far_n++;
  last=y;last_c=c;last_w=w;
 }
 if(n<20||near_n<10||far_n<8||longest<12||ws<=0)return 0;
 out->valid=1;out->model_used=1;out->rows=(unsigned)n;out->near_rows=(unsigned)near_n;
 out->error=sum/ws;out->near_error=near/near_n;out->clearance=clear/near_n;return 1;
}
#endif
