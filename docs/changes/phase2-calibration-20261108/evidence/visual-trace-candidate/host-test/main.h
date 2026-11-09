#include <stdint.h>
#define AT_SDRAM_SECTION_ALIGN(var,n) var
#define GPT_TIM_1 1
#define DEBUG_UART_INDEX 1
#define APP_LINK_USB 0
typedef struct {unsigned valid,status,confidence;} vision_result_t;
typedef struct {float near_y,far_y;} vc_config_t;
typedef struct {vc_config_t control;} vd_profile_t;
typedef struct {unsigned observation_valid,far_valid;} vc_output_t;
uint32_t timer_get(unsigned);void st_test_marker(uint32_t);unsigned app_transport_owner(void);void vision_drive_get_profile(vd_profile_t *);const vc_output_t *vision_drive_output(void);void uart_write_buffer(unsigned,const uint8_t *,unsigned);
int vq_command(const char *,char *,unsigned,int,uint32_t);void vq_poll(uint32_t,int);void vq_record(const uint8_t *,const vision_result_t *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
