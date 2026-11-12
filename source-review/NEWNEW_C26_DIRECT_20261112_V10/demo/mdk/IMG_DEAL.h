#ifndef __IMG_DEAL_H__
#define __IMG_DEAL_H__ 

#include "main.h"
//// 斑马线斑马线检测相关变量
//#define ZEBRA_NONE 0         // 未检测到
//#define ZEBRA_DETECTED 1     // 检测到斑马线
//#define ZEBRA_STOPPED 2      // 已停车
//// 声明全局变量（供外部文件访问）
//extern int zebra_state;       // 斑马线状态（定义在 IMG_DEAL.c 中）
//extern int zebra_stop_delay;  // 停车延时计数器（定义在 IMG_DEAL.c 中）




typedef enum
{
    NO_JUGED,
    IN_JUGED,
}ALL_enum;//所有元素标志位

typedef enum
{
    NO_FORK,
    IN_FORK,
}FORK_enum;//三叉

typedef enum
{
    NO_T,
    IN_T,
    T_READY,
}T_CONNER_enum;//T弯

typedef enum
{
    GET_OUT,
    FIRST,
    READY,
    GET_IN,
    NO_GARAGE,
}Garage_enmum;//车库

typedef enum
{
    ROUND_READY,
    GET_IN_READY,
    GET_IN_ROUND,
    IN_ROUND,
    GET_OUT_ROUND,
    OUTTING,
    OUT_WAY,
    OUT_ROUND,
    NO_ROUND,
}Roundabout_enmum;//环岛

typedef enum
{
    ROUND_L,
    ROUND_R,
    NOROUND,
}Roundabout_position_enmum;//环岛

typedef enum
{
    NO_RAMP,
    RAMP_READY,
    IN_RAMP,
}Ramp_enmum;//环岛

//----------------函数定义区--------------
void Img_Deal(void);
void Img_Deal1(void);
void Deal_Init(void);
void Data_collect_display(void);
//void Img_Send_To_Top( uint8 Image[ROW][COL]); 
void LeftStartFind(void);
void RightStartFind(void);
void Boost(void);
float Cross_col(void);

void Zebra_Detect(void);  //斑马线停车

extern int v;
   
extern int dj_end;
 
extern int right_flag;
extern int left_flag;
extern int right_start;
extern int left_start;
extern int middle_start;
extern int middle_end;

extern int right_line[120];
extern int left_line[120];
extern int middleline[240];
extern int mid_row[240];
#define ROW   (MT9V03X_H)
#define COL   (MT9V03X_W)
extern int max;
extern int min;
extern int line[120];
extern int weight[120];
extern int wild1;
extern int wild10;
extern float ang,ang_l,ang_r;
extern int L_lenth,R_lenth;
extern float L_S,R_S;
extern float CorssCol;

extern unsigned char stop;
extern unsigned char deal_flag;
extern unsigned char IMG_DATA[120][188];
extern unsigned char fork_flag;
extern unsigned char fork_turn;
extern unsigned char T_go_flag[2];
extern uint8 distance_flag,fork_times;
extern int speed_flag;
 extern Roundabout_enmum Roundabout_flag;

// 新增：斑马线检测相关定义
#define WHITE_IMG 255
#define BLACK_IMG 0
#define ZEBRA_THRESHOLD 20  // 单行连续黑色像素阈值
#define ZEBRA_ROW_COUNT 28     // 判定为斑马线的最少行数
#define STOP_DELAY 700      // 停车延迟计数（10ms周期对应2秒）

typedef enum {
    ZEBRA_NONE,        // 未检测到斑马线
    ZEBRA_DETECTED,    // 检测到斑马线，正在停车
    ZEBRA_STOPPED      // 已停车，不再恢复
} ZebraState;

// 新增：全局变量声明
extern ZebraState zebra_state;
extern int zebra_stop_delay;
extern int speed_flag;



#endif 
