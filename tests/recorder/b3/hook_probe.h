#include <stdint.h>
typedef struct {uint32_t CYCCNT;} b3_probe_dwt_t;
extern b3_probe_dwt_t b3_probe_dwt;
#define DWT (&b3_probe_dwt)
