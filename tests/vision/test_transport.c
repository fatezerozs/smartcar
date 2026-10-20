#include "app_transport.h"
#include "app_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char wire[2][20000];
static unsigned used[2],radio_busy;
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned ch,uint8_t c)
{
    if(ch && radio_busy)return 0;
    assert(used[ch]+1<sizeof(wire[ch]));wire[ch][used[ch]++]=(char)c;return 1;
}
static void inject(unsigned ch,const char *s)
{while(*s)app_link_rx_isr(ch,(uint8_t)*s++);}
static uint32_t read_cmd(int busy,uint8_t *out)
{app_transport_begin(busy);return app_transport_read(out,16);}
static void flush(void){for(unsigned i=0;i<1000;++i)app_transport_poll();}
int main(void)
{
    uint8_t out[16];uint32_t n;
    app_transport_init();assert(app_transport_owner()==0);
    inject(0,"@gain ");n=read_cmd(0,out);assert(n==6 && !memcmp(out,"@gain ",6));
    inject(1,"@motor 1 2 3\n");assert(read_cmd(0,out)==0); /* not merged */
    inject(1,"h");assert(read_cmd(1,out)==0); /* no foreign heartbeat */
    inject(0,"1 2 3\n");assert(read_cmd(0,out)==6);
    inject(0,"a");inject(1,"!");assert(read_cmd(0,out)==1 && app_transport_stopped());
    inject(1,"x");assert(read_cmd(1,out)==0 && app_transport_owner()==0);
    inject(1,"xa");assert(read_cmd(0,out)==0 && app_transport_changed() && app_transport_stopped());
    assert(app_transport_owner()==1);
    inject(0,"h");inject(1,"h");assert(read_cmd(1,out)==1 && out[0]=='h');
    app_console_printf("owner-radio\r\n");flush();assert(strstr(wire[1],"owner-radio"));assert(!strstr(wire[0],"owner-radio"));
    inject(1,"@servo ");assert(read_cmd(0,out)==7 && !app_transport_stopped());
    inject(0,"x");assert(read_cmd(0,out)==0 && app_transport_changed());
    inject(1,"1500 1200 1800 1\n");assert(read_cmd(0,out)==0);assert(read_cmd(0,out)==0);
    inject(0,"a");assert(read_cmd(0,out)==1 && out[0]=='a');
    inject(0,"@gain");assert(read_cmd(0,out)==5);
    app_link_error_isr(0);inject(0," 1 2 3\na");assert(read_cmd(0,out)==0 && app_transport_stopped());
    inject(0,"a\n");assert(read_cmd(0,out)==0); /* resync, never execute tail */
    inject(0,"a");assert(read_cmd(0,out)==1);
    inject(1,"@status\n");assert(read_cmd(0,out)==0 && !app_transport_stopped()); /* s inside text is not STOP */
    radio_busy=1;app_transport_summary(1000,42,0,0,1,0);app_console_printf("usb-progress\r\n");flush();
    assert(strstr(wire[0],"usb-progress"));radio_busy=0;flush();assert(strstr(wire[1],"[TELEM]"));
    puts("DUAL ROUTER PASS: split lines, source ownership, heartbeat isolation, STOP priority, claim fencing, RX error resync, RTS fairness.");
    return 0;
}
