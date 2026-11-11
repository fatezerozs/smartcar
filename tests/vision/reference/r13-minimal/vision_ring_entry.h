#ifndef VISION_RING_ENTRY_H
#define VISION_RING_ENTRY_H
/* A confirmed tangent island needs an entry target, rather than continuing
 * to center the straight corridor. Keep measured boundaries/widths untouched.
 * A confirmed side may survive a cropped island cap only while a CURRENT
 * anchored white mouth still supports a connected turn into that branch. */
static int reference_entry_row(const vision_result_t *out,int y)
{
    int flags=out->row_flags[y];
    /* The current crossing already has eight real boundary anchors and its
     * white bridge. It is usable approach geometry, not an absent camera row.
     * Each proposed guide still passes the unchanged current-pixel planner. */
    return (flags>=1&&flags<=3)||(flags==4&&out->junction_anchors>=8);
}
static void reference_ring_entry_guide(const uint8_t *binary,vision_result_t *out,float weight_sum)
{
    float correction=0,crossing_correction=0;
    unsigned rows=0,crossing_rows=0;
    int mouth_only=!reference_ring.island_visible;
    if(!out->valid||!out->roundabout_entry_active||
       (mouth_only&&(out->junction_anchors<8||out->junction_rows<12)))return;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        int flags=out->row_flags[y],width=out->row_width[y],point,margin,good=1;
        float center,reference_center,progress,weight;
        if(!reference_entry_row(out,y)||width<VISION_MIN_WIDTH)continue;
        if(mouth_only&&flags!=4)continue;
        center=flags==1?out->left[y]+(width-1)*.5f:
            flags==2?out->right[y]-(width-1)*.5f:(out->left[y]+out->right[y])*.5f;
        reference_center=center;
        /* A junction's modeled forward line is NOT the chosen ring branch.
         * Start a new current-pixel lateral search at the chassis axis, as
         * the connected planner does, instead of inheriting that exit line. */
        if(flags==4)center=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X;
        if(center<0||center>VISION_WIDTH-1)continue;
        /* Current island lower cap advances the entry target toward the car.
         * No elapsed-time turn schedule or old-frame pixels are used. */
        progress=mouth_only?1:(reference_ring.island_bottom+12.0f-y)/32.0f;
        if(progress<=0)continue;if(progress>1)progress=1;
        /* This is a confirmed island's CURRENT open mouth. Explore up to the
         * existing half-road-width bound instead of barely nudging toward
         * the tangent. White clearance and the connected planner still bound
         * the actual target; this is not a fixed steering command. */
        if(flags==4)progress=1;
        point=(int)(center+.5f);margin=(width+3)/4;
        if(point-margin<0||point+margin>=VISION_WIDTH)continue;
        for(int x=point-margin;x<=point+margin;++x)
            if(!binary[y*VISION_WIDTH+x]){good=0;break;}
        if(!good)continue;
        /* Advance only through CURRENT white clearance. A fixed fraction can
         * be too weak to leave a tangent main line, or reject a useful smaller
         * move when the full fraction is obstructed. Stop at the first dark
         * pixel; never jump across an obstacle to a different white branch. */
        for(int step=1;step<=(int)(width*.5f*progress);++step){
            int next=point+reference_ring.side,clear=1;
            if(next-margin<0||next+margin>=VISION_WIDTH)break;
            for(int x=next-margin;x<=next+margin;++x)
                if(!binary[y*VISION_WIDTH+x]){clear=0;break;}
            if(!clear)break;
            point=next;
        }
        if(point==(int)(center+.5f))continue;
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
        correction+=weight*(point-reference_center);
        if(flags==4){++crossing_rows;crossing_correction+=weight*(point-reference_center);}
    }
    /* Replace independent offsets with one current connected entry path. */
    float legacy_correction=correction-crossing_correction;
    uint8_t legacy_guide[VISION_HEIGHT];
    unsigned legacy_rows=rows-crossing_rows;
    memcpy(legacy_guide,out->entry_guide,sizeof(legacy_guide));
    /* New crossing candidates may control only after the connected planner
     * succeeds. An optional planner failure retains the ORIGINAL legacy
     * guide, without the newly searched crossing points. */
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)
        if(out->row_flags[y]==4)legacy_guide[y]=VISION_INVALID_X;
    if(rows>=12){
        float near=0;int near_n=0,near_bottom=VISION_ROI_BOTTOM;
        /* A small bottom crop must not erase an otherwise current entry.
         * Start at the closest usable near row, at most six rows earlier;
         * never extend the guide into pixels below that visible footprint. */
        while(near_bottom>VISION_ROI_BOTTOM-6&&!reference_entry_row(out,near_bottom))--near_bottom;
        for(int y=near_bottom;y>near_bottom-8;--y)
            if(reference_entry_row(out,y)){
                if(!near_n)near=out->center[y];++near_n;
            }
        if(near_n>=6){
            /* Anchor to the vehicle reference within the current near strip,
             * before the island-side width model biases its center. */
            near=(VISION_WIDTH-1)*VISION_AXIS_NEAR_X;
            reference_ring_bridge_prepare_at(binary,near,1,out->entry_guide,near_bottom);
            int selected=-1,selected_guide=0,near_top=near_bottom;float best=0;
            while(near_top>=VISION_ROI_TOP&&reference_entry_row(out,near_top))--near_top;
            for(int y=VISION_ROI_TOP+7;y<=near_bottom-11;++y){
                int guide=out->entry_guide[y];
                /* Confirmed entry owns the current crossing branch. Its
                 * virtual straight-line center cannot veto an islandward
                 * target; measure that crossing shift from the chassis. */
                float shift=reference_ring.side*(guide-near);
                /* Every published path row must be usable by the controller;
                 * do not mark a guide connected while silently omitting a
                 * flags0 hole. Current bridge rows above are permitted. */
                if(y<=near_top||guide>=VISION_WIDTH||shift<1||
                   !reference_ring_bridge_to(y,(float)guide,near_bottom))continue;
                float score=shift/(learned_width[y]+1)*(y>=60&&y<=90?1.0f:.7f);
                if(score>best){best=score;selected=y;selected_guide=guide;}
            }
            if(selected>=0&&reference_ring_bridge_to(selected,(float)selected_guide,near_bottom)){
                int y=selected;
                memset(out->entry_guide,VISION_INVALID_X,sizeof(out->entry_guide));
                rows=0;correction=0;
                for(int row=y;row<=near_bottom;++row)
                    if(reference_entry_row(out,row)){
                        out->entry_guide[row]=ring_bridge_points[row];++rows;
                        float w=row>=60&&row<=90?14.0f:(row>=40?8.0f:2.0f);
                        if(out->row_flags[row]!=3)w*=.5f;
                        correction+=w*((int)ring_bridge_points[row]-out->center[row]);
                    }
                if(rows<12)goto legacy_entry;
                out->entry_guide_rows=(uint8_t)rows;
                out->error_px+=correction/weight_sum;
                out->entry_path_valid=1;return;
            }
        }
    }
legacy_entry:
    /* Optional planner failure must not erase the existing usable guide. */
    if(legacy_rows>=12){
        memcpy(out->entry_guide,legacy_guide,sizeof(legacy_guide));
        out->entry_guide_rows=(uint8_t)legacy_rows;
        out->error_px+=legacy_correction/weight_sum;
    } else memset(out->entry_guide,VISION_INVALID_X,sizeof(out->entry_guide));
}
#endif
