/* Control policy for current junction repairs. Geometry/flags remain honest;
 * a repair below the sampling region must not hide measured road above it. */
#ifndef VISION_JUNCTION_POLICY_H
#define VISION_JUNCTION_POLICY_H

static int vj_bridge_rows(const vision_result_t *r,int top,int bottom)
{
    int n=0;
    for(int y=top;y<=bottom&&y<=VISION_ROI_BOTTOM;++y)
        if(y>=VISION_ROI_TOP&&r->row_flags[y]==4)++n;
    return n;
}

static int vj_restrict_preview(const vision_result_t *r)
{
    return r->junction_rows>=4&&r->junction_anchors>=8&&!r->roundabout_state&&
        vj_bridge_rows(r,40,88)>=4;
}

static int vj_current_bridge(const vision_result_t *r)
{
    /* Publication already checks CURRENT white pixels along every repaired
     * strip and real entry/exit anchors. A junction tag is not a speed hazard. */
    return r->valid&&!r->junction_partial&&r->junction_anchors>=8&&
        r->junction_rows>=4;
}

static int vj_paired_support(const vision_result_t *r)
{return r->measured_rows>=24||vj_current_bridge(r);}
#endif
