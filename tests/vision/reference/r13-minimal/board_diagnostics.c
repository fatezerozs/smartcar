#include "board_io.h"
#include "board_diagnostics.h"
#include "app_mode.h"
#include "app_transport.h"
void board_diagnostics_report(void)
{
    board_io_state_t s;board_io_read(&s);
    app_console_printf("[BOARD_IO] C26=%u C27=%u mode=%s ready=%d; keys=REMOVED\r\n",
        s.dip26,s.dip27,app_mode_name(app_mode_current()),app_mode_allows(app_mode_current()));
    app_console_printf("[B11] level=%u mux=%lu dir=%lu out=%lu fast=%lu pad=0x%08lX expected:mux5/dir1/out0/fast0\r\n",
        s.buzzer_level,(unsigned long)s.buzzer_mux,(unsigned long)s.buzzer_direction,
        (unsigned long)s.buzzer_output,(unsigned long)s.buzzer_fast_bank,(unsigned long)s.buzzer_pad);
}
