#ifndef HEARTBEAT_DIAG_H
#define HEARTBEAT_DIAG_H
#define HD_QUIET_CONTROL 1
extern unsigned hd_quiet;
#include <stdint.h>
typedef struct {uint32_t count,last_ms,max_gap_ms;} hd_event_t;
typedef struct {hd_event_t rx,parsed,accepted;uint32_t stop_ms,lease_ms,reason,valid;} hd_state_t;
static inline void hd_event(hd_event_t *e,uint32_t now) {
    uint32_t gap=now-e->last_ms;
    if(e->count && gap>e->max_gap_ms)e->max_gap_ms=gap;
    ++e->count;e->last_ms=now;
}
static inline void hd_capture(hd_state_t *snapshot,const hd_state_t *live,
                             uint32_t now,uint32_t lease,unsigned reason) {
    *snapshot=*live;snapshot->stop_ms=now;snapshot->lease_ms=lease;
    snapshot->reason=reason;snapshot->valid=1;
}
#ifndef HD_HOST_TEST
extern volatile hd_state_t hd_live;
void hd_arm(void);
void hd_stop(unsigned reason,uint32_t now,uint32_t lease);
void hd_format(char *out,unsigned capacity);
#endif
#endif
