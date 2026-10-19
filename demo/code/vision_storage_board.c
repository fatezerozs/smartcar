#include "main.h"
#include "vision_storage.h"

/* The scatter file reserves exactly the final two 4KiB pages of 4MiB NOR.
 * Never accept caller-supplied addresses; each callback only accepts slot 0/1.
 * No pin is reconfigured, in particular MODE/RST are not application keys. */
#define VS_SECTOR 127U
#define VS_FIRST_PAGE 6U
#define VS_SLOT_BASE 0x703FE000U
static int read_slot(unsigned slot,uint32_t *out,unsigned words)
{
    unsigned i;const volatile uint32_t *source;
    if(slot>1 || !out || words!=VS_RECORD_WORDS)return 0;
    DCACHE_CleanInvalidateByRange(VS_SLOT_BASE+slot*FLASH_PAGE_SIZE,FLASH_PAGE_SIZE);
    source=(const volatile uint32_t *)(VS_SLOT_BASE+slot*FLASH_PAGE_SIZE);
    for(i=0;i<words;++i)out[i]=source[i];
    return 1;
}

static int replace_slot(unsigned slot,const uint32_t *record,unsigned words)
{
    const volatile uint32_t *existing;unsigned i;int blank=1;
    flash_page_enum page;
    if(slot>1 || words!=VS_RECORD_WORDS || !record || !vision_drive_can_store())return 0;
    page=(flash_page_enum)(VS_FIRST_PAGE+slot);
    DCACHE_CleanInvalidateByRange(VS_SLOT_BASE+slot*FLASH_PAGE_SIZE,FLASH_PAGE_SIZE);
    existing=(const volatile uint32_t *)(VS_SLOT_BASE+slot*FLASH_PAGE_SIZE);
    for(i=0;i<FLASH_PAGE_SIZE/4;++i)if(existing[i]!=0xFFFFFFFFU){blank=0;break;}
    /* Do not erase an unknown previous application's private data. A damaged
     * record bearing our magic is ours; foreign/nonempty pages are refused. */
    if(!blank && existing[0]!=0x56465334U)return 0;
    if(flash_init()!=0)return 0;
    if(flash_erase_page(VS_SECTOR,page)!=0)return 0;
    flash_buffer_clear();
    for(i=0;i<words;++i)flash_union_buffer[i].uint32_type=record[i];
    /* Vendor flash_write_page programs flash_union_buffer, NOT its buf argument;
     * populate it explicitly and check this direct function's return value.
     * The convenience from_buffer wrapper incorrectly reports success always.
     * vs_save independently rereads every record word and validates its CRC. */
    if(flash_write_page(VS_SECTOR,page,(const uint32_t *)flash_union_buffer,(uint16)words)!=0)return 0;
    DCACHE_CleanInvalidateByRange(VS_SLOT_BASE+slot*FLASH_PAGE_SIZE,FLASH_PAGE_SIZE);
    __DSB();__ISB();
    return 1;
}

void vision_storage_board_init(void)
{
    const vs_io_t io={read_slot,replace_slot};
    vs_init(&io); /* No flash configuration/erase/program at boot. */
}
