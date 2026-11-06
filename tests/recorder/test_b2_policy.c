#include "b2_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){b2_rx_t rx={0};b2_line_t line={0};uint8_t c;const char *s="B2 START\r\n";unsigned i,n=0;
 assert(!b2_window_expired(0,0,0,0,0));assert(!b2_window_expired(9999,0,0,0,0));assert(b2_window_expired(10000,0,0,0,0));
 assert(!b2_window_expired(2999,0,1,0,99));assert(b2_window_expired(3000,0,1,0,0));assert(b2_window_expired(1,0,0,0,100));
 assert(!b2_window_expired(0x10,0xFFFFFFF0,1,0xFFFFFFF0,1));assert(b2_window_expired(0xBB0,0xFFFFFFF0,1,0xFFFFFFF0,1));
 for(i=0;s[i];i++)if(b2_line_feed(&line,(uint8_t)s[i])){assert(!strcmp(line.text,"B2 START"));n++;}assert(n==1);
 for(i=0;i<80;i++)assert(!b2_line_feed(&line,'x'));
 for(i=0;s[i];i++)assert(!b2_line_feed(&line,(uint8_t)s[i]));
 for(i=0;s[i];i++)if(b2_line_feed(&line,(uint8_t)s[i]))n++;assert(n==2);
 for(i=0;i<128;i++)assert(b2_rx_push(&rx,(uint8_t)i));assert(!b2_rx_push(&rx,99));assert(rx.dropped==1);
 for(i=0;i<128;i++){assert(b2_rx_pop(&rx,&c));assert(c==(uint8_t)i);}assert(!b2_rx_pop(&rx,&c));
 rx.head=rx.tail=0xfffffff0U;
 for(i=0;i<64;i++)assert(b2_rx_push(&rx,(uint8_t)i));
 for(i=0;i<64;i++){assert(b2_rx_pop(&rx,&c));assert(c==(uint8_t)i);}
 memset(&rx,0,sizeof rx);memset(&line,0,sizeof line);s="B2 STOP\r\n";n=0;
 for(i=0;s[i];i++)assert(b2_rx_push(&rx,(uint8_t)s[i]));
 while(b2_rx_pop(&rx,&c))if(b2_line_feed(&line,c)){n++;assert(!strcmp(line.text,"B2 STOP"));}assert(n==1);
 puts("PASS IRQ staging: 128-byte burst, overflow, FIFO order, index wrap, complete STOP command");
 puts("PASS B2 window: zero-time, limits, wrap, observed events; command: CRLF, fragmentation, overflow tail discard");return 0;}
