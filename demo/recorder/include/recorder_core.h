#ifndef RECORDER_CORE_H
#define RECORDER_CORE_H
#include <stddef.h>
#include <stdint.h>
#define REC_WIDTH 188U
#define REC_HEIGHT 120U
#define REC_BYTES (REC_WIDTH * REC_HEIGHT)
#define REC_ALIGNED_BYTES ((REC_BYTES + 63U) & ~63U)
#define REC_CAPTURE_SLOTS 8U
#define REC_QUEUE_SLOTS 64U
#define REC_NONE 255U
typedef enum { REC_FREE, REC_DMA, REC_RETIRED, REC_READY, REC_HELD } rec_owner_t;
typedef struct { uint64_t sequence, timestamp_us; uint8_t owner; } rec_slot_t;
typedef struct {
    rec_slot_t slot[REC_CAPTURE_SLOTS];
    uint8_t active[2];
    uint64_t observed, ambiguous, exhausted, discarded;
} rec_capture_t;
/* Caller serializes these very short operations against the CSI IRQ.
 * prepare -> PROGRAM REPLACEMENT ADDRESS + DSB -> publish is mandatory. */
void rec_capture_init(rec_capture_t *p);
int rec_capture_prepare(rec_capture_t *p, unsigned dma, uint64_t timestamp_us,
                        uint8_t *retired, uint8_t *replacement);
int rec_capture_publish(rec_capture_t *p, unsigned retired);
int rec_capture_acquire(rec_capture_t *p);
int rec_capture_release(rec_capture_t *p, unsigned index);
void rec_capture_ambiguous(rec_capture_t *p,unsigned observed_completions);
int rec_capture_valid(const rec_capture_t *p);
uint32_t rec_crc_update(uint32_t previous, const void *bytes, size_t size);
void rec_segment_header(uint8_t out[64], uint32_t segment, const uint8_t uuid[16],
                        uint32_t metadata_size, uint32_t metadata_crc);
void rec_record_header(uint8_t out[40], const char tag[4], uint64_t sequence,
                       uint64_t timestamp_us, uint32_t size, uint32_t payload_crc);
void rec_put32(uint8_t *p, uint32_t n);
#endif
