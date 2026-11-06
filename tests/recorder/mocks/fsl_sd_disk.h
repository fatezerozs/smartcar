#include <stdint.h>
typedef unsigned int UINT;
typedef int FRESULT;
typedef struct {int unused;} FATFS;
typedef struct {unsigned index;} FIL;
typedef struct {struct {uint32_t productSerialNumber;} cid;} sd_card_t;
extern sd_card_t g_sd;
enum {FR_OK=0,FR_DISK_ERR=1,FR_INT_ERR=2,FR_DENIED=7,FR_EXIST=8,FR_TIMEOUT=15};
enum {FA_WRITE=2,FA_CREATE_NEW=4};
FRESULT f_write(FIL *,const void *,UINT,UINT *);
FRESULT f_sync(FIL *);
FRESULT f_close(FIL *);
FRESULT f_open(FIL *,const char *,unsigned);
FRESULT f_mkdir(const char *);
FRESULT f_mount(FATFS *,const char *,unsigned);
