#include <stdint.h>
enum {VC_DISARMED,VC_RUNNING,VC_FAULT};
typedef struct {unsigned state;uint32_t faults;float lateral,heading,steering,near_center_x,far_center_x;uint8_t observation_valid,far_valid,recovering,search_source;} vc_output_t;
const vc_output_t *vision_drive_output(void);
