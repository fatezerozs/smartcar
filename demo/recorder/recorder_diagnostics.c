#include "main.h"
#include "recorder_diagnostics.h"
volatile uint32_t recorder_boot_stage_id;
volatile uint8_t recorder_camera_ok;
/* Deliberately independent of the scheduler, libc formatting, queue mutexes
 * and radio RTS. Each byte is tried once; absent/busy hardware never waits.
 * Poll at bounded CPU iterations so broken clocks cannot hang diagnostics. */
static void emergency_text(const char *s)
{
    unsigned i;
    while(*s) {
        for(i=0;i<20000U;++i)
            if((LPUART_GetStatusFlags(LPUART1)&kLPUART_TxDataRegEmptyFlag) &&
               (LPUART_GetStatusFlags(LPUART8)&kLPUART_TxDataRegEmptyFlag))break;
        if(LPUART_GetStatusFlags(LPUART1)&kLPUART_TxDataRegEmptyFlag)LPUART_WriteByte(LPUART1,(uint8_t)*s);
        if(LPUART_GetStatusFlags(LPUART8)&kLPUART_TxDataRegEmptyFlag)LPUART_WriteByte(LPUART8,(uint8_t)*s);
        ++s;
    }
}
static void emergency_hex(uint32_t value)
{
    char s[11];unsigned i;s[0]='0';s[1]='x';s[10]=0;
    for(i=0;i<8;++i)s[i+2]="0123456789abcdef"[(value>>(28U-i*4U))&15U];
    emergency_text(s);
}
void recorder_boot_stage(uint32_t stage)
{
    recorder_boot_stage_id=stage;
    emergency_text("[REC_BOOT] rev=5 stage=");emergency_hex(stage);
    emergency_text(" primask=");emergency_hex(__get_PRIMASK());
    emergency_text(" basepri=");emergency_hex(__get_BASEPRI());
    emergency_text(" vtor=");emergency_hex(SCB->VTOR);emergency_text("\r\n");
}
void recorder_panic_report(const char *file,uint32_t line)
{
    /* Called after all actuator outputs are zeroed. No RTOS calls here. */
    emergency_text("[REC_PANIC] stage=");emergency_hex(recorder_boot_stage_id);
    emergency_text(" line=");emergency_hex(line);
    emergency_text(" ipsr=");emergency_hex(__get_IPSR());
    emergency_text(" cfsr=");emergency_hex(SCB->CFSR);
    emergency_text(" hfsr=");emergency_hex(SCB->HFSR);
    emergency_text(" bfar=");emergency_hex(SCB->BFAR);
    emergency_text(" file=");emergency_text(file?file:"exception");emergency_text("\r\n");
}
