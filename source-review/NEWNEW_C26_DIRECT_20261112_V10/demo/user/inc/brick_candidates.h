#ifndef BRICK_CANDIDATES_H
#define BRICK_CANDIDATES_H
#include "brick_mask.h"
#include "obstacle_path.h"
/* This secondary mask is obstacle-only. It never enters donor image or
 * lane/ring geometry. Preserve the original detector as first choice. */
static inline int brick_bright_mask(const uint8_t gray[120][188],uint8_t mask[120][188])
{
 uint32_t hist[256]={0},sum=0;int low=0,high=255,drop,threshold;
 for(int y=40;y<=110;y++)for(int x=0;x<188;x++)hist[gray[y][x]]++;
 for(int i=0;i<256;i++){sum+=hist[i];if(sum>=188U*71U/10U){low=i;break;}}
 sum=0;for(int i=0;i<256;i++){sum+=hist[i];if(sum>=188U*71U*9U/10U){high=i;break;}}
 if(high-low<24){memset(mask,0,120*188);return 0;}
 drop=(high-low)*4/100;if(drop<8)drop=8;threshold=high-drop;
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(gray[y][x]>threshold?255:0);
 return 1;
}
static inline int brick_stage_allowed(int before,int after)
{
 return (before==NO_ROUND||before==ROUND_READY||before==GET_IN_READY)&&
        (after==NO_ROUND||after==ROUND_READY||after==GET_IN_READY);
}
static inline int brick_rejects_ring_candidate(int before,int after,int active)
{return active&&brick_stage_allowed(before,after)&&after!=NO_ROUND;}
static inline void brick_paths(const uint8_t base[120][188],int base_valid,
 const uint8_t bright[120][188],int bright_valid,obstacle_path_t *out,unsigned *variant)
{
 int8_t side=op_last_side;int tracking=op_tracking,l=op_prev_l,r=op_prev_r,bottom=op_prev_bottom;
 memset(out,0,sizeof(*out));*variant=0;
 if(base_valid)obstacle_path(base,out);else obstacle_path_reset();
 if(out->active){*variant=1;return;}
 if(bright_valid){
  op_last_side=side;op_tracking=tracking;op_prev_l=l;op_prev_r=r;op_prev_bottom=bottom;
  obstacle_path(bright,out);if(out->active)*variant=2;
 }
}
#endif
