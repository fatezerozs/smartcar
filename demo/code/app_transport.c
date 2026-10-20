#include "main.h"
#include "vision_profile.h"
#include "vision_field.h"
#include "app_transport.h"
#include <stdarg.h>
#include <stdio.h>
#include "app_link.h"
#if VISION_DUAL_UART_ENABLE
static unsigned owner, busy_now;
static int stop_now, changed_now, activity_now;
static uint8_t in_line[2], discard[2];
static uint32_t last_summary;
void app_transport_init(void)
{
    owner=busy_now=0;stop_now=changed_now=activity_now=0;last_summary=0;
    memset(in_line,0,sizeof(in_line));memset(discard,0,sizeof(discard));
    app_link_init();
}
static int send_text(unsigned ch,const char *text)
{return app_link_send(ch,(const uint8_t *)text,(uint32_t)strlen(text));}
void app_transport_begin(int busy)
{busy_now=(unsigned)(busy!=0);stop_now=changed_now=activity_now=0;}
int app_transport_stopped(void){return stop_now;}
int app_transport_changed(void){return changed_now;}
int app_transport_activity(void){return activity_now;}
unsigned app_transport_owner(void){return owner;}
void app_transport_poll(void){app_link_poll();}
static void link_status(unsigned ch)
{
    char text[256];app_link_stats_t s;app_link_stats(ch,&s);
    (void)snprintf(text,sizeof(text),"[LINK] port=%s owner=%s rx=%lu tx=%lu overflow=%lu errors=%lu tx_rejected=%lu; x=claim while stopped\r\n",
        ch?"radio":"usb",owner?"radio":"usb",(unsigned long)s.rx_bytes,(unsigned long)s.tx_bytes,
        (unsigned long)s.rx_overflow,(unsigned long)s.rx_errors,(unsigned long)s.tx_rejected);
    (void)send_text(ch,text);
}
uint32_t app_transport_read(uint8_t *data,uint32_t capacity)
{
    unsigned ch;uint32_t n=0;
    /* Scan both bounded batches before the caller may execute any command. */
    for(ch=0;ch<2;++ch) {
        uint32_t i;uint8_t bytes[16];
        uint32_t count;
        if(app_link_take_error(ch)) {
            uint8_t junk[APP_LINK_RX_SIZE];
            (void)app_link_read(ch,junk,sizeof(junk));
            discard[ch]=1;in_line[ch]=1;stop_now=1;changed_now=1;
            (void)send_text(ch,"[LINK] RX_ERROR; discarded until newline\r\n");
        }
        count=app_link_read(ch,bytes,capacity<sizeof(bytes)?capacity:sizeof(bytes));
        if(count)activity_now=1;
        for(i=0;i<count;++i) {
            uint8_t c=bytes[i],lower=(c>='A' && c<='Z')?(uint8_t)(c-'A'+'a'):c;
            if(c=='!' || c==3 || (!in_line[ch] && lower=='s')) {
                stop_now=1;
                /* Finish discarding a partial line; its tail is never commands. */
                if(in_line[ch])discard[ch]=1;
                (void)send_text(ch,"[LINK] STOP\r\n");
            }
            if(discard[ch]) {
                if(c=='\r' || c=='\n'){discard[ch]=0;in_line[ch]=0;}
                continue;
            }
            if(!in_line[ch] && lower=='x') {
                if(!busy_now && !stop_now) {
                    if(owner!=ch){owner=ch;changed_now=1;stop_now=1;n=0;}
                    link_status(ch);
                } else (void)send_text(ch,"[LINK] REJECT busy/stop; stop first, then x\r\n");
                continue;
            }
            if(c=='@')in_line[ch]=1;
            else if(c=='\r' || c=='\n')in_line[ch]=0;
            if(ch!=owner) {
                if(c=='@') {
                    discard[ch]=1;
                    (void)send_text(ch,"[LINK] REJECT not owner; x=claim while stopped\r\n");
                } else if(c=='?')link_status(ch);
                continue;
            }
            if(n<capacity)data[n++]=c;
        }
    }
    /* A claim is a stop-only cycle; no bytes following x can launch motion. */
    if(changed_now){n=0;for(ch=0;ch<2;++ch)if(in_line[ch])discard[ch]=1;}
    if(stop_now) {
        uint8_t junk[APP_LINK_RX_SIZE];
        for(ch=0;ch<2;++ch) {
            /* Drop commands already queued behind STOP/claim, never replay them. */
            uint32_t count=app_link_read(ch,junk,sizeof(junk));
            if(count){in_line[ch]=discard[ch]=1;}
        }
    }
    return n;
}
void app_transport_write(const uint8_t *data,uint32_t length)
{(void)app_link_send(owner,data,length);}
void app_transport_frame(const uint8_t *h,uint32_t hn,const uint8_t *data,uint32_t n)
{(void)app_link_frame(owner,h,hn,data,n);}
void app_transport_summary(uint32_t now,uint32_t frame,unsigned state,unsigned faults,int valid,unsigned motion)
{
    char text[160];
    if((uint32_t)(now-last_summary)<200U)return;
    last_summary=now;
    (void)snprintf(text,sizeof(text),"[TELEM] ms=%lu n=%lu state=%u faults=%u valid=%d owner=%s motion=%u\r\n",
        (unsigned long)now,(unsigned long)frame,state,faults,valid,owner?"radio":"usb",motion);
    (void)send_text(owner^1U,text);
}
#else
void app_transport_init(void){}
void app_transport_begin(int busy){(void)busy;}
void app_transport_poll(void){}
int app_transport_stopped(void){return 0;}
int app_transport_changed(void){return 0;}
int app_transport_activity(void){return 0;}
unsigned app_transport_owner(void){return VISION_WIRELESS_UART_ENABLE?1U:0U;}
void app_transport_summary(uint32_t now,uint32_t frame,unsigned state,unsigned faults,int valid,unsigned motion)
{(void)now;(void)frame;(void)state;(void)faults;(void)valid;(void)motion;}
void app_transport_frame(const uint8_t *h,uint32_t hn,const uint8_t *data,uint32_t n)
{app_transport_write(h,hn);app_transport_write(data,n);}
uint32_t app_transport_read(uint8_t *data,uint32_t capacity)
{
#if VISION_WIRELESS_UART_ENABLE
    return wireless_uart_read_buffer(data,capacity);
#else
    return debug_read_ring_buffer(data,capacity);
#endif
}
void app_transport_write(const uint8_t *data,uint32_t length)
{
#if VISION_WIRELESS_UART_ENABLE
    (void)wireless_uart_send_buffer(data,length);
#else
    (void)debug_send_buffer(data,length);
#endif
}
#endif
int app_console_printf(const char *format,...)
{
    char text[VF_RESPONSE_CAPACITY];int n;va_list args;
    va_start(args,format);n=vsnprintf(text,sizeof(text),format,args);va_end(args);
    if(n>0) {
#if VISION_WIRELESS_UART_ENABLE || VISION_DUAL_UART_ENABLE
        app_transport_write((const uint8_t *)text,(uint32_t)n<sizeof(text)?(uint32_t)n:sizeof(text)-1U);
#else
        printf("%s",text);
#endif
    }
    return n;
}
