/* Read-only export of the five known diagnostic files. No write/expand/delete API. */
#include "diag.h"
#include <stdio.h>
#include <string.h>
#define EXPORT_MAX 32768U
static FATFS export_fs;
static uint8_t export_buffer[EXPORT_MAX] __attribute__((aligned(64)));
static int export_mounted;
static int allowed(const char *name)
{
    static const char *names[]={"S0000000.BIN","S0000001.BIN","S0000002.BIN","P0000000.BIN","P0000001.BIN"};
    unsigned i;
    for(i=0;i<sizeof names/sizeof names[0];i++)if(!strcmp(name,names[i]))return 1;
    return 0;
}
static void error(const char *code,FRESULT fr)
{
    char s[100];snprintf(s,sizeof s,"SDREAD ERROR code=%s fr=%u\r\n",code,(unsigned)fr);diag_emit(s);
}
static int mount_export(void)
{
    FRESULT r;
    if(export_mounted)return 1;
    r=f_mount(&export_fs,"2:",1);
    if(r!=FR_OK){error("MOUNT",r);return 0;}
    export_mounted=1;return 1;
}
static void read_file(const char *name,unsigned long off,unsigned long count,int info)
{
    FIL file;
    FRESULT r,closed;
    UINT got=0;
    unsigned long size;
    char path[32],header[180];
    if(!allowed(name)){error("NAME",FR_INVALID_NAME);return;}
    if(!info&&(!count||count>EXPORT_MAX||off>16UL*1024UL*1024UL)){error("RANGE",FR_INVALID_PARAMETER);return;}
    if(!mount_export())return;
    snprintf(path,sizeof path,"2:/%s",name);
    r=f_open(&file,path,FA_READ);
    if(r!=FR_OK){error("OPEN",r);return;}
    size=(unsigned long)f_size(&file);
    if(!size||size>16UL*1024UL*1024UL){f_close(&file);error("SIZE",FR_INVALID_PARAMETER);return;}
    if(info){
        closed=f_close(&file);if(closed!=FR_OK){error("CLOSE",closed);return;}
        snprintf(header,sizeof header,"SDREAD INFO name=%s size=%lu\r\n",name,size);diag_emit(header);return;
    }
    if(off>=size||count>size-off){f_close(&file);error("RANGE",FR_INVALID_PARAMETER);return;}
    r=f_lseek(&file,(FSIZE_t)off);
    if(r==FR_OK)r=f_read(&file,export_buffer,(UINT)count,&got);
    closed=f_close(&file);
    if(r!=FR_OK||got!=count){error("READ",r);return;}
    if(closed!=FR_OK){error("CLOSE",closed);return;}
    snprintf(header,sizeof header,"SDREAD DATA name=%s offset=%lu length=%lu crc32=%08lX\r\n",
        name,off,count,(unsigned long)diag_crc(export_buffer,(unsigned)count));
    diag_emit(header);diag_send_bytes(export_buffer,(unsigned)count);diag_emit("\r\nSDREAD END\r\n");
}
int diag_export_command(const char *s)
{
    char name[24],extra;
    unsigned long off,count,baud;
    if(strncmp(s,"SDDIAG EXPORT ",14))return 0;
    s+=14;
    if(sscanf(s,"INFO %23s %c",name,&extra)==1){read_file(name,0,0,1);return 1;}
    if(sscanf(s,"READ %23s %lu %lu %c",name,&off,&count,&extra)==3){read_file(name,off,count,0);return 1;}
    if(sscanf(s,"BAUD %lu %c",&baud,&extra)==1){
        char ack[80];
        if(baud!=115200UL&&baud!=460800UL&&baud!=921600UL){error("BAUD",FR_INVALID_PARAMETER);return 1;}
        snprintf(ack,sizeof ack,"SDREAD BAUD next=%lu\r\n",baud);diag_emit(ack);
        if(!diag_uart_baud((uint32_t)baud))error("BAUD_SWITCH",FR_INVALID_PARAMETER);
        return 1;
    }
    error("SYNTAX",FR_INVALID_PARAMETER);return 1;
}
