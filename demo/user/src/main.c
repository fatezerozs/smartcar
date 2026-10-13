#include "main.h" // 所有引脚信息更改在main.h里改宏
#include "zf_common_headfile.h"
#include "math.h"

// Test mode switches (set one of them to 1 when doing module tests)
#ifndef TEST_SERVO
#define TEST_SERVO 0
#endif
#ifndef TEST_MOTOR
#define TEST_MOTOR 0
#endif
#ifndef TEST_UART
#define TEST_UART  0
#endif

// *************************** 例程硬件连接说明 ***************************
/*

            连接好电机、主板、驱动板、摄像头、编码器
            (请确保全部正确连接后在使用此例程，所有外设均会使用)
*/
// *************************** 例程测试说明 ***************************
/*
            本例程包含功能：
            1.初始化摄像头并采集图像进行大津法二值化
            2.初始化屏幕并显示关键信息
            3.根据偏差计算最基本的舵机打角
            4.运用最基本pid参数和pid算法使电机旋转 (pid参数在pid.c中)


            注意！注意！注意！注意！
            注意！注意！注意！注意！
            注意！注意！注意！注意！

            1.请仔细阅读main.h的所有注释，确保所有宏都和自己的硬件连接对应
            2.若屏幕显示二值化后的图像有缺陷，可能不能正常走直线，
            可以自行更改二值化算法或找光环境良好且均匀的环境测试例程（例程不提供更多的二值化算法）

*/

// **************************** 代码区域 ****************************

// 舵机动作角度中值
const float servo_motor_duty_middle = (SERVO_MOTOR_R_MAX + SERVO_MOTOR_L_MAX) / 2.f; 
short hist_gram[256];
uint8 image_two_value[MT9V03X_H][MT9V03X_W];//二值化后的原数组
uint8_t Left_Line[MT9V03X_H]; //左边线数组
uint8_t Right_Line[MT9V03X_H];//右边线数组
uint8_t Mid_Line[MT9V03X_H];//中线数组

// 记录斑马线检测次数和状态
int zebraCrossingCount = 0;
bool isFirstZebraPassed = false;
bool shouldStop = false;
uint8_t Zebra_Flag = 0;

// 斑马线状态枚举
typedef enum {
    ZEBRA_NONE = 0,       // 无斑马线
    ZEBRA_FIRST_DETECTED, // 第一次检测到斑马线
    ZEBRA_FIRST_PASSED,   // 已通过第一次斑马线
    ZEBRA_SECOND_DETECTED,// 第二次检测到斑马线
    ZEBRA_SECOND_PASSED,  // 已通过第二次斑马线（新增状态，用于标记第二次通过）
    ZEBRA_STOPPED         // 已停车
} zebra_state_t;
// 斑马线检测参数结构体（保持不变）
struct zebra_struct {
    zebra_state_t state;            // 斑马线检测状态
    uint16_t detect_counter;        // 连续检测计数器（防误检）
    uint16_t pass_counter;          // 通过斑马线计数器
    uint16_t ignore_line_counter;   // 忽略边界线计数（防止斑马线干扰）  
    const uint8_t detect_row_start; // 检测区域起始行（图像中下部）
    const uint8_t detect_row_end;   // 检测区域结束行
    const uint8_t jump_threshold;   // 像素跳变次数阈值（斑马线特征）
    const uint8_t confirm_count;    // 连续检测确认次数（防误检）
    const uint16_t pass_delay;      // 通过斑马线所需的延迟计数（统一用于两次通过判断）
    const uint16_t ignore_line_time;// 忽略边界线的时间（防止干扰）
} zebra = {
    .state = ZEBRA_NONE,
    .detect_counter = 0,
    .pass_counter = 0,
    .ignore_line_counter = 0,
    .detect_row_start = 45, 					  // 图像下部区域开始
    .detect_row_end = MT9V03X_H - 10,   // 图像下部区域结束
    .jump_threshold = 10,               // 每行平均跳变阈值
    .confirm_count = 2,                 // 连续2帧确认（防误检）
    .pass_delay = 5,                    // 通过斑马线需要的周期（和第一次保持一致）
    .ignore_line_time = 1               // 检测到斑马线后忽略边界线1个周期
};
// 斑马线检测函数
uint8_t detect_zebra(uint8_t* image)
{
    // 已停车后不再检测
    if (zebra.state == ZEBRA_STOPPED) {
        return 0;
    }

    uint16_t total_jump = 0;  // 检测区域内总跳变次数
    uint8_t prev_pixel = 0;   // 上一个像素值
    uint8_t valid_rows = 0;   // 有效行数计数

    // 遍历检测区域的行和列，统计像素跳变次数
    for (uint8_t row = zebra.detect_row_start; row < zebra.detect_row_end; row++)
    {
        uint16_t row_jump = 0;
        prev_pixel = image[row * MT9V03X_W];  // 行首像素

        for (uint8_t col = 20; col < MT9V03X_W-20; col++)
        {
            uint8_t curr_pixel = image[row * MT9V03X_W + col];
            // 二值化图像中，0（黑）和255（白）跳变即为斑马线特征
            if (curr_pixel != prev_pixel)
            {
                row_jump++;
                prev_pixel = curr_pixel;
            }
        }

        // 过滤掉跳变次数异常多的行（可能是边界线干扰）
        if (row_jump > 5 && row_jump < 50) {  // 合理范围：5-50次跳变
            total_jump += row_jump;
            valid_rows++;
        }
    }

    if (valid_rows == 0) return 0;

    // 计算检测区域的平均每行跳变次数
    uint8_t avg_jump = total_jump / valid_rows;

    // 跳变次数超过阈值，认为检测到斑马线特征
    return (avg_jump > zebra.jump_threshold && avg_jump < 40) ? 1 : 0;
}
// 斑马线状态处理函数
void zebra_handle(void)
{
    switch (zebra.state)
    {
    case ZEBRA_NONE:
				Zebra_Flag = 0;
        if (detect_zebra((uint8_t*)image_two_value))
        {
            zebra.detect_counter++;
            // 连续检测到指定次数，确认第一次斑马线
            if (zebra.detect_counter >= zebra.confirm_count)
            {
                zebra.state = ZEBRA_FIRST_DETECTED;
                zebra.detect_counter = 0;
                zebra.ignore_line_counter = zebra.ignore_line_time; // 开始忽略边界线
                tft180_show_string(130, 0, "1D");
            }
        }
        else
        {
            zebra.detect_counter = 0;  // 未检测到，计数器清零
        }
        break;

    case ZEBRA_FIRST_DETECTED:
        // 第一次检测到斑马线，保持直线行驶并判断是否通过
				Zebra_Flag = 1;
        if (zebra.ignore_line_counter > 0) {
            zebra.ignore_line_counter--;
        }

        if (!detect_zebra((uint8_t*)image_two_value))
        {
            zebra.pass_counter++;
            // 连续未检测到斑马线一段时间，认为已通过第一次斑马线
            if (zebra.pass_counter >= zebra.pass_delay)
            {
                zebra.state = ZEBRA_FIRST_PASSED;
                zebra.pass_counter = 0; // 重置计数器，用于第二次通过判断
                tft180_show_string(130, 20, "1P");
            }
        }
        else
        {
            zebra.pass_counter = 0;  // 仍检测到，重置通过计数器
            zebra.ignore_line_counter = zebra.ignore_line_time; // 重新忽略边界线
        }
        break;

    case ZEBRA_FIRST_PASSED:
				Zebra_Flag = 0;
        // 已通过第一次，等待第二次斑马线检测
        if (detect_zebra((uint8_t*)image_two_value))
        {
            zebra.detect_counter++;
            // 连续检测到指定次数，确认第二次斑马线
            if (zebra.detect_counter >= zebra.confirm_count)
            {
                zebra.state = ZEBRA_SECOND_DETECTED;
                zebra.detect_counter = 0;
                zebra.ignore_line_counter = zebra.ignore_line_time; // 开始忽略边界线
                tft180_show_string(130, 40, "2D");
            }
        }
        else
        {
            zebra.detect_counter = 0;  // 未检测到，计数器清零
        }
        break;

    case ZEBRA_SECOND_DETECTED:
				Zebra_Flag = 1;
        // 第二次检测到斑马线，保持直线行驶并判断是否通过（完全仿照第一次通过逻辑）
        if (zebra.ignore_line_counter > 0) {
            zebra.ignore_line_counter--;
        }

        if (!detect_zebra((uint8_t*)image_two_value))
        {
            zebra.pass_counter++;
            // 连续未检测到斑马线一段时间，认为已通过第二次斑马线 → 立即停车
            if (zebra.pass_counter >= zebra.pass_delay)
            {
                zebra.state = ZEBRA_SECOND_PASSED;
                zebra.pass_counter = 0;
								tft180_show_string(130, 60, "P");
                zebra.state = ZEBRA_STOPPED; // 直接切换到停车状态
            }
        }
        else
        {
            zebra.pass_counter = 0;  // 仍检测到，重置通过计数器
            zebra.ignore_line_counter = zebra.ignore_line_time; // 重新忽略边界线
        }
        break;

    case ZEBRA_SECOND_PASSED:
				Zebra_Flag = 0;
        // 过渡状态，直接切换到停车（冗余保障）
        zebra.state = ZEBRA_STOPPED;
        break;

    case ZEBRA_STOPPED:
				Zebra_Flag = 0;
        // 持续停止电机
        pwm_set_duty(MOTOR1_PWM, 0);
        pwm_set_duty(MOTOR2_PWM, 0);
        
        // 设置电机方向引脚为低，避免驱动板异常
        gpio_set_level(MOTOR1_DIR, GPIO_LOW);
        gpio_set_level(MOTOR2_DIR, GPIO_LOW);
        
        // 关闭PID输出
        speed_pid_l.enable = false;
        speed_pid_r.enable = false;
        break;
    }
}

void Init()
{

    // 初始化flash, 储存参数. 一个扇区有8页, 一页可以储存4096字节, 一个参数占4个字节, 因此一页最多只能存64个参数
    flash_init();

    tft180_set_dir(TFT180_CROSSWISE);                                           // 需要先横屏 不然显示不下
    tft180_init();
	
    // 电机初始化
    pwm_init(MOTOR1_PWM, 17000, 0); // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_init(MOTOR1_DIR, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高
    pwm_init(MOTOR2_PWM, 17000, 0); // PWM 通道初始化频率 17KHz 占空比初始为 0
    gpio_init(MOTOR2_DIR, GPO, GPIO_HIGH, GPO_PUSH_PULL); // GPIO 初始化为输出 默认上拉输出高

    // 舵机
    pwm_init(SERVO_MOTOR_PWM, SERVO_MOTOR_FREQ, 0);


    // mt9v03x摄像头初始化
#if !(TEST_SERVO || TEST_MOTOR || TEST_UART)
    while (1)
    {
        if (mt9v03x_init())
            tft180_show_string(0, 16, "mt9v03x reinit.");
        else
            break;
        system_delay_ms(1000);
    }
    tft180_show_string(0, 16, "init success.");
#else
    tft180_show_string(0, 16, "test mode: skip cam");
#endif

    // 编码器/PID/PIT 只在整车模式启用，避免单模块测试被中断控制覆盖
#if !(TEST_SERVO || TEST_MOTOR || TEST_UART)
    // 编码器初始化
    encoder_quad_init(ENCODER_1, ENCODER_1_A, ENCODER_1_B); // 初始化编码器模块与引脚 正交解码编码器模式
    encoder_quad_init(ENCODER_2, ENCODER_2_A, ENCODER_2_B); // 初始化编码器模块与引脚 正交解码编码器模式

    // PID初始化
    My_Pid_Init();

    // 定时器0初始化，10ms可调
    pit_ms_init(PIT_CH0, 10);
#endif
}

static uint32_t system_start_time = 0;      // 系统启动时间戳
static uint8_t timer_initialized = 0;       // 计时器初始化标志

int main(void)
{
    clock_init(SYSTEM_CLOCK_600M); // 不可删除
    debug_init();                  // 调试端口初始化
    system_delay_ms(300);          // 等待主板其他外设上电完成

    Init();
    interrupt_global_enable(0);

    // 新增：初始化计时器并记录启动时间
    timer_init(GPT_TIM_1, TIMER_MS); // 使用毫秒模式
    timer_start(GPT_TIM_1);
    system_start_time = timer_get(GPT_TIM_1);
    timer_initialized = 1;
    uint32_t start_time = timer_get(GPT_TIM_1);
    uint32_t end_time;
    uint8_t last_state;
    uint8_t now_state;

#if TEST_UART
    // UART 测试：持续打印，观察上位机是否能收到
    uint32_t cnt = 0;
    while (1)
    {
        printf("[TEST_UART] hello %lu\r\n", (unsigned long)cnt++);
        system_delay_ms(500);
    }
#endif

#if TEST_SERVO
    // 舵机测试：中-左-右 循环，便于观察角度和抖动
    while (1)
    {
        // 中位
        pwm_set_duty(SERVO_MOTOR_PWM, SERVO_MOTOR_DUTY( (SERVO_MOTOR_L_MAX + SERVO_MOTOR_R_MAX) / 2 ));
        printf("[TEST_SERVO] middle\r\n");
        system_delay_ms(1000);

        // 左极限
        pwm_set_duty(SERVO_MOTOR_PWM, SERVO_MOTOR_DUTY(SERVO_MOTOR_L_MAX));
        printf("[TEST_SERVO] left max\r\n");
        system_delay_ms(1000);

        // 右极限
        pwm_set_duty(SERVO_MOTOR_PWM, SERVO_MOTOR_DUTY(SERVO_MOTOR_R_MAX));
        printf("[TEST_SERVO] right max\r\n");
        system_delay_ms(1000);
    }
#endif

#if TEST_MOTOR
    // 电机开环测试：执行 3 轮后自动停止
    const uint8_t motor1_reverse_level = (MOTOR1_FORWARD_DIR_LEVEL == GPIO_HIGH) ? GPIO_LOW : GPIO_HIGH;
    const uint8_t motor2_reverse_level = (MOTOR2_FORWARD_DIR_LEVEL == GPIO_HIGH) ? GPIO_LOW : GPIO_HIGH;
    const uint32_t duty_mid = PWM_DUTY_MAX / 2;
    const uint8_t test_rounds = 3;

    for (uint8_t round = 0; round < test_rounds; ++round)
    {
        printf("[TEST_MOTOR] round %d/%d\r\n", (int)(round + 1), (int)test_rounds);

        // 左轮正转
        gpio_set_level(MOTOR1_DIR, MOTOR1_FORWARD_DIR_LEVEL);
        pwm_set_duty(MOTOR1_PWM, duty_mid);
        pwm_set_duty(MOTOR2_PWM, 0);
        printf("[TEST_MOTOR] M1 forward\r\n");
        system_delay_ms(1500);

        // 右轮正转
        pwm_set_duty(MOTOR1_PWM, 0);
        gpio_set_level(MOTOR2_DIR, MOTOR2_FORWARD_DIR_LEVEL);
        pwm_set_duty(MOTOR2_PWM, duty_mid);
        printf("[TEST_MOTOR] M2 forward\r\n");
        system_delay_ms(1500);


        // 双轮正转
        gpio_set_level(MOTOR1_DIR, MOTOR1_FORWARD_DIR_LEVEL);
        gpio_set_level(MOTOR2_DIR, MOTOR2_FORWARD_DIR_LEVEL);
        pwm_set_duty(MOTOR1_PWM, duty_mid);
        pwm_set_duty(MOTOR2_PWM, duty_mid);
        printf("[TEST_MOTOR] M1+M2 forward\r\n");
        system_delay_ms(1500);

        // 双轮反转
        gpio_set_level(MOTOR1_DIR, motor1_reverse_level);
        gpio_set_level(MOTOR2_DIR, motor2_reverse_level);
        pwm_set_duty(MOTOR1_PWM, duty_mid);
        pwm_set_duty(MOTOR2_PWM, duty_mid);
        printf("[TEST_MOTOR] M1+M2 reverse\r\n");
        system_delay_ms(1500);

        // 停止
        pwm_set_duty(MOTOR1_PWM, 0);
        pwm_set_duty(MOTOR2_PWM, 0);
        printf("[TEST_MOTOR] stop\r\n");
        system_delay_ms(1000);
    }

    // 测试结束后保持安全停止
    pwm_set_duty(MOTOR1_PWM, 0);
    pwm_set_duty(MOTOR2_PWM, 0);
    gpio_set_level(MOTOR1_DIR, GPIO_LOW);
    gpio_set_level(MOTOR2_DIR, GPIO_LOW);
    printf("[TEST_MOTOR] finished\r\n");
    while (1) { system_delay_ms(1000); }
#endif

	
    while (1)
    {
        //  mt9v03x摄像头
        if (mt9v03x_finish_flag)
        {
            // 另寻空间将图像保存下来，以免产生因读写冲突带来的未知后果
            memcpy((uint8_t *)image_two_value, (uint8_t *)mt9v03x_image, sizeof(uint8_t) * MT9V03X_H * MT9V03X_W);
            // 获取直方图
            get_hist_gram((uint8_t *)image_two_value, MT9V03X_H, MT9V03X_W, hist_gram);
            unsigned char threshold = get_threshold_otsu(hist_gram);
					
            // 二值化处理
            binaryzation_process((uint8_t *)image_two_value, MT9V03X_H, MT9V03X_W, threshold);
            // 边界线寻找
            //auxiliary_process((uint8_t *)image_two_value, MT9V03X_H, MT9V03X_W, threshold, Left_Line, Mid_Line, Right_Line);
						Longest_White_Column();
																	
						//计时检测
						uint32_t time = timer_get(GPT_TIM_1) - system_start_time;
						
						// 道路元素检测
            // 1. 环岛检测
					
						if (time > 5000 && time < 10000)
						{
							Island_Detect();
						}
						else 
						{
							Island_State = 0;
						}
						//	Island_Detect();
				
            // 2. 十字检测
            //Cross_Detect();
            // 3. 斑马线检测
						if((time < 500 || time > 25000)&&(Island_State==0)&&(Cross_Flag==0))
						{
							zebra_handle();
            }
						
						
						last_state = now_state;
						now_state = Island_State;
						if(now_state!=last_state)
						{
							start_time = timer_get(GPT_TIM_1);
						}
/*					
						if((Island_State==1||Island_State==2)||(Island_State==3))
						{
								end_time = timer_get(GPT_TIM_1) - start_time;
								if(end_time>1500)
								{
									Island_State = 0;
									tft180_show_string(0, 110, "              ");
									Left_Island_Flag=0;
									Right_Island_Flag=0;
								}
						}
*/						
						for (uint8_t i = 0; i < MT9V03X_H; i++)
						{
							Mid_Line[i] = (Left_Line[i] + Right_Line[i]) / 2;
						}
						
						for (uint8_t _i = 0; _i < MT9V03X_H; ++_i)
            {
                // 将边界线也显示出来
                image_two_value[_i][Left_Line[_i]+5] = 0;			
                image_two_value[_i][Mid_Line[_i]] = 0;
                image_two_value[_i][Right_Line[_i]-5] = 0;
            }
						// 显示图像
            tft180_displayimage03x((uint8_t *)image_two_value, 125, 100);
						
            // 控制策略
						float err1;
						float err2;
						float err3;
            float mid_err;
            if (Island_State > 0) {
                // 环岛控制策略
               err1 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/3];
						   err2 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/4];
               err3 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/5];
							 mid_err = (err1 + err2 + err3)/3;
            } 
            else if (Cross_Flag == 1) {
                // 十字路口控制策略
                err1 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/3];
						    err2 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/4];
                err3 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/5];
							  mid_err = (err1 + err2 + err3)/3;
            }
						else if ((zebra.ignore_line_counter > 0 || zebra.state == ZEBRA_FIRST_DETECTED || zebra.state == ZEBRA_SECOND_DETECTED))
            {
                mid_err = 0;
						}
            else {
                // 正常巡线策略
							 err1 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/3];
						   err2 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/4];
               err3 = (MT9V03X_W/2) - Mid_Line[MT9V03X_H-MT9V03X_H/5];
							 mid_err = (err1 + err2 + err3)/3;
							
            }
						
            // 基础舵机打角
            float base_duty = servo_motor_duty_middle - mid_err * 0.5 * SERVO_DIR;
            float tmp_duty = base_duty;
            
            // 限幅
            tmp_duty = MAX(tmp_duty,MIN(SERVO_MOTOR_L_MAX,SERVO_MOTOR_R_MAX));
            tmp_duty = MIN(tmp_duty,MAX(SERVO_MOTOR_L_MAX,SERVO_MOTOR_R_MAX));
						
						// 舵机驱动
						theta = tmp_duty + 5;
            pwm_set_duty(SERVO_MOTOR_PWM, SERVO_MOTOR_DUTY(theta));
						
            // 处理完一帧图像后务必把该标志位清零！
            mt9v03x_finish_flag = 0;
        }
    }
}
