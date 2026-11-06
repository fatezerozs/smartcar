#ifndef B2_EXPORT_H
#define B2_EXPORT_H
#include <stddef.h>
/* Strict namespace; no path traversal, root diagnostic files or write APIs. */
static inline int b2_export_dir(const char *s)
{
 unsigned i;if(!s||s[0]!='R')return 0;
 for(i=1;i<8;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='A'&&s[i]<='F')))return 0;
 return s[8]==0;
}
static inline int b2_export_name(const char *s)
{
 unsigned i;const char *a="session.json";const char *p=s;
 if(!s)return 0;while(*a&&*p==*a){p++;a++;}if(!*a&&!*p)return 1;
 for(i=0;i<6;i++)if(s[i]<'0'||s[i]>'9')return 0;
 a=".scraw";p=s+6;while(*a&&*p==*a){p++;a++;}return !*a&&!*p;
}
int b2_export_command(const char *line);
#endif
