#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include "vision_midterm.h"
#include "vision_debug.h"

/* Actual vision_debug.c runs unchanged. Only MCU time, camera delivery,
 * interrupts and UART transport are simulated; this is not a hardware test. */
volatile uint8_t mt9v03x_finish_flag;
uint8_t (*mt9v03x_image)[VISION_WIDTH];
uint8_t image_two_value[VISION_HEIGHT][VISION_WIDTH];
uint8_t Left_Line[VISION_HEIGHT], Right_Line[VISION_HEIGHT], Mid_Line[VISION_HEIGHT];
static uint8_t camera[2][VISION_HEIGHT][VISION_WIDTH];
static jmp_buf stop_loop;
static uint32_t elapsed, ticks_base, finish_at, feed_until, irq_mask, barriers;
static int current_case, failures, checks, packets, text_during_image;
static char text_log[32768], expected_mode;
static size_t log_length;
static uint8_t wire_header[12];
static int wire_phase;
static const char *case_name;

enum { TEXT, IMAGE, PAUSE_RESUME, RESET, STOP_CAMERA, NO_CAMERA, CLOCK_WRAP, COPY_RACE, COPY_ABA, NULL_BUFFER, IMAGE_GAP,
       TEXT_BURST, MODE_BURST, RESET_BURST, REPEAT_IMAGE };

static void check(int ok, const char *what)
{
    ++checks;
    if (!ok) { ++failures; printf("FAIL [%s]: %s\n", case_name, what); }
}
static void fill_camera(int id, int shift)
{
    int x, y;
    for (y = 0; y < VISION_HEIGHT; ++y) {
        int width = 40 + y * 3 / 4, center = 94 + shift;
        for (x = 0; x < VISION_WIDTH; ++x)
            camera[id][y][x] = (uint8_t)(x >= center - width / 2 && x <= center + width / 2 ? 210 : 30);
    }
}
uint32_t timer_get(int index) { (void)index; return ticks_base + elapsed; }
uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1; }
void __set_PRIMASK(uint32_t mask) { irq_mask = mask; }
void __DMB(void)
{
    ++barriers;
    if ((current_case == COPY_RACE || current_case == COPY_ABA) && barriers % 2 == 0) {
        if (current_case == COPY_RACE)
            mt9v03x_image = mt9v03x_image == camera[0] ? camera[1] : camera[0];
        mt9v03x_finish_flag = 1;
    }
}
uint32_t debug_read_ring_buffer(uint8_t *data, uint32_t capacity)
{
    char command = 0;
    elapsed += 10;
    if (elapsed >= finish_at) longjmp(stop_loop, 1);
    if (current_case != NO_CAMERA && elapsed < feed_until) {
        if (current_case == NULL_BUFFER) mt9v03x_image = NULL;
        else {
            int id = (elapsed / 10) % 2;
            fill_camera(id, current_case == IMAGE_GAP && elapsed > 200 ? 70 : 0);
            mt9v03x_image = camera[id];
        }
        mt9v03x_finish_flag = 1;
    }
    if ((current_case == TEXT_BURST && elapsed == 100) ||
        (current_case == MODE_BURST && elapsed == 100) ||
        (current_case == RESET_BURST && elapsed == 500)) {
        const char *burst = current_case == TEXT_BURST ? "TTTTTTTTTTTTTTTT" :
                            current_case == MODE_BURST ? "?r?G?rb" : "rrrrrrrrrrrrrrrr";
        uint32_t length = (uint32_t)strlen(burst);
        if (length > capacity) length = capacity;
        memcpy(data, burst, length);
        if (current_case == MODE_BURST) expected_mode = 'b';
        return length;
    }
    if ((current_case == IMAGE || current_case == IMAGE_GAP) && elapsed == 10) command = 'g';
    if (current_case == IMAGE && elapsed >= 600 && expected_mode == 'g') command = 'b';
    if (current_case == IMAGE && elapsed >= 1300 && expected_mode == 'b') command = 'o';
    if (current_case == IMAGE && elapsed >= 2200 && expected_mode != 't') command = 't';
    if (current_case == PAUSE_RESUME && elapsed == 200) command = 'p';
    if (current_case == PAUSE_RESUME && elapsed == 700) command = 'T';
    if (current_case == RESET && elapsed == 500) command = 'r';
    if (current_case == IMAGE_GAP && elapsed >= 1700 && expected_mode != 't') command = 't';
    if (current_case == REPEAT_IMAGE) command = 'G';
    if (command && capacity) {
        char lower = command >= 'A' && command <= 'Z' ? (char)(command - 'A' + 'a') : command;
        if (lower == 't' || lower == 'g' || lower == 'b' || lower == 'o' || lower == 'p') expected_mode = lower;
        data[0] = (uint8_t)command;
        return 1;
    }
    return 0;
}
int vision_test_printf(const char *format, ...)
{
    va_list args;
    int n;
    if (expected_mode != 't' || wire_phase) ++text_during_image;
    va_start(args, format);
    n = vsnprintf(text_log + log_length, sizeof(text_log) - log_length, format, args);
    va_end(args);
    if (n > 0 && (size_t)n < sizeof(text_log) - log_length) log_length += (size_t)n;
    else check(0, "mock text buffer capacity");
    return n;
}
uint32_t debug_send_buffer(const uint8_t *pixels, uint32_t length)
{
    /* Test the actual UART bytes, not a mock detector API call. Header bytes
     * independently derived from the pinned official V2 RT1064 config. */
    static const uint8_t expected_header[12] = {0xAA,0x49,0x02,0x02,0x5E,0,0x3C,0,0x01,0,0,0};
    check(expected_mode == 'g' || expected_mode == 'b' || expected_mode == 'o',
          "UART image bytes sent only in requested mode");
    if (!wire_phase) {
        check(length == sizeof(expected_header), "V2 header is exactly twelve bytes");
        check(pixels && length == sizeof(expected_header) &&
              memcmp(pixels, expected_header, sizeof(expected_header)) == 0,
              "V2 command/type/endian/checksum/reserved bytes match official config");
        if (pixels && length == sizeof(wire_header)) memcpy(wire_header, pixels, sizeof(wire_header));
        wire_phase = 1;
        return 0;
    }
    check(length == 94U * 60U, "V2 grayscale payload has 5640 bytes");
    wire_phase = 0;
    int expected_pixel = expected_mode == 'o' ? 0 : expected_mode == 'b' ? 255 :
                         current_case == IMAGE_GAP && elapsed > 200 ? 30 : 210;
    check(pixels && length == 5640 && pixels[30 * 94 + 47] == expected_pixel,
          "wire payload matches selected preview mode");
    if (pixels && length == 5640 && expected_mode == 'g') {
        int mismatch = 0, x, y;
        for (y = 0; y < 60; ++y)
            for (x = 0; x < 94; ++x)
                if (pixels[y * 94 + x] != mt9v03x_image[y * 2][x * 2]) mismatch = 1;
        check(!mismatch, "whole raw preview payload is the sampled camera frame");
    }
    if (pixels && length == 5640 && current_case == IMAGE && packets == 0) {
        FILE *packet = fopen("v2_gray_packet.bin", "wb");
        check(packet != NULL, "wire fixture opens");
        if (packet) { fwrite(wire_header, 1, sizeof(wire_header), packet); fwrite(pixels, 1, length, packet); fclose(packet); }
    }
    ++packets;
    elapsed += 490; /* 94*60 bytes at 115200, 8N1; test blocking output timing. */
    return 0;
}
static void scenario(int which, const char *name, uint32_t duration)
{
    current_case = which; case_name = name;
    elapsed = 0; finish_at = duration; feed_until = duration;
    ticks_base = which == CLOCK_WRAP ? UINT32_MAX - 400U : 0;
    if (which == RESET || which == STOP_CAMERA || which == RESET_BURST) feed_until = 400;
    irq_mask = which == CLOCK_WRAP ? 1 : 0;
    barriers = 0; packets = text_during_image = wire_phase = 0; log_length = 0;
    memset(text_log, 0, sizeof(text_log));
    expected_mode = 't'; mt9v03x_finish_flag = 0; mt9v03x_image = NULL;
    if (setjmp(stop_loop) == 0) vision_debug_run();
    check(irq_mask == (which == CLOCK_WRAP ? 1U : 0U), "interrupt mask restored");
    check(text_during_image == 0, "no text in image/pause mode");
    check(wire_phase == 0, "each V2 header has a complete payload");
    check(strstr(text_log, "[VISION2]") != NULL, "firmware build marker");
    check(strstr(text_log, "SeekFree V2") != NULL, "explicit V2 wire-protocol marker");
    if (which == TEXT || which == CLOCK_WRAP || which == PAUSE_RESUME)
        check(strstr(text_log, "valid=1 state=TRACK") != NULL, "working camera text result");
    if (which == IMAGE) {
        check(packets >= 2 && packets <= 4, "image output throttled");
        check(strstr(text_log, "valid=1 state=TRACK") != NULL, "return to text after image");
    }
    if (which == RESET) {
        check(strstr(text_log, "history cleared") != NULL, "reset acknowledged");
        check(strstr(text_log, "valid=0 state=NO_LANE") != NULL, "reset rejects stale result");
    }
    if (which == STOP_CAMERA || which == NO_CAMERA || which == COPY_RACE || which == COPY_ABA || which == NULL_BUFFER) {
        check(strstr(text_log, "NO_FRAME") != NULL, "timeout still reached");
        check(Mid_Line[90] == VISION_INVALID_X, "timeout clears exported coordinates");
    }
    if (which == IMAGE_GAP) {
        check(packets >= 1, "gap test has image transmission");
        check(strstr(text_log, "valid=0 state=NO_LANE") != NULL, "blocking send cannot extend old-width inference");
        check(strstr(text_log, "history_reset=0") == NULL, "wall-clock gap reset counted");
    }
    if (which == TEXT_BURST || which == RESET_BURST) {
        const char *phrase = which == TEXT_BURST ? "t=text" : "history cleared";
        const char *found = text_log;
        int count = 0;
        while ((found = strstr(found, phrase)) != NULL) { ++count; found += strlen(phrase); }
        check(count == 1, which == TEXT_BURST ? "repeated text mode does not spam help" :
                                               "reset burst acknowledged once");
    }
    if (which == MODE_BURST) {
        check(packets >= 1, "mixed burst selects final image mode");
        check(strstr(text_log, "history cleared") == NULL, "no reset acknowledgement in final image mode");
    }
    if (which == REPEAT_IMAGE) check(packets == 2, "repeated image command cannot bypass output throttle");
    printf("%-22s packets=%d elapsed=%lu: complete\n", name, packets, (unsigned long)elapsed);
}
int main(void)
{
    scenario(TEXT, "text", 1300);
    scenario(IMAGE, "image/text", 3000);
    scenario(PAUSE_RESUME, "pause/uppercase resume", 1300);
    scenario(RESET, "history reset", 1300);
    scenario(STOP_CAMERA, "camera stops", 3000);
    scenario(NO_CAMERA, "no camera frames", 2600);
    scenario(CLOCK_WRAP, "32-bit clock wraps", 1400);
    scenario(COPY_RACE, "copy race rejection", 2600);
    scenario(COPY_ABA, "two-buffer cycle", 2600);
    scenario(NULL_BUFFER, "null camera buffer", 2600);
    scenario(IMAGE_GAP, "image/history gap", 2600);
    scenario(TEXT_BURST, "text command burst", 1300);
    scenario(MODE_BURST, "mixed mode burst", 2300);
    scenario(RESET_BURST, "reset command burst", 1300);
    scenario(REPEAT_IMAGE, "repeat image command", 2300);
    printf("ADAPTER RESULT: %d checks, %d failures (mock MCU/transport, no hardware claim)\n", checks, failures);
    return failures ? 1 : 0;
}
