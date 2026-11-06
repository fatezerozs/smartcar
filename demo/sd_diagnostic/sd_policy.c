/* Compile original SDK source with a local policy, without editing/copying it.
 * Types/enums are parsed first. Macro aliases affect implementation uses only. */
#include "fsl_sd.h"
#define kSD_Support4BitWidthFlag 0U
#include "../../libraries/components/sdmmc/sd/fsl_sd.c"
