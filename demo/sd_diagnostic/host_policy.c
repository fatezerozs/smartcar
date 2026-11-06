#include "fsl_sdmmc_host.h"
/* Unsupported capabilities must never be advertised by this board. */
#define kSDMMCHOST_SupportHighSpeed 0U
#define kSDMMCHOST_SupportVoltage1v8 0U
#define kSDMMCHOST_SupportVoltage1v2 0U
#define kSDMMCHOST_Support4BitDataWidth 0U
#define kSDMMCHOST_Support8BitDataWidth 0U
#define kSDMMCHOST_SupportDDRMode 0U
#define kSDMMCHOST_SupportDetectCardByData3 0U
#define kSDMMCHOST_SupportSDR104 0U
#define kSDMMCHOST_SupportSDR50 0U
#define kSDMMCHOST_SupportHS200 0U
#define kSDMMCHOST_SupportHS400 0U
#include "../../libraries/components/sdmmc/host/usdhc/non_blocking/fsl_sdmmc_host.c"
