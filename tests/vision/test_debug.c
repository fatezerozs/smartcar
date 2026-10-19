#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include "vision_midterm.h"
#include "vision_debug.h"
#include "vision_drive.h"
#include "vision_storage.h"

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
static uint32_t pwm[3];
static int moved, servo_moved;
static unsigned usb_reads, wireless_reads, usb_sends, wireless_sends;

enum { TEXT, IMAGE, PAUSE_RESUME, RESET, STOP_CAMERA, NO_CAMERA, CLOCK_WRAP, COPY_RACE, COPY_ABA, NULL_BUFFER, IMAGE_GAP,
       TEXT_BURST, MODE_BURST, RESET_BURST, REPEAT_IMAGE, FULL_MODE, RETURN_MIDTERM, STOP_BATCH, DRIVE_IMAGE,
       FIELD_FRAGMENT, FIELD_SMUGGLE, FIELD_STOP_RUNNING,
       PROBE_TEXT, PROBE_IMAGE, PROBE_PREFIX_STOP, PROBE_SUFFIX_STOP, PROBE_IN_IMAGE,
       WHEEL_TEXT, WHEEL_IMAGE, WHEEL_PREFIX_STOP, WHEEL_SUFFIX_STOP, WHEEL_IN_IMAGE,
       AUTO_RUN, AUTO_CANCEL, AUTO_IMAGE, AUTO_OPEN, AUTO_INVALID };
#if VISION_DRIVE_ENABLE
static uint32_t saved_words[2][VS_RECORD_WORDS];
static int store_read(unsigned slot,uint32_t *out,unsigned n)
{if(slot>1 || n!=VS_RECORD_WORDS)return 0;memcpy(out,saved_words[slot],n*sizeof(*out));return 1;}
static int store_write(unsigned slot,const uint32_t *in,unsigned n)
{if(slot>1 || n!=VS_RECORD_WORDS)return 0;memcpy(saved_words[slot],in,n*sizeof(*in));return 1;}
#endif

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
void pwm_set_duty(int channel, uint32_t duty)
{
    check(channel >= 0 && channel < 3 && duty <= 10000, "bounded output");
    if (channel >= 0 && channel < 3) pwm[channel] = duty;
    if (channel == 0 && duty) moved = 1;
    if (channel == 2 && duty) servo_moved = 1;
}
void gpio_set_level(int pin, int value) { (void)pin; check(value == 1, "forward polarity only"); }
void pit_ms_init(int channel, uint32_t period) { check(channel == 1 && period == 10, "PIT1 preserved"); }
void interrupt_set_priority(int irq, uint8_t priority) { check(irq == 7 && priority == 0, "safety priority preserved"); }
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
static uint32_t mock_receive(uint8_t *data, uint32_t capacity)
{
    char command = 0;
    elapsed += 10;
#if VISION_DRIVE_ENABLE
    if(current_case>=AUTO_RUN)vision_drive_switch_sample(current_case!=AUTO_OPEN || elapsed<2300);
    vision_drive_safety_tick();
#endif
    if (elapsed >= finish_at) longjmp(stop_loop, 1);
#if VISION_DRIVE_ENABLE
    if (current_case>=PROBE_TEXT && current_case<=WHEEL_IN_IMAGE && elapsed==100) {
        vd_profile_t cfg;vision_drive_get_profile(&cfg);
        cfg.control.cruise_duty=cfg.control.duty_cap=0;
        check(vision_drive_configure(&cfg),"bench profile keeps motor cap zero");
    }
    if (current_case>=PROBE_TEXT && current_case<=WHEEL_IN_IMAGE) {
        int wheel=current_case>=WHEEL_TEXT;
        int variant=current_case-(wheel?WHEEL_TEXT:PROBE_TEXT);
        uint32_t start=variant==4?650U:200U;
        if(elapsed>=start&&elapsed<start+40){
            const char *line=wheel ? (variant==2?"!@wheel 1 30 1 300\n":variant==3?"@wheel 1 30 1 300\ns":"@wheel 1 30 1 300\n") :
                (variant==2?"!@probe 1500 700\n":variant==3?"@probe 1500 700\ns":"@probe 1500 700\n");
            uint32_t chunk=variant==3?12U:16U;
            uint32_t offset=(elapsed-start)/10*chunk,length=(uint32_t)strlen(line);
            if(offset<length){length-=offset;if(length>chunk)length=chunk;if(length>capacity)length=capacity;memcpy(data,line+offset,length);return length;}
        }
        if(variant==4&&elapsed==150)command='g';
        if(variant==1&&elapsed==400)command='g';
    }
#endif
    if (current_case != NO_CAMERA && elapsed < feed_until) {
        if (current_case == NULL_BUFFER) mt9v03x_image = NULL;
        else {
            int id = (elapsed / 10) % 2;
            fill_camera(id, (current_case == IMAGE_GAP && elapsed > 200) ||
                        (current_case==AUTO_INVALID && elapsed<2200) ? 70 : 0);
            mt9v03x_image = camera[id];
        }
        mt9v03x_finish_flag = 1;
    }
    if ((current_case == FULL_MODE || current_case == RETURN_MIDTERM) && elapsed == 100) command = 'f';
    if ((current_case == FULL_MODE || current_case == RETURN_MIDTERM) && elapsed == 200) command = 'a';
    if (current_case == RETURN_MIDTERM && elapsed == 300) command = 'm';
    if (current_case == RETURN_MIDTERM && elapsed == 400) command = 'a';
    if (current_case == STOP_BATCH && elapsed == 200 && capacity >= 2) { memcpy(data,"sa",2); return 2; }
    if (current_case == DRIVE_IMAGE && elapsed == 200) command = 'a';
    if (current_case == DRIVE_IMAGE && elapsed == 400) command = 'g';
    if (current_case == FIELD_STOP_RUNNING && elapsed == 200) command = 'a';
    if ((current_case == FIELD_FRAGMENT || current_case == FIELD_SMUGGLE || current_case == FIELD_STOP_RUNNING) &&
        elapsed >= 400 && elapsed < 420) {
        const char *line = current_case == FIELD_SMUGGLE ? "@gain 0.7 0.02 0.2 a\n" : "@gain 0.7 0.02 0.2\n";
        uint32_t offset=(elapsed-400)/10*16, length=(uint32_t)strlen(line);
        if (offset<length) {
            length-=offset;if(length>capacity)length=capacity;
            memcpy(data,line+offset,length);return length;
        }
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
    if (current_case==AUTO_CANCEL && elapsed==1500)command='s';
    if (current_case==AUTO_IMAGE && elapsed==1000)command='g';
    if (command && capacity) {
        char lower = command >= 'A' && command <= 'Z' ? (char)(command - 'A' + 'a') : command;
        if (lower == 't' || lower == 'g' || lower == 'b' || lower == 'o' || lower == 'p') expected_mode = lower;
        data[0] = (uint8_t)command;
        return 1;
    }
    return 0;
}
uint32_t debug_read_ring_buffer(uint8_t *data, uint32_t capacity)
{ ++usb_reads; return mock_receive(data, capacity); }
uint32_t wireless_uart_read_buffer(uint8_t *data, uint32_t capacity)
{ ++wireless_reads; return mock_receive(data, capacity); }
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
static uint32_t mock_image(const uint8_t *pixels, uint32_t length)
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
#if VISION_DRIVE_ENABLE
    check(pwm[0] == 0 && pwm[1] == 0 && pwm[2] == 0, "image transmission is disarmed");
#endif
    elapsed += 490; /* 94*60 bytes at 115200, 8N1; test blocking output timing. */
    return 0;
}
uint32_t debug_send_buffer(const uint8_t *pixels, uint32_t length)
{ ++usb_sends; return mock_image(pixels,length); }
uint32_t wireless_uart_send_buffer(const uint8_t *data, uint32_t length)
{
    ++wireless_sends;
    if (wire_phase || (length == 12 && data[0] == 0xAA)) return mock_image(data,length);
    if (expected_mode != 't') ++text_during_image;
    check(log_length + length < sizeof(text_log), "wireless text buffer capacity");
    if (log_length + length < sizeof(text_log)) {
        memcpy(text_log + log_length,data,length); log_length += length; text_log[log_length]=0;
    }
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
    memset(pwm,0,sizeof(pwm)); moved=0;servo_moved=0;
    usb_reads=wireless_reads=usb_sends=wireless_sends=0;
    memset(text_log, 0, sizeof(text_log));
    expected_mode = 't'; mt9v03x_finish_flag = 0; mt9v03x_image = NULL;
#if VISION_DRIVE_ENABLE
    vs_init(NULL);
    if(which>=AUTO_RUN) {
        vd_profile_t p;const vs_io_t io={store_read,store_write};
        memset(saved_words,255,sizeof(saved_words));vs_init(&io);
        vision_drive_init(0);vision_drive_get_profile(&p);
        p.autostart_enabled=1;p.startup_delay_ms=2000;
        check(vs_save(&p),"seed complete synthetic boot profile");
    }
#endif
    if (setjmp(stop_loop) == 0) vision_debug_run();
    check(irq_mask == (which == CLOCK_WRAP ? 1U : 0U), "interrupt mask restored");
    check(text_during_image == 0, "no text in image/pause mode");
    check(wire_phase == 0, "each V2 header has a complete payload");
    check(strstr(text_log, "[VISION2]") != NULL, "firmware build marker");
    check(strstr(text_log, "SeekFree V2") != NULL, "explicit V2 wire-protocol marker");
#if VISION_WIRELESS_UART_ENABLE
    check(wireless_reads>0 && wireless_sends>0 && usb_reads==0 && usb_sends==0, "wireless owns all runtime IO");
#else
    check(usb_reads>0 && wireless_reads==0 && wireless_sends==0, "USB runtime does not mix wireless commands");
#endif
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
    if (which == FULL_MODE) {
        check(strstr(text_log,"mode=FULL_OBSERVER") != NULL, "full observer selected");
        check(!moved, "full observer cannot arm midterm motor");
    }
    if (which == STOP_BATCH) check(!moved, "stop dominates later arm in same batch");
#if VISION_DRIVE_ENABLE
    if (which == RETURN_MIDTERM || which == DRIVE_IMAGE) check(moved, "midterm can arm with synthetic calibration");
    if (which == FIELD_FRAGMENT || which == FIELD_STOP_RUNNING) {
        vd_profile_t cfg; vision_drive_get_profile(&cfg);
        check(strstr(text_log,"[FIELD] OK;")!=NULL,"fragmented field command acknowledged");
        check(cfg.control.kp==0.7f,"fragmented field command commits requested gain");
        check(vision_drive_output()->state==VC_DISARMED,"field command leaves disarmed");
    }
    if (which == FIELD_SMUGGLE) {
        check(strstr(text_log,"[FIELD] REJECT")!=NULL,"trailing arm rejected");
        check(!moved,"parameter contents cannot arm motor");
    }
    if (which == FIELD_STOP_RUNNING) check(moved && pwm[0]==0,"field start stops running car");
    if (which>=PROBE_TEXT && which<=WHEEL_IN_IMAGE) {
        int wheel=which>=WHEEL_TEXT,variant=which-(wheel?WHEEL_TEXT:PROBE_TEXT);
        check(pwm[0]==0&&pwm[1]==0&&pwm[2]==0,"bounded bench finishes fully disabled");
        if(variant==0||variant==1)check(wheel?(moved&&!servo_moved):(!moved&&servo_moved),"bench actuates only explicitly requested output");
        else check(!moved&&!servo_moved,"STOP or image context rejects whole bench line");
    }
    if(which>=AUTO_RUN) {
        if(which==AUTO_RUN) {
            check(moved&&vision_drive_is_local(),"restored profile starts after 2s without remote heartbeats");
            check(strstr(text_log,"[RUN]")==NULL,"local run does not depend on blocking telemetry");
        } else if(which==AUTO_OPEN)check(moved&&pwm[0]==0&&pwm[1]==0&&pwm[2]==0,"physical opening stops restored local run");
        else check(!moved,"STOP/image/invalid boot cannot launch later");
    }
#else
    check(!moved, "default firmware never moves");
#endif
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
    scenario(FULL_MODE, "full observer no drive", 1300);
    scenario(RETURN_MIDTERM, "return midterm then arm", 650);
    scenario(STOP_BATCH, "stop wins over arm", 650);
    scenario(DRIVE_IMAGE, "image disarms drive", 1300);
#if VISION_DRIVE_ENABLE
    scenario(FIELD_FRAGMENT,"fragmented field gain",800);
    scenario(FIELD_SMUGGLE,"field trailing arm",800);
    scenario(FIELD_STOP_RUNNING,"field stops running",800);
    scenario(PROBE_TEXT,"probe bounded text",1200);
    scenario(PROBE_IMAGE,"image cancels probe",1300);
    scenario(PROBE_PREFIX_STOP,"STOP before probe",1200);
    scenario(PROBE_SUFFIX_STOP,"STOP after probe",1200);
    scenario(PROBE_IN_IMAGE,"no probe in image",1300);
    scenario(WHEEL_TEXT,"wheel bounded text",1200);
    scenario(WHEEL_IMAGE,"image cancels wheel",1300);
    scenario(WHEEL_PREFIX_STOP,"STOP before wheel",1200);
    scenario(WHEEL_SUFFIX_STOP,"STOP after wheel",1200);
    scenario(WHEEL_IN_IMAGE,"no wheel in image",1300);
    scenario(AUTO_RUN,"saved C12 startup",3000);
    scenario(AUTO_CANCEL,"STOP cancels countdown",3000);
    scenario(AUTO_IMAGE,"image cancels countdown",3000);
    scenario(AUTO_OPEN,"C12 stops local run",3000);
    scenario(AUTO_INVALID,"invalid boot no retry",3000);
#endif
    printf("ADAPTER RESULT: %d checks, %d failures (mock MCU/transport, no hardware claim)\n", checks, failures);
    return failures ? 1 : 0;
}
