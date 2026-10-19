#include "main.h"
#include "vision_debug.h"
#include "vision_profile.h"
#include "vision_storage.h"

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
    gpio_init(VISION_START_SWITCH_PIN,GPI,GPIO_HIGH,GPI_PULL_UP);
    pwm_init(MOTOR1_PWM, VISION_MOTOR_PWM_HZ, 0);
    gpio_init(MOTOR1_DIR, GPO, GPIO_LOW, GPO_PUSH_PULL);
    pwm_init(MOTOR2_PWM, VISION_MOTOR_PWM_HZ, 0);
    gpio_init(MOTOR2_DIR, GPO, GPIO_LOW, GPO_PUSH_PULL);
    pwm_init(SERVO_MOTOR_PWM, SERVO_MOTOR_FREQ, 0);
    printf("[BOOT] stage=PWM_OFF build=%s\r\n",VISION_FIELD_BUILD);
    printf("[BOOT] stage=CAMERA_INIT outputs=OFF\r\n");
    while (mt9v03x_init()) {
        printf("[VISION2] camera init failed; outputs OFF; retrying\r\n");
        system_delay_ms(VISION_CAMERA_RETRY_MS);
    }
    printf("[VISION2] camera init OK; encoder-free runtime\r\n");
}

int main(void)
{
    clock_init(SYSTEM_CLOCK_600M);
    /* Boot-relative uptime: startup delay includes camera initialization.
     * Motor-board power is not sensed; turn driver power on BEFORE the MCU.
     * If initialization overruns the deadline, automatic start is blocked. */
    timer_init(GPT_TIM_1, TIMER_MS);
    timer_start(GPT_TIM_1);
    debug_init(); /* 保留 debug UART RX 环形缓冲区和中断，非轮询串口替代。 */
    printf("[BOOT] stage=UART_READY build=%s baud=%lu\r\n",VISION_FIELD_BUILD,(unsigned long)DEBUG_UART_BAUDRATE);
    system_delay_ms(VISION_POWER_SETTLE_MS);
    board_init();
#if VISION_WIRELESS_UART_ENABLE
    while (wireless_uart_init()) {
        printf("[VISION2] wireless init failed; outputs OFF\r\n");
        system_delay_ms(VISION_CAMERA_RETRY_MS);
    }
#endif
    interrupt_global_enable(0);
    vision_storage_board_init();
    printf("[BOOT] stage=CONTROL_READY outputs=LOCKED\r\n");
    /* drive_init 在主循环中按编译开关启动 PIT1 独立安全租约。
     * UART8 无线模块及 CSI/VSYNC 回调仍由原 isr.c 分发。 */
    vision_debug_run();
    for (;;) { } /* 不允许意外回落至旧整车控制路径。 */
}
