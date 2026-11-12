#include <stdio.h>
#include <string.h>
#define COL 188
#define ROW 120
int middleline[240],mid_row[240],weight[120];
static float old_error(void)
{
    // middleline / mid_row / weight 都在 IMG_DEAL.c 里定义
    int i;
    long err_sum = 0;
    long w_sum   = 0;

    // 没有有效中线，直接认为误差为 0
    if (middleline[0] == 254)
    {
        return 0.0f;
    }

    for (i = 119; i >= 0; i--)
    {
        if (middleline[i] == 254 || mid_row[i] == 254)
        {
            continue;
        }

        int Mid_row = 119 - mid_row[i];
        if (Mid_row < 0 || Mid_row >= 120)
        {
            continue;
        }

        int delta = (COL / 2) - middleline[i];   // 中心 - 中线

        err_sum += (long)weight[Mid_row] * delta;
        w_sum   += (long)weight[Mid_row];
    }

    if (w_sum == 0)
    {
        return 0.0f;
    }

    return (float)err_sum / (float)w_sum;
}
static float now_error(void)
{
    // middleline / mid_row / weight 都在 IMG_DEAL.c 里定义
    int i;
    long err_sum = 0;
    long w_sum   = 0;

    // 没有有效中线，直接认为误差为 0
    if (middleline[0] == 254)
    {
        return 0.0f;
    }

    for (i = 119; i >= 0; i--)
    {
        if (middleline[i] == 254 || mid_row[i] == 254)
        {
            continue;
        }

        int Mid_row = 119 - mid_row[i];
        if (Mid_row < 0 || Mid_row >= 120)
        {
            continue;
        }

        int delta = (COL / 2) - middleline[i];   // 中心 - 中线

        err_sum += (long)weight[Mid_row] * delta;
        w_sum   += (long)weight[Mid_row];
    }

    if (w_sum == 0)
    {
        return 0.0f;
    }

    return (float)err_sum / (float)w_sum;
}
int main(void){int failures=0;int values[]={0,188,34,154,94};
for(int k=0;k<5;k++){for(int i=0;i<240;i++){middleline[i]=254;mid_row[i]=254;}for(int i=0;i<120;i++)weight[i]=1;for(int i=0;i<60;i++){middleline[i]=values[k];mid_row[i]=119-i;}
float a=old_error(),b=now_error();printf("ENTRY_COMPAT col=%d old=%.1f now=%.1f\n",values[k],(double)a,(double)b);if(a!=b)failures++;}
printf("ENTRY_COMPAT failures=%d\n",failures);return failures?1:0;}
