#include "IMG_DEAL.h"
#include "obstacle_path.h"
#include "brick_mask.h"
#include "brick_candidates.h"
#include "exit_lane.h"
#include "lane_centering.h"
#include "lane_geometry.h"
static lane_width_model_t lane_widths;
static lane_geometry_t geometry;
static lane_centering_t normal_center;
static uint8_t obstacle_mask[120][188];
static obstacle_path_t avoidance;
static uint8_t brick_bright[120][188];
#include "Threshold.h"
#include "main.h"
#include "ring_transition.h"
static ring_transition_t ring_transition;
extern Roundabout_position_enmum Roundabout_flag_position;
extern ALL_enum element_flag;

volatile unsigned char c26_drive_enabled = 0;
volatile uint32_t control_elapsed_ms=0;
volatile float servo_requested_us=1550.0f,servo_applied_us=1550.0f;
volatile float speed_target_left=0,speed_target_right=0;
extern int16 encoder_data_1,encoder_data_2;
uint8_t image[MT9V03X_H][MT9V03X_W];

// 舵机动作角度中值
const float servo_motor_duty_middle = (SERVO_MOTOR_R_MAX + SERVO_MOTOR_L_MAX) / 2.f;

// 显示控制参数
#define DISPLAY_INTERVAL 3      // 每3帧显示一次，可根据需要调整（2-5）
#define DISPLAY_WIDTH 160       // TFT显示宽度
#define DISPLAY_HEIGHT 120      // TFT显示高度

void Init()
{
    // 初始化flash, 储存参数
    flash_init();

    // tft180_set_dir(TFT180_CROSSWISE);  // 需要先横屏 不然显示不下
    tft180_init();

    // mt9v03x摄像头初始化
    while (1)
    {
        if (mt9v03x_init())
            tft180_show_string(0, 16, "mt9v03x reinit.");
        else
            break;
        system_delay_ms(1000);
    }
    tft180_show_string(0, 16, "init success.");

    // 编码器初始化
    IOMUXC_GPR->GPR6 &= ~(IOMUXC_GPR_GPR6_QTIMER2_TRM0_INPUT_SEL_MASK|
        IOMUXC_GPR_GPR6_QTIMER2_TRM1_INPUT_SEL_MASK|IOMUXC_GPR_GPR6_QTIMER2_TRM2_INPUT_SEL_MASK|
        IOMUXC_GPR_GPR6_QTIMER2_TRM3_INPUT_SEL_MASK);
    __DSB();
    encoder_quad_init(ENCODER_1, ENCODER_1_A, ENCODER_1_B);
    encoder_quad_init(ENCODER_2, ENCODER_2_A, ENCODER_2_B);

    // 电机初始化
    pwm_init(MOTOR1_PWM, 17000, 0);
    gpio_init(MOTOR1_DIR, GPO, GPIO_HIGH, GPO_PUSH_PULL);
    pwm_init(MOTOR2_PWM, 17000, 0);
    gpio_init(MOTOR2_DIR, GPO, GPIO_HIGH, GPO_PUSH_PULL);

    // 舵机
    pwm_init(SERVO_MOTOR_PWM, SERVO_MOTOR_FREQ, SERVO_MOTOR_DUTY(servo_motor_duty_middle));

    // PID初始化
    My_Pid_Init();

    // 定时器0初始化，10ms可调
    pit_ms_init(PIT_CH0, 10);
}

/**
 * @brief 使用 IMG_DEAL 生成的 middleline[] 计算整体图像偏差
 */
static float ImgDeal_GetMidError(void)
{
    // middleline / mid_row / weight 都在 IMG_DEAL.c 里定义
    int i;
    long err_sum = 0;
    long w_sum   = 0;

    // 没有有效中线，直接认为误差为 0
    if (middleline[0] == 254)
    {
        return 0.0f;
    }

    for (i = 119; i >= 0; i--)
    {
        if (middleline[i] == 254 || mid_row[i] == 254)
        {
            continue;
        }

        int Mid_row = 119 - mid_row[i];
        if (Mid_row < 0 || Mid_row >= 120)
        {
            continue;
        }

        int delta = (COL / 2) - middleline[i];   // 中心 - 中线

        err_sum += (long)weight[Mid_row] * delta;
        w_sum   += (long)weight[Mid_row];
    }

    if (w_sum == 0)
    {
        return 0.0f;
    }

    return (float)err_sum / (float)w_sum;
}

// 进入/处在/刚离开环岛这几个阶段，一律按 40 跑
static inline uint8_t IsRoundaboutSlowState(void)
{
    return  (Roundabout_flag == ROUND_READY)   ||
            (Roundabout_flag == GET_IN_READY) ||
            (Roundabout_flag == GET_IN_ROUND) ||
            (Roundabout_flag == IN_ROUND)     ||
            (Roundabout_flag == GET_OUT_ROUND)||
            (Roundabout_flag == OUTTING)      ||
            (Roundabout_flag == OUT_WAY);
    // 真正完全跑出去了，一般会变成 OUT_ROUND 或 NO_ROUND，
    // 这时就恢复 45
}


int main(void)
{
    clock_init(SYSTEM_CLOCK_600M); // 不可删除
    debug_init();                  // 调试端口初始化
    system_delay_ms(300);          // 等待主板其他外设上电完成

    gpio_init(C26, GPI, GPIO_HIGH, GPI_PULL_UP);
    uart_init(UART_8,115200,UART8_TX_D16,UART8_RX_D17);
    Init();
    interrupt_global_enable(0);
    
    // 显示控制变量
    static int frame_counter = 0;     // 帧计数器
    static int display_counter = 0;   // 显示帧计数器
    
    // 性能调试变量（可选）
    #ifdef DEBUG_PERFORMANCE
    uint32_t start_time, end_time;
    float max_process_time = 0;
    #endif

    while (1)
    {
        if (mt9v03x_finish_flag)
        {
            mt9v03x_finish_flag = 0;
            frame_counter++;  // 总帧数计数
            
            #ifdef DEBUG_PERFORMANCE
            start_time = get_system_tick();  // 获取开始时间
            #endif

            /******** 1. 把摄像头原始图像拷贝到 IMG_DATA，给 IMG_DEAL 用 ********/
            // 方法1：逐行拷贝（较慢但稳定）
            for (int r = 0; r < ROW; r++)
            {
                memcpy(IMG_DATA[r], mt9v03x_image[r], COL);
            }
            
            // 方法2：内存块拷贝（较快，但需要确认内存布局）
            // memcpy((uint8_t*)IMG_DATA, (uint8_t*)mt9v03x_image, ROW*COL);

            /******** 2. 调用 IMG_DEAL，完成二值化 + 赛道/环岛识别 + 生成 middleline[] ********/
            int ring_before=(int)Roundabout_flag;
            int mask_ok=0;
            mask_ok=brick_mask(IMG_DATA,obstacle_mask);
            int bright_ok=brick_bright_mask(IMG_DATA,brick_bright);
            Img_Deal();

            /******** 3. 计算中线误差 ********/
            float mid_err = ImgDeal_GetMidError();
            memset(&avoidance,0,sizeof(avoidance));
            unsigned brick_variant=0;
            if(brick_stage_allowed(ring_before,(int)Roundabout_flag))
                brick_paths(obstacle_mask,mask_ok,brick_bright,bright_ok,&avoidance,&brick_variant);
            else obstacle_path_reset();
            if(brick_rejects_ring_candidate(ring_before,(int)Roundabout_flag,avoidance.active)){
                Roundabout_flag=NO_ROUND;Roundabout_flag_position=NOROUND;element_flag=NO_JUGED;
            }
            float legacy_error=mid_err;unsigned centered=0;
            normal_center.paired=normal_center.near_rows=0;
            int paired_ok=mask_ok&&!avoidance.active&&left_flag&&right_flag&&
                paired_lane_center(left.Row,left.Col,right.Row,right.Col,obstacle_mask,weight,&normal_center);
            if(paired_ok&&lane_center_may_override(ring_before,(int)Roundabout_flag,(int)NO_ROUND,
                    avoidance.active,left_flag,right_flag)){
                mid_err=normal_center.error;centered=1;
            }
            memset(&geometry,0,sizeof(geometry));
            if(mask_ok&&!avoidance.active&&(left_flag||right_flag)){
                int learning=ring_before==(int)NO_ROUND&&Roundabout_flag==NO_ROUND;
                lane_geometry(&lane_widths,left.Row,left.Col,right.Row,right.Col,obstacle_mask,weight,
                    mid_row,middleline,learning,&geometry);
                if(!centered&&learning&&geometry.valid){mid_err=geometry.error;centered=2;}
            }
            unsigned mask_exit=0;
            if(mask_ok&&!avoidance.active&&!geometry.valid&&
                ((Roundabout_flag>=GET_OUT_ROUND&&Roundabout_flag<=OUT_ROUND)||
                 (Roundabout_flag==NO_ROUND&&(ring_before==OUT_WAY||ring_before==OUT_ROUND||ring_transition.recover_frames)))){
                mask_exit=exit_lane_from_mask(obstacle_mask,&lane_widths,weight,&geometry);
            }
            ring_target_t selected=ring_transition_target(&ring_transition,ring_before,(int)Roundabout_flag,
                (int)Roundabout_flag_position,avoidance.active,paired_ok,normal_center.error,
                left_flag||mask_exit,right_flag||mask_exit,&geometry,mid_err);
            if(selected.source){mid_err=selected.error;centered=selected.source;}
            if(avoidance.active&&!avoidance.blocked)mid_err=avoidance.error;

            // tft180_show_float(0, 0, mid_err, 5, 2);  // 移到显示部分
servo_requested_us=steer_target_us(mid_err);
            /* Keep entry steering exactly. Only a selected same-corridor
             * FOLLOW/EXIT path can receive the small post-map correction. */
            if(Roundabout_flag>=IN_ROUND&&Roundabout_flag<=OUT_ROUND&&!avoidance.active&&!selected.source){
                float near_sum=0;int near_count=0;
                for(int i=0;i<120;i++)if(mid_row[i]>=90&&mid_row[i]<=110&&middleline[i]!=254){
                    near_sum+=94.0f-middleline[i];near_count++;
                }
                if(near_count>=8)servo_requested_us=ring_center_servo(servo_requested_us,
                    geometry.near_error,near_sum/near_count,geometry.valid,geometry.same_corridor);
            }
            float tmp_duty=(servo_requested_us-500.0f)*.09f;


            /******** 5. 斑马线停车控制逻辑 ********/
              switch (zebra_state) {
                case ZEBRA_DETECTED:
                    // 检测到斑马线，立即停车
                    speed_flag = 0;
                    zebra_stop_delay++;

                    // 停车指定时间后进入永久停止状态（第二次斑马线）
                    if (zebra_stop_delay > STOP_DELAY) {
                        zebra_state = ZEBRA_STOPPED;
                        zebra_stop_delay = 0;
                    }
                    break;

                case ZEBRA_STOPPED:
                    // 保持停车状态，不再恢复速度
                    speed_flag = 0;
                    break;

                default:  // ZEBRA_NONE
                    // 未触发斑马线停车时：根据环岛状态分段速度
                    if (IsRoundaboutSlowState()) {
                        // 检测到/进入/正在绕环岛/刚出来 —— 低速 40
                        speed_flag = 42;
                    } else {
                        // 其它情况全部 45
                        speed_flag = 43;
                    }
                    break;
            }

            // 把最终的速度档位喂给 PID
            speed_target = gpio_get_level(C26)||speed_flag==0?0:
                (avoidance.active?(avoidance.blocked?0:.55f):TARGET_SPEED_MPS);
            if(!avoidance.active)speed_target=route_cruise_speed(speed_target,(int)Roundabout_flag,servo_requested_us);
            float raw_speed_target=avoidance.active?speed_target:ring_transition_raw_speed(speed_target,&geometry,(int)Roundabout_flag);
            if(!avoidance.active)speed_target=ring_transition_speed(&ring_transition,speed_target,&geometry,
                (int)Roundabout_flag,control_elapsed_ms);
            else {ring_transition.speed=speed_target;ring_transition.speed_ms=control_elapsed_ms;
                ring_transition.speed_initialized=speed_target>0;}
            speed_target_left=speed_target*LEFT_COUNTS_PER_METER*CONTROL_PERIOD_S;
            speed_target_right=speed_target*RIGHT_COUNTS_PER_METER*CONTROL_PERIOD_S;
            if(frame_counter%5==0){char telemetry[460];
                snprintf(telemetry,sizeof(telemetry),"[NEWNEW_C26_V10] n=%d ms=%lu on=%u ring=%d zebra=%d err=%.2f set_mps=%.2f target=%.2f/%.2f measured_mps=%.3f/%.3f req_us=%.0f servo_us=%.0f obs=%u blocked=%u side=%d centered=%u paired=%u legacy=%.2f model=%u near=%.2f margin=%.3f exitok=%u raw_mps=%.2f geo=%u rside=%d brickmask=%u maskexit=%u recover=%u\r\n",
                    frame_counter,(unsigned long)control_elapsed_ms,(unsigned)(gpio_get_level(C26)==0),(int)Roundabout_flag,(int)zebra_state,
                    (double)mid_err,(double)speed_target,(double)speed_target_left,(double)speed_target_right,
                    (double)(encoder_data_1/(LEFT_COUNTS_PER_METER*CONTROL_PERIOD_S)),
                    (double)(encoder_data_2/(RIGHT_COUNTS_PER_METER*CONTROL_PERIOD_S)),(double)servo_requested_us,(double)servo_applied_us,avoidance.active,avoidance.blocked,avoidance.side,centered,normal_center.paired,(double)legacy_error,(unsigned)geometry.model_used,
                    (double)geometry.near_error,(double)geometry.clearance,(unsigned)ring_transition.exit_current,(double)raw_speed_target,(unsigned)geometry.valid,(int)Roundabout_flag_position,brick_variant,mask_exit,ring_transition.recover_frames);
                uart_write_string(UART_8,telemetry);
            }
            /******** 6. 降低TFT显示频率（每N帧显示一次） ********/
            display_counter++;
            if (display_counter >= DISPLAY_INTERVAL)
            {
                display_counter = 0;  // 重置计数器
                
                // 显示二值化图像（可缩放以加快速度）
                // 参数说明：(图像数据, 缩放后宽度, 缩放后高度)
                // 如果原图是188x120，显示160x100可以加快显示速度
                tft180_displayimage03x((uint8_t *)IMG_DATA, 125, 100);
                
                // 显示调试信息（每N帧更新一次）
                // 显示误差值
                tft180_show_float(0, 0, mid_err, 5, 2);
                
                // 显示舵机占空比
                tft180_show_float(0, 20, tmp_duty, 5, 2);
                
                // 显示帧率信息（可选）
                // tft180_show_int(0, 40, frame_counter, 5);
                
                // 显示斑马线状态（可选）
                // tft180_show_int(0, 60, zebra_state, 1);
            }
            
            #ifdef DEBUG_PERFORMANCE
            // 性能调试：计算处理时间
            end_time = get_system_tick();
            float process_time = (end_time - start_time);  // 毫秒
            
            if (process_time > max_process_time) {
                max_process_time = process_time;
            }
            
            // 每100帧打印一次性能信息
            if (frame_counter % 100 == 0) {
                printf("Frame: %d, Process time: %.1fms, Max: %.1fms\r\n", 
                       frame_counter, process_time, max_process_time);
            }
            #endif
            
            // 简单的性能保护：如果帧处理时间过长，可以跳过一些非关键操作
            if (frame_counter % 1000 == 0) {
                // 每1000帧重置计数器防止溢出
                frame_counter = 0;
            }
        }
        
        // 可以在这里添加一些低优先级的后台任务
        // 例如：按键检测、参数更新等
    }
    return 0;
}