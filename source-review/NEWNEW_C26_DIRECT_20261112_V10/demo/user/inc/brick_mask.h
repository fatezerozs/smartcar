#ifndef BRICK_MASK_H
#define BRICK_MASK_H
#include <stdint.h>
#include <string.h>
/* Private obstacle mask. Never changes IMG_DATA, donor threshold/history,
 * widths or island recognition. Gray values cannot identify color. */
static inline int brick_mask(const uint8_t gray[120][188],uint8_t mask[120][188])
{
 uint32_t hist[256]={0},sum=0;int low=0,high=255,threshold;
 for(int y=40;y<=110;y++)for(int x=0;x<188;x++)hist[gray[y][x]]++;
 for(int i=0;i<256;i++){sum+=hist[i];if(sum>=188U*71U/10U){low=i;break;}}
 sum=0;for(int i=0;i<256;i++){sum+=hist[i];if(sum>=188U*71U*9U/10U){high=i;break;}}
 if(high-low<24){memset(mask,0,120*188);return 0;}
 threshold=low+(high-low)*88/100;
 for(int y=0;y<120;y++)for(int x=0;x<188;x++)mask[y][x]=(uint8_t)(gray[y][x]>threshold?255:0);
 return 1;
}
static inline int obstacle_may_override(int before,int after,int normal_state)
{
 return before==normal_state&&after==normal_state;
}
#endif
