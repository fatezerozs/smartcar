#ifndef VISION_RING_ENTRY_H
#define VISION_RING_ENTRY_H
/* A confirmed tangent island needs an entry target, rather than continuing
 * to center the straight corridor. Keep measured boundaries/widths untouched.
 * The guide exists only with a CURRENT island and CURRENT white clearance. */
static void reference_ring_entry_guide(const uint8_t *binary,vision_result_t *out,float weight_sum)
{
    float correction=0;
    unsigned rows=0;
    if(!out->valid||reference_ring.state!=1||!reference_ring.island_visible)return;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        int flags=out->row_flags[y],width=out->row_width[y],point,margin,good=1;
        float center,progress,desired,weight;
        if(flags<1||flags>3||width<VISION_MIN_WIDTH)continue;
        center=flags==1?out->left[y]+(width-1)*.5f:
            flags==2?out->right[y]-(width-1)*.5f:(out->left[y]+out->right[y])*.5f;
        if(center<0||center>VISION_WIDTH-1)continue;
        /* Current island lower cap advances the entry target toward the car.
         * No elapsed-time turn schedule or old-frame pixels are used. */
        progress=(reference_ring.island_bottom+12.0f-y)/32.0f;
        if(progress<=0)continue;if(progress>1)progress=1;
        desired=center+reference_ring.side*width*.18f*progress;
        point=(int)(desired+.5f);margin=(width+3)/4;
        if(point-margin<0||point+margin>=VISION_WIDTH)continue;
        /* A central half-road-width strip around the candidate must be
         * current white, as must the whole lateral transition from center. */
        for(int x=point-margin;x<=point+margin;++x)if(!binary[y*VISION_WIDTH+x]){good=0;break;}
        if(!good)continue;
        {int a=(int)(center+.5f),b=point;if(a>b){int t=a;a=b;b=t;}
         for(int x=a;x<=b;++x)if(!binary[y*VISION_WIDTH+x]){good=0;break;}}
        if(!good)continue;
        out->entry_guide[y]=(uint8_t)point;++rows;
        weight=y>=60&&y<=90?14.0f:(y>=40?8.0f:2.0f);
        if(flags!=3)weight*=.5f;
        correction+=weight*(point-center);
    }
    if(rows>=12){out->entry_guide_rows=(uint8_t)rows;out->error_px+=correction/weight_sum;}
    else memset(out->entry_guide,VISION_INVALID_X,sizeof(out->entry_guide));
}
#endif
