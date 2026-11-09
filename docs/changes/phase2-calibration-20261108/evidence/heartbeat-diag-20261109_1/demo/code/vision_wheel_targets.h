#ifndef VISION_WHEEL_TARGETS_H
#define VISION_WHEEL_TARGETS_H
/* Paired/curve mode boundaries are diagnostic labels, not a step in speed.
 * Only actual requested steering and remaining lateral margin reduce the
 * requested scale. Existing duty shaping also accounts for turn transitions. */
static inline unsigned vt_route_ticks(unsigned maximum,float steering,float requested,float lateral,
                                      float paired,float edge)
{
    float ticks,turn=steering<0?-steering:steering,offset=lateral<0?-lateral:lateral;
    float ahead=requested<0?-requested:requested;
    if(ahead>turn)turn=ahead; /* Anticipate a bend before the servo catches up. */
    {
        turn=(turn-.15f)/.85f;if(turn<0)turn=0;if(turn>1)turn=1;
        ticks=MT_ROUTE_FAST_TICKS-(MT_ROUTE_FAST_TICKS-MT_ROUTE_CORNER_TICKS)*turn;
    }
    /* A one-row boundary flicker must not switch 132 -> 92 -> 50 ticks.
     * Both weights come from validated CURRENT geometry. Missing preview,
     * recovery and STOP supply zero weights immediately. */
    {float support=paired>edge?paired:edge;
     ticks=MT_ROUTE_BASE_TICKS+(MT_ROUTE_EDGE_TICKS-MT_ROUTE_BASE_TICKS)*support+
        (ticks-MT_ROUTE_EDGE_TICKS)*paired;}
    /* |lateral|=1 places the inferred center one half-road-width away.
     * Restore the conservative base continuously as that margin is consumed. */
    offset=(offset-.30f)/.70f;if(offset<0)offset=0;if(offset>1)offset=1;
    ticks+=(MT_ROUTE_BASE_TICKS-ticks)*offset;
    return ticks>maximum?maximum:(unsigned)ticks;
}
/* Ramp one forward-speed envelope, then retain the CURRENT steering ratio.
 * Independent wheel ramps can accidentally request equal speeds for the
 * first part of a bend, fighting the front steering while accelerating. */
static inline void vt_ramp_targets(int32_t targets[2],int32_t old_right,int32_t old_left,unsigned step)
{
    int32_t desired=targets[0]>targets[1]?targets[0]:targets[1];
    int32_t previous=old_right>old_left?old_right:old_left;
    int32_t envelope=previous+(int32_t)step;
    if(desired>envelope&&desired>0){
        targets[0]=(int32_t)((int64_t)targets[0]*envelope/desired);
        targets[1]=(int32_t)((int64_t)targets[1]*envelope/desired);
    }
}
#endif
