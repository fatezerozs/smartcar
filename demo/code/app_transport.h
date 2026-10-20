#ifndef APP_TRANSPORT_H
#define APP_TRANSPORT_H
#include <stdint.h>
uint32_t app_transport_read(uint8_t *data,uint32_t capacity);
void app_transport_write(const uint8_t *data,uint32_t length);
int app_console_printf(const char *format,...);
void app_transport_init(void);
void app_transport_begin(int busy);
void app_transport_poll(void);
int app_transport_stopped(void);
int app_transport_changed(void);
int app_transport_activity(void);
unsigned app_transport_owner(void);
void app_transport_frame(const uint8_t *header,uint32_t hn,const uint8_t *data,uint32_t n);
void app_transport_summary(uint32_t now,uint32_t frame,unsigned state,unsigned faults,int valid,unsigned motion);
#endif
