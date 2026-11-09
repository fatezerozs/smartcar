#include "vision_drive_internal.h"

int vision_drive_servo_probe(uint16_t pulse_us, uint16_t hold_ms, uint32_t now)
{
    (void)pulse_us;(void)hold_ms;(void)now;return 0;
}

int vision_drive_motor_probe(uint8_t channel, uint16_t duty_permille, uint8_t level,
                             uint16_t hold_ms, uint32_t now)
{
    (void)channel;(void)duty_permille;(void)level;(void)hold_ms;(void)now;return 0;
}

void vision_drive_get_probe(vd_probe_info_t *out)
{
#if VISION_DRIVE_ENABLE
    uint32_t mask=__get_PRIMASK();__disable_irq();
    *out=vd.probe_info;out->active=vd.probe_active!=0;__set_PRIMASK(mask);
#else
    *out=vd.probe_info;
#endif
}

#if VISION_DRIVE_ENABLE
void vd_probe_poll(uint32_t now)
{
    if (vd.probe_active && (uint32_t)(now - vd.probe_started_ms) >= vd.probe_hold_ms) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq(); vd.probe_active = 0; vd_output_zero(); __set_PRIMASK(mask);
    }
}
#endif
