#include "app_link.h"
#include <string.h>
typedef struct {
    volatile uint8_t rx[APP_LINK_RX_SIZE];
    uint8_t tx[APP_LINK_TX_SIZE];
    volatile uint32_t rx_write, rx_read, errors;
    uint32_t tx_write, tx_read, seen_errors;
    app_link_stats_t stats;
} link_t;
static link_t links[APP_LINK_COUNT];
void app_link_init(void)
{
    memset(links,0,sizeof(links));
    app_link_board_init();
}
void app_link_rx_isr(unsigned channel,uint8_t byte)
{
    link_t *p;
    if(channel>=APP_LINK_COUNT)return;
    p=&links[channel];++p->stats.rx_bytes;
    if((uint32_t)(p->rx_write-p->rx_read)>=APP_LINK_RX_SIZE) {
        ++p->stats.rx_overflow;++p->errors;return;
    }
    p->rx[p->rx_write%APP_LINK_RX_SIZE]=byte;
    ++p->rx_write;
}
void app_link_error_isr(unsigned channel)
{
    if(channel<APP_LINK_COUNT){++links[channel].stats.rx_errors;++links[channel].errors;}
}
int app_link_take_error(unsigned channel)
{
    uint32_t errors;
    if(channel>=APP_LINK_COUNT)return 0;
    errors=links[channel].errors;
    if(errors==links[channel].seen_errors)return 0;
    links[channel].seen_errors=errors;return 1;
}
uint32_t app_link_read(unsigned channel,uint8_t *data,uint32_t capacity)
{
    uint32_t n=0;link_t *p;
    if(channel>=APP_LINK_COUNT || !data)return 0;
    p=&links[channel];
    while(n<capacity && p->rx_read!=p->rx_write) {
        data[n++]=p->rx[p->rx_read%APP_LINK_RX_SIZE];++p->rx_read;
    }
    return n;
}
int app_link_frame(unsigned channel,const uint8_t *header,uint32_t hn,const uint8_t *data,uint32_t n)
{
    link_t *p;uint32_t i;
    if(channel>=APP_LINK_COUNT || (hn && !header) || (n && !data))return 0;
    p=&links[channel];
    if(hn>APP_LINK_TX_SIZE || n>APP_LINK_TX_SIZE-hn ||
       hn+n>APP_LINK_TX_SIZE-(uint32_t)(p->tx_write-p->tx_read)) {
        ++p->stats.tx_rejected;return 0;
    }
    for(i=0;i<hn;++i)p->tx[(p->tx_write++)%APP_LINK_TX_SIZE]=header[i];
    for(i=0;i<n;++i)p->tx[(p->tx_write++)%APP_LINK_TX_SIZE]=data[i];
    return 1;
}
int app_link_send(unsigned channel,const uint8_t *data,uint32_t n)
{return app_link_frame(channel,0,0,data,n);}
void app_link_poll(void)
{
    unsigned ch,i;
    for(ch=0;ch<APP_LINK_COUNT;++ch) {
        link_t *p=&links[ch];
        for(i=0;i<16U && p->tx_read!=p->tx_write;++i) {
            if(!app_link_board_try_tx(ch,p->tx[p->tx_read%APP_LINK_TX_SIZE]))break;
            ++p->tx_read;++p->stats.tx_bytes;
        }
    }
}
void app_link_stats(unsigned channel,app_link_stats_t *stats)
{if(channel<APP_LINK_COUNT && stats)*stats=links[channel].stats;}
void app_link_mvp_run(void)
{
    static const char *banner="[LINK_MVP] outputs=OFF; send bytes for same-port echo\r\n";
    unsigned ch;uint8_t data[32];uint32_t n;
    for(ch=0;ch<APP_LINK_COUNT;++ch)
        (void)app_link_send(ch,(const uint8_t *)banner,(uint32_t)strlen(banner));
    for(;;) {
        for(ch=0;ch<APP_LINK_COUNT;++ch) {
            n=app_link_read(ch,data,sizeof(data));
            if(n)(void)app_link_send(ch,data,n);
        }
        app_link_poll();
    }
}
