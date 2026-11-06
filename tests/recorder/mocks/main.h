#include <stdint.h>
#define MT9V03X_FPS_DEF 50
#define MT9V03X_EXP_TIME_DEF 512
#define MT9V03X_GAIN_DEF 32
#define MT9V03X_AUTO_EXP_DEF 0
#define MIDTERM_VISUAL_REVIEWED 0
typedef struct {uint32_t CFG0,CFG1;} mock_ocotp_t;
extern mock_ocotp_t mock_ocotp;
#define OCOTP (&mock_ocotp)
#define kCLOCK_Ocotp 1
#define CLOCK_EnableClock(n) ((void)(n))
#define __DSB() ((void)0)
