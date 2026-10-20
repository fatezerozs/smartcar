#include "main.h"
#include "app_runtime.h"
#include "board_io.h"
#include "board_diagnostics.h"
#include "board_camera.h"
#include "vision_profile.h"
#include "vision_storage.h"
#include "app_link.h"
#include "app_transport.h"

/* 新入口只有一条运行链：摄像头 -> 视觉/贝叶斯 -> 中期控制 -> 安全适配层。
 * 旧环岛定时窗口、第二次斑马线强停、固定补线和电机速度 PID 不再运行。
 * 原 Camera.c / Island.c / pid.c 保留在原目录，已从此 MDK 目标移除。
 * 引脚保持 main.h 的实际定义；输出默认锁定，未标定绝不自动发车。 */
uint8 image_two_value[MT9V03X_H][MT9V03X_W];
uint8_t Left_Line[MT9V03X_H], Right_Line[MT9V03X_H], Mid_Line[MT9V03X_H];

/* 初始化时先建立零占空比，摄像头失败/重试期间也不会产生运动。
 * 不初始化故障编码器、不初始化 TFT、不启动原 PIT0 速度环。 */
static void board_init(void)
{
    pwm_init(MOTOR1_PWM, VISION_MOTOR_PWM_HZ, 0);
    gpio_init(MOTOR1_DIR, GPO, GPIO_LOW, GPO_PUSH_PULL);
    pwm_init(MOTOR2_PWM, VISION_MOTOR_PWM_HZ, 0);
    gpio_init(MOTOR2_DIR, GPO, GPIO_LOW, GPO_PUSH_PULL);
    pwm_init(SERVO_MOTOR_PWM, SERVO_MOTOR_FREQ, 0);
    printf("[BOOT] stage=PWM_OFF build=%s\r\n",VISION_FIELD_BUILD);
    board_camera_reset(); /* Camera service runs only in OBSERVE/DRIVE modes. */
}

int main(void)
{
    clock_init(SYSTEM_CLOCK_600M);
    /* Boot-relative uptime: startup delay includes camera initialization.
     * Motor-board power is not sensed; turn driver power on BEFORE the MCU.
     * If initialization overruns the deadline, automatic start is blocked. */
    timer_init(GPT_TIM_1, TIMER_MS);
    timer_start(GPT_TIM_1);
    board_io_init();
    debug_init(); /* UART1 and printf setup; dual RX is routed by isr.c below. */
#if VISION_DUAL_UART_ENABLE
    {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        app_transport_init(); /* Reset rings before either RX producer runs. */
        __set_PRIMASK(mask);
    }
#endif
    printf("[BOOT] stage=UART_READY build=%s baud=%lu\r\n",VISION_FIELD_BUILD,(unsigned long)DEBUG_UART_BAUDRATE);
    system_delay_ms(VISION_POWER_SETTLE_MS);
    board_init();
    printf("[BOOT] io=UART_CONTROL_BASE_V1 built=%s %s\r\n",__DATE__,__TIME__);
    board_diagnostics_report();
#if VISION_LINK_MVP
    interrupt_global_enable(0);
    app_link_mvp_run(); /* No storage restore, camera init, PIT lease or motion. */
#endif
#if VISION_WIRELESS_UART_ENABLE && !VISION_DUAL_UART_ENABLE
    while (wireless_uart_init()) {
        printf("[VISION2] wireless init failed; outputs OFF\r\n");
        system_delay_ms(VISION_CAMERA_RETRY_MS);
    }
#endif
    interrupt_global_enable(0);
    vision_storage_board_init();
    printf("[BOOT] stage=CONTROL_READY outputs=LOCKED\r\n");
    /* drive_init 在主循环中按编译开关启动 PIT1 独立安全租约。
     * Dual UART RX uses app_link; CSI/VSYNC dispatch remains unchanged. */
    board_mode_sample();
    app_runtime_run();
    for (;;) { } /* 不允许意外回落至旧整车控制路径。 */
}
