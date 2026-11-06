#include "b2_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){b2_line_t line={0};const char *s="B2 START\r\n";unsigned i,n=0;
 assert(!b2_window_expired(0,0,0,0,0));assert(!b2_window_expired(9999,0,0,0,0));assert(b2_window_expired(10000,0,0,0,0));
 assert(!b2_window_expired(2999,0,1,0,99));assert(b2_window_expired(3000,0,1,0,0));assert(b2_window_expired(1,0,0,0,100));
 assert(!b2_window_expired(0x10,0xFFFFFFF0,1,0xFFFFFFF0,1));assert(b2_window_expired(0xBB0,0xFFFFFFF0,1,0xFFFFFFF0,1));
 for(i=0;s[i];i++)if(b2_line_feed(&line,(uint8_t)s[i])){assert(!strcmp(line.text,"B2 START"));n++;}assert(n==1);
 for(i=0;i<80;i++)assert(!b2_line_feed(&line,'x'));
 for(i=0;s[i];i++)assert(!b2_line_feed(&line,(uint8_t)s[i]));
 for(i=0;s[i];i++)if(b2_line_feed(&line,(uint8_t)s[i]))n++;assert(n==2);
 puts("PASS B2 window: zero-time, limits, wrap, observed events; command: CRLF, fragmentation, overflow tail discard");return 0;}
