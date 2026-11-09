#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "zf_common_headfile.h"

#define IMG_BLACK     0X00      //0x00ÊÇºÚ
#define IMG_WHITE     0Xff      //0xffÎª°×

void Longest_White_Column(void);
void Cross_Detect(void);
void Straight_Detect(void);
void Show_Boundry(void);
void Find_Up_Point(int start,int end);
void Find_Down_Point(int start,int end);
void Lengthen_Left_Boundry(int start,int end);
void Lengthen_Right_Boundry(int start,int end);
void Left_Add_Line(int x1,int y1,int x2,int y2);
void Right_Add_Line(int x1,int y1,int x2,int y2);
void Draw_Line(int startX, int startY, int endX, int endY);

#endif
