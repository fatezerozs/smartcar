#include "IMG_DEAL.h"
#include "Threshold.h"
#include <stdio.h>
void tft180_show_int(int x,int y,int n,int len){(void)x;(void)y;(void)n;(void)len;}
void tft180_show_string(int x,int y,const char *s){(void)x;(void)y;(void)s;}
int main(void){unsigned long sum=0;int road=0,background=0;
for(int y=0;y<120;y++)for(int x=0;x<188;x++)IMG_DATA[y][x]=(unsigned char)(x>=22&&x<=166?220:30);
zzkLQGetOSTU0200(0,119);for(int i=0;i<256;i++)sum+=HistoGram[i];
for(int f=0;f<12;f++){for(int y=0;y<120;y++)for(int x=0;x<188;x++)IMG_DATA[y][x]=(unsigned char)(x>=22&&x<=166?220:30);Deal_Init();}
for(int y=0;y<120;y++)for(int x=0;x<188;x++)if(IMG_DATA[y][x]==255){if(x>=22&&x<=166)road++;else background++;}
printf("THRESHOLD_CHECK histogram=%lu/22560 road=%d/17400 background=%d/0\n",sum,road,background);
return sum!=22560||road!=17400||background!=0;}
