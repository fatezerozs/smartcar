#ifndef VISION_STORAGE_H
#define VISION_STORAGE_H
#include "vision_drive.h"

/* Two independent 4KiB erase pages, record is an explicit 64-word format.
 * Never serialize the C struct/padding, live arm state, deadlines or faults.
 * The board supplies bounded slot IO; host tests use an in-memory flash model. */
#define VS_RECORD_WORDS 64U
typedef struct {
    int (*read)(unsigned slot, uint32_t *record, unsigned words);
    int (*replace)(unsigned slot, const uint32_t *record, unsigned words);
} vs_io_t;
enum { VS_OK=0, VS_EMPTY, VS_CORRUPT, VS_IO_ERROR, VS_INVALID, VS_VERIFY_ERROR };
typedef struct { uint32_t generation; uint8_t valid, slot, error; } vs_info_t;
void vs_init(const vs_io_t *io);
int vs_load(vd_profile_t *profile);
/* Full ready profiles only. Save never applies RAM settings or starts motion.
 * Preserve the active slot until the other slot is programmed and verified. */
int vs_save(const vd_profile_t *profile);
void vs_get_info(vs_info_t *info);
/* Board setup installs callbacks only; it performs no erase/write at boot. */
void vision_storage_board_init(void);
#endif
