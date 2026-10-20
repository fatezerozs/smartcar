#include "app_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned busy, calls[2], count[2];
static uint8_t wire[2][20000];
void app_link_board_init(void){}
int app_link_board_try_tx(unsigned channel,uint8_t byte)
{
    ++calls[channel];
    if(channel==1 && busy)return 0;
    assert(count[channel]<sizeof(wire[channel]));wire[channel][count[channel]++]=byte;return 1;
}
int main(void)
{
    uint8_t data[APP_LINK_RX_SIZE], frame[5652];unsigned i;
    app_link_stats_t stats;
    app_link_init();
    app_link_rx_isr(0,'U');app_link_rx_isr(1,'W');
    assert(app_link_read(0,data,1)==1 && data[0]=='U');
    assert(app_link_read(1,data,1)==1 && data[0]=='W');
    assert(app_link_send(0,(const uint8_t *)"usb",3));
    assert(app_link_send(1,(const uint8_t *)"radio",5));
    busy=1;app_link_poll();
    assert(count[0]==3 && count[1]==0 && calls[1]==1);
    busy=0;app_link_poll();assert(count[1]==5 && !memcmp(wire[1],"radio",5));
    for(i=0;i<APP_LINK_RX_SIZE+1;++i)app_link_rx_isr(1,(uint8_t)i);
    assert(app_link_take_error(1));assert(!app_link_take_error(1));
    assert(app_link_read(1,data,sizeof(data))==sizeof(data));
    for(i=0;i<sizeof(data);++i)assert(data[i]==(uint8_t)i);
    app_link_stats(1,&stats);assert(stats.rx_overflow==1);
    memset(frame,0xA5,sizeof(frame));
    assert(app_link_frame(1,frame,12,frame+12,5640));
    assert(!app_link_frame(1,frame,12,frame+12,5640));
    for(i=0;i<400;++i)app_link_poll();
    assert(count[1]==5657 && !memcmp(wire[1]+5,frame,sizeof(frame)));
    for(i=0;i<40;++i){assert(app_link_send(0,frame,2000));for(unsigned j=0;j<125;++j)app_link_poll();count[0]=0;}
    app_link_error_isr(0);assert(app_link_take_error(0));
    puts("LINK MVP PASS: isolated RX, same-port TX, RTS busy fairness, overflow, atomic frame, ring wrap.");
    return 0;
}
