#include "../support/fixtures/runtime_io.h"
#if VISION_DRIVE_ENABLE
static uint32_t saved_words[2][VS_RECORD_WORDS];
static int store_read(unsigned slot,uint32_t *out,unsigned n)
{if(slot>1 || n!=VS_RECORD_WORDS)return 0;memcpy(out,saved_words[slot],n*sizeof(*out));return 1;}
static int store_write(unsigned slot,const uint32_t *in,unsigned n)
{if(slot>1 || n!=VS_RECORD_WORDS)return 0;memcpy(saved_words[slot],in,n*sizeof(*in));return 1;}
#endif
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
    if(which>=AUTO_RUN && which<=AUTO_INVALID) {
        vd_profile_t p;const vs_io_t io={store_read,store_write};
        memset(saved_words,255,sizeof(saved_words));vs_init(&io);
        vision_drive_init(0); app_mode_init(0,0,0); app_mode_sample(0,0,30); (void)app_mode_take_change();vision_drive_get_profile(&p);
        p.autostart_enabled=1;p.startup_delay_ms=2000;
        check(vs_save(&p),"seed complete synthetic boot profile");
    }
#endif
    app_mode_init(current_case>=WHEEL_TEXT && current_case<=WHEEL_IN_IMAGE?1:0,
                  current_case>=PROBE_TEXT && current_case<=PROBE_IN_IMAGE?1:0,ticks_base-40U);
    app_mode_sample(current_case>=WHEEL_TEXT && current_case<=WHEEL_IN_IMAGE?1:0,
                    current_case>=PROBE_TEXT && current_case<=PROBE_IN_IMAGE?1:0,ticks_base);
    (void)app_mode_take_change();
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
    if(which>=AUTO_RUN && which<=AUTO_INVALID) {
        if(which==AUTO_RUN) {
            check(!moved&&!vision_drive_is_local(),"restored profile never auto-arms after physical-key rollback");
            check(strstr(text_log,"[RUN]")==NULL,"local run does not depend on blocking telemetry");
        } else if(which==AUTO_OPEN)check(!moved&&pwm[0]==0&&pwm[1]==0&&pwm[2]==0,"physical samples cannot start restored profile");
        else check(!moved,"STOP/image/invalid boot cannot launch later");
    }
#else
    check(!moved, "default firmware never moves");
#endif
#if VISION_DRIVE_ENABLE
    if(which==DIP_OBSERVE || which==DIP_PARTIAL) {
        check(!moved&&!servo_moved,"selector/fragment cannot authorize motion");
    }
    if(which==DIP_STOP)check(moved&&!pwm[0]&&!pwm[1]&&!pwm[2]&&vision_drive_output()->state==VC_DISARMED,"raw DIP change stops runtime and cancels run");
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
    scenario(AUTO_RUN,"saved profile stays stopped",3000);
    scenario(AUTO_CANCEL,"STOP cancels countdown",3000);
    scenario(AUTO_IMAGE,"image cancels countdown",3000);
    scenario(AUTO_OPEN,"physical samples do not arm",3000);
    scenario(AUTO_INVALID,"invalid boot no retry",3000);
    scenario(DIP_OBSERVE,"DIP observer rejects arm",800);
    scenario(DIP_STOP,"DIP stops running",800);
    scenario(DIP_PARTIAL,"DIP cancels partial line",800);
#endif
    printf("ADAPTER RESULT: %d checks, %d failures (mock MCU/transport, no hardware claim)\n", checks, failures);
    return failures ? 1 : 0;
}
