#include "zf_common_headfile.h"
#include "Island.h"
#include "main.h"

//环岛变量
uint8_t Island_State=0;     //环岛状态标志
uint8_t Left_Island_Flag=0; //左右环岛标志
uint8_t Right_Island_Flag=0;//左右环岛标志
uint8_t Left_Up_Guai[2];    //四个拐点的坐标存储，[0]存y，第某行，{1}存x，第某列
uint8_t Right_Up_Guai[2];   //四个拐点的坐标存储，[0]存y，第某行，{1}存x，第某列

/*-------------------------------------------------------------------------------------------------------------------
  @brief     环岛检测
  @param     null
  @return    null
  Sample     Island_Detect(void);
  @note      利用四个拐点判别函数，单调性改变函，连续性数撕裂点，分为8步
-------------------------------------------------------------------------------------------------------------------*/
void Island_Detect()
{ 
    static float k=0;//3和5状态的k
    static int island_state_5_down[2]={0};//状态5时即将离开环岛，左右边界边最低点，[0]存y，第某行，{1}存x，第某列
    static int island_state_3_up[2]={0};//状态3时即将进入环岛用，左右上面角点[0]存y，第某行，{1}存x，第某列
    static int left_down_guai[2]={0};//四个拐点的坐标存储，[0]存y，第某行，{1}存x，第某列
    static int right_down_guai[2]={0};//四个拐点的坐标存储，[0]存y，第某行，{1}存x，第某列
    int monotonicity_change_line[2];//单调性改变点坐标，[0]寸某行，[1]寸某列
    int monotonicity_change_left_flag=0;//不转折是0
    int monotonicity_change_right_flag=0;//不转折是0
    int continuity_change_right_flag=0; //连续是0
    int continuity_change_left_flag=0;  //连续是0
    //以下是常规判断法
    continuity_change_left_flag=Continuity_Change_Left(MT9V03X_H-1-5,10);//连续性判断
    continuity_change_right_flag=Continuity_Change_Right(MT9V03X_H-1-5,10);
    monotonicity_change_right_flag=Monotonicity_Change_Right(MT9V03X_H-1-10,10);
    monotonicity_change_left_flag=Monotonicity_Change_Left(MT9V03X_H-1-10,10);
    if((Cross_Flag==0)&&(Island_State==0)&&(Zebra_Flag==0))
    {
        continuity_change_left_flag=Continuity_Change_Left(MT9V03X_H-1-5,10);//连续性判断
        continuity_change_right_flag=Continuity_Change_Right(MT9V03X_H-1-5,10);
        if(Left_Island_Flag==0)//左环
        {
            if(1
							 //&&monotonicity_change_right_flag==0 //右边是单调的
               && continuity_change_left_flag!=0 //左边是不连续的
               //&& continuity_change_right_flag==0 //左环岛右边是连续的
               && Left_Lost_Time>=10 //左边丢线很多
               && Left_Lost_Time<=50 //也不能全丢了
               && Right_Lost_Time<=10//右边丢线较少
               && Search_Stop_Line>=MT9V03X_H*0.8 //搜索截止行看到很远
               && Boundry_Start_Left>=MT9V03X_H-10&&Boundry_Start_Right>=MT9V03X_H-5 //边界起始点靠下
               && Both_Lost_Time<=10
							)//双边丢线少
            {
                left_down_guai[0]=Find_Left_Down_Point(MT9V03X_H-1,20);//找左下角点
								
                if(left_down_guai[0]>=30)//条件1很松，在这里判断拐点，位置不对，则是误判，跳出
                {
                    Island_State=0;
                    Left_Island_Flag=0;
                }
                else//误判，归零
                {
									  tft180_show_string(0, 110, "              ");
                    Island_State=0;
                    Left_Island_Flag=0;
                }
            }
        }
        if(Right_Island_Flag==0)//右环
        {
            if(1
							 //&& monotonicity_change_left_flag==0
               //&& continuity_change_left_flag==0 //右环岛左边是连续的
               && continuity_change_right_flag!=0 //右边是不连续的
               && Right_Lost_Time>=10           //右丢线多
               && Right_Lost_Time<=50           //右丢线不能太多
               && Left_Lost_Time<=10            //左丢线少
               && Search_Stop_Line>=MT9V03X_H*0.8 //搜索截止行看到很远
               && Boundry_Start_Left>=MT9V03X_H-5&&Boundry_Start_Right>=MT9V03X_H-10 //边界起始点靠下
               && Both_Lost_Time<=10
							)
            {
                right_down_guai[0]=Find_Right_Down_Point(MT9V03X_H-1,20);//右下点
							
                if(right_down_guai[0]>=30)//条件1很松，在这里加判拐点，位置不对，则是误判，跳出
                {
                    Island_State=1;
                    Right_Island_Flag=1;
                }
                else
                {
							     	tft180_show_string(0, 110, "              ");
                    Island_State=0;
                    Right_Island_Flag=0;
                }
            }
        }
    }

    if(Left_Island_Flag==1)//1状态下拐点还在，没丢线
    {
				tft180_show_string(0, 110, "L");
        if(Island_State==1)
        {
						tft180_show_string(10, 110, "1");
            monotonicity_change_line[0]=Monotonicity_Change_Left(30,5);//寻找单调性改变点
            monotonicity_change_line[1]=Left_Line[monotonicity_change_line[0]];
            Left_Add_Line((int)(monotonicity_change_line[1]*0.15),MT9V03X_H-1,monotonicity_change_line[1],monotonicity_change_line[0]);
            if((Island_State==1)&&(Boundry_Start_Left<100))//下方当丢线时候进2
            {
                Island_State=2;
            }
        }


        else if(Island_State==2)//下方角点消失，2状态时下方应该是丢线，上面是弧线
        {
						tft180_show_string(20, 110, "2");
            monotonicity_change_line[0]=Monotonicity_Change_Left(110,20);//寻找单调性改变点
            monotonicity_change_line[1]=Left_Line[monotonicity_change_line[0]];
            Left_Add_Line((int)(monotonicity_change_line[1]*0.1),MT9V03X_H-1,monotonicity_change_line[1],monotonicity_change_line[0]);
            if((Island_State==2)&&(Boundry_Start_Left>=MT9V03X_H-5)&&(monotonicity_change_line[0]>50))//当圆弧靠下时候，进3
            {
                Island_State=3;//最长白列寻找范围也要改，见camera.c
                Left_Island_Flag=1;
            }
        }
        else if(Island_State==3)//3状态准备进环，寻找上拐点，连线
        {
						tft180_show_string(30, 110, "3");
            if(k!=0)
            {
                K_Draw_Line(k,MT9V03X_W,MT9V03X_H-10,0);//k是刚刚算出来的，静态变量存着
                Longest_White_Column();//刷新边界数据
            }
            else
            {
                Left_Up_Guai[0]=Find_Left_Up_Point(90,0);//找左上拐点
                Left_Up_Guai[1]=Left_Line[Left_Up_Guai[0]];

                if(k==0&&(15<=Left_Up_Guai[0]&&Left_Up_Guai[0]<50)&&(50<Left_Up_Guai[1]&&Left_Up_Guai[1]<110))//拐点出现在一定范围内，认为是拐点出现
                {
                    island_state_3_up[0]= Left_Up_Guai[0];
                    island_state_3_up[1]= Left_Up_Guai[1];
                    k=(float)((float)(MT9V03X_H-10-island_state_3_up[0])/(float)(MT9V03X_W-island_state_3_up[1]));
                    K_Draw_Line(k,MT9V03X_W,MT9V03X_H-10,0);//记录下第一次上点出现时位置，针对这个环岛拉一条死线，入环
                    Longest_White_Column();//刷新边界数据
                }
            }
						Right_Up_Guai[0]=Find_Right_Up_Point(110,50);
						Right_Up_Guai[1]=Right_Line[Right_Up_Guai[0]];
            if((Island_State==3)&&(Right_Up_Guai[0] >= 80))
            {
                k=0;//斜率清零
                Island_State=4;//这一步时候顺便调整了最长白列的搜索范围
                Longest_White_Column();//最长白列
            }
        }
        else if(Island_State==4)//状态4已经在里面
        {
						tft180_show_string(40, 110, "4");
						
						if(1)
						{
                //monotonicity_change_line[0]=Monotonicity_Change_Right(MT9V03X_H-10,10);//单调性改变
								monotonicity_change_line[0]=Find_Right_Down_Point(MT9V03X_H-1,20);
                monotonicity_change_line[1]=Right_Line[monotonicity_change_line[0]];					
                if((Island_State==4)&&(monotonicity_change_line[0]>=60)&&(monotonicity_change_line[0]<=110)&&(monotonicity_change_line[1]>=MT9V03X_W/3))//单调点靠下，进去5
                {
                    island_state_5_down[0]=MT9V03X_H-1;
                    island_state_5_down[1]=Right_Line[MT9V03X_H-1];
                    k=(float)((float)(MT9V03X_H-monotonicity_change_line[0])/(float)(MT9V03X_W-monotonicity_change_line[1]));
                    K_Add_Boundry_Right(k,MT9V03X_W,island_state_5_down[0],0);//和状态3一样，记住斜率
                    Island_State=5;
                }
            }
        }
        else if(Island_State==5)//出环
        {
						tft180_show_string(50, 110, "5");
            K_Add_Boundry_Right(k,MT9V03X_W,island_state_5_down[0],0);
            if((Island_State==5)&&(Boundry_Start_Right<MT9V03X_H-10))//右边先丢线
            {
                Island_State=6;
            }
        }
        else if(Island_State==6)//还在出
        {
						tft180_show_string(60, 110, "6");
            K_Add_Boundry_Right(k,MT9V03X_W,island_state_5_down[0],0);
            if((Island_State==6)&&((Boundry_Start_Right>MT9V03X_H-10)))//右边不丢线
            {//
                k=0;
                Island_State=7;
            }
        }
        else if(Island_State==7)//基本出去了，在寻找拐点，准备离开环岛状态
        {
						tft180_show_string(70, 110, "7");
            Left_Up_Guai[0]=Find_Left_Up_Point(MT9V03X_H-5,0);//获取左上点坐标，坐标点合理去8
            Left_Up_Guai[1]=Left_Line[Left_Up_Guai[0]];
            if((Island_State==7)&&(Left_Up_Guai[1]<=MT9V03X_W*3/4)&&(Left_Up_Guai[0]>=5)&&(Left_Up_Guai[0]<=MT9V03X_H-10))//注意这里，对横纵坐标都有要求
            {
                Island_State=8;//基本上找到拐点就去8
            }
        }
        else if(Island_State==8)//连线，出环最后一步
        {
						tft180_show_string(80, 110, "8");
            Left_Up_Guai[0]=Find_Left_Up_Point(MT9V03X_H-1,10);//获取左上点坐标
            Left_Up_Guai[1]=Left_Line[Left_Up_Guai[0]];
            Lengthen_Left_Boundry(Left_Up_Guai[0]-1,MT9V03X_H-1);
            if((Island_State==8)&&(Left_Up_Guai[0]>=MT9V03X_H-20||(Left_Up_Guai[0]<10&&Boundry_Start_Left>=MT9V03X_H-10)))//当拐点靠下时候，认为出环了，环岛结束
            {//要么拐点靠下，要么拐点丢了，切下方不丢线，认为环岛结束了
                Island_State=0;
                Right_Island_Flag=0;
								tft180_show_string(0, 110, "              ");
            }
        }
    }
    else if(Right_Island_Flag==1)
    {
				tft180_show_string(0, 110, "R");
        if(Island_State==1)//1状态下拐点还在，没丢线
        {
						tft180_show_string(10, 110, "1");
            monotonicity_change_line[0]=Monotonicity_Change_Right(30,5);//单调性改变
            monotonicity_change_line[1]=Right_Line[monotonicity_change_line[0]];
            Right_Add_Line((int)(MT9V03X_W-(MT9V03X_W-(monotonicity_change_line[1]))*0.1),MT9V03X_H-1,monotonicity_change_line[1],monotonicity_change_line[0]);
            if((Island_State==1)&&(Boundry_Start_Right<=100))//右下角先丢线
            {
                Island_State=2;
            }
        }
        else if(Island_State==2)//2状态下方丢线，上方即将出现大弧线
        {
						tft180_show_string(20, 110, "2");
            monotonicity_change_line[0]=Monotonicity_Change_Right(110,20);//单调性改变
            monotonicity_change_line[1]=Right_Line[monotonicity_change_line[0]];
            Right_Add_Line((int)(MT9V03X_W-(MT9V03X_W-(monotonicity_change_line[1]))*0.1),MT9V03X_H-1,monotonicity_change_line[1],monotonicity_change_line[0]);
            if(Island_State==2&&(Boundry_Start_Right>=MT9V03X_H-5)&&(monotonicity_change_line[0]>50))//右下角再不丢线进3
            {
                Island_State=3;//下方丢线，说明大弧线已经下来了
                Right_Island_Flag=1;
            }
        }
        else if(Island_State==3)//下面已经出现大弧线，且上方出现角点
        {
						tft180_show_string(30, 110, "3");
            if(k!=0)//将角点与下方连接，画一条死线
            {
                K_Draw_Line(k,30,MT9V03X_H-10,0);
                Longest_White_Column();//刷新最长白列
            }
            else
            {
                Right_Up_Guai[0]=Find_Right_Up_Point(90,0);//找右上拐点
                Right_Up_Guai[1]=Right_Line[Right_Up_Guai[0]];
/*
                if(Right_Up_Guai[0]<10)//这里改过，此处为了防止环岛误判，如果经常出现环岛3归零，请修改此处判断条件
                {
                    Island_State=0;
                    Right_Island_Flag=0;
                }
*/
                if(k==0&&(15<=Right_Up_Guai[0]&&Right_Up_Guai[0]<50)&&(70<Right_Up_Guai[1]&&Right_Up_Guai[1]<150))//找第一个符合条件的角点，连线
                {
                    island_state_3_up[0]= Right_Up_Guai[0];
                    island_state_3_up[1]= Right_Up_Guai[1];
                    k=(float)((float)(MT9V03X_H-10-island_state_3_up[0])/(float)(30-island_state_3_up[1]));
                    K_Draw_Line(k,30,MT9V03X_H-10,0);
                    Longest_White_Column();//刷新赛道数据
                }
            }
						Left_Up_Guai[0]=Find_Left_Up_Point(110,50);
						Left_Up_Guai[1]=Left_Line[Left_Up_Guai[0]];
            if((Island_State==3)&&(Left_Up_Guai[0] >= 80))
            {
                k=0;//斜率清零
                Island_State=4;
                Longest_White_Column();//最长白列
            }
        }
        else if(Island_State==4)//4状态完全进去环岛了
        {
						tft180_show_string(40, 110, "4");
            if(1)
            {
                monotonicity_change_line[0]=Find_Left_Down_Point(MT9V03X_H-1,20);//单调性改变
                monotonicity_change_line[1]=Left_Line[monotonicity_change_line[0]];
                if((Island_State==4)&&(monotonicity_change_line[0]>=60)&&(monotonicity_change_line[0]<=110)&&(monotonicity_change_line[1]<=MT9V03X_W*2/3))//单调点靠下，进去5
                {
                   island_state_5_down[0]=MT9V03X_H-1;
                   island_state_5_down[1]=Left_Line[MT9V03X_H-1];
                   k=(float)((float)(MT9V03X_H-monotonicity_change_line[0])/(float)(0-monotonicity_change_line[1]));
                   K_Add_Boundry_Left(k,0,island_state_5_down[0],0);
                   Island_State=5;
                }
            }
        }
        else if(Island_State==5)//准备出环岛
        {
						tft180_show_string(50, 110, "5");
            K_Add_Boundry_Left(k,0,island_state_5_down[0],0);
            if(Island_State==5&&Boundry_Start_Left<MT9V03X_H-10)//左边先丢线
            {
                Island_State=6;
            }
        }
        else if(Island_State==6)//继续出
        {
						tft180_show_string(60, 110, "6");
            K_Add_Boundry_Left(k,0,island_state_5_down[0],0);
            if((Island_State==6)&&(Boundry_Start_Left>MT9V03X_H-10))//左边先丢线
            {//
                k=0;
                Island_State=7;
            }
        }
        else if(Island_State==7)//基本出环岛，找角点
        {
						tft180_show_string(70, 110, "7");
            Right_Up_Guai[0]=Find_Right_Up_Point(MT9V03X_H-5,0);//获取左上点坐标，找到了去8
            Right_Up_Guai[1]=Right_Line[Right_Up_Guai[0]];
            if((Island_State==7)&&((Right_Up_Guai[1]>=MT9V03X_W/4)&&(Right_Up_Guai[0]>=5)&&(Right_Up_Guai[0]<=MT9V03X_H-10)))//注意这里，对横纵坐标都有要求，因为赛道不一样，会意外出现拐点
            {//当角点位置合理时，进8
                Island_State=8;
            }
        }
        else if(Island_State==8)//环岛8
        {
						tft180_show_string(80, 110, "8");
            Right_Up_Guai[0]=Find_Right_Up_Point(MT9V03X_H-1,10);//获取右上点坐标
            Right_Up_Guai[1]=Right_Line[Right_Up_Guai[0]];
            Lengthen_Right_Boundry(Right_Up_Guai[0]-1,MT9V03X_H-1);
            if((Island_State==8)&&(Right_Up_Guai[0]>=MT9V03X_H-20||(Right_Up_Guai[0]<10&&Boundry_Start_Left>=MT9V03X_H-10)))//当拐点靠下时候，认为出环了，环岛结束
            {//角点靠下，或者下端不丢线，认为出环了
                Island_State=0;
                Right_Island_Flag=0;
								tft180_show_string(0, 110, "              ");
            }
        }
    }
}


/*-------------------------------------------------------------------------------------------------------------------
  @brief     左赛道连续性检测
  @param     起始点，终止点
  @return    连续返回0，不连续返回断线出行数
  Sample     Continuity_Change_Left(int start,int end);
  @note      连续性的阈值设置为5，可更改
-------------------------------------------------------------------------------------------------------------------*/
int Continuity_Change_Left(int start,int end)//连续性阈值设置为5
{
    int i;
    int t;
    int continuity_change_flag=0;
    if(Left_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没必要判断了
       return 1;
    if(Search_Stop_Line<=5)//搜所截止行很矮
       return 1;
    if(start>=MT9V03X_H-1-5)//数组越界保护
        start=MT9V03X_H-1-5;
    if(end<=5)
       end=5;
    if(start<end)//都是从下往上计算的，反了就互换一下
    {
       t=start;
       start=end;
       end=t;
    }
    
    for(i=start;i>=end;i--)
    {
       if(abs(Left_Line[i]-Left_Line[i-1])>=5)//连续判断阈值是5,可更改
       {
            continuity_change_flag=i;
            break;
       }
    }
    return continuity_change_flag;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     右赛道连续性检测
  @param     起始点，终止点
  @return    连续返回0，不连续返回断线出行数
  Sample     continuity_change_flag=Continuity_Change_Right(int start,int end)
  @note      连续性的阈值设置为5，可更改
-------------------------------------------------------------------------------------------------------------------*/
int Continuity_Change_Right(int start,int end)
{
    int i;
    int t;
    int continuity_change_flag=0;
    if(Right_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没必要判断了
       return 1;
    if(start>=MT9V03X_H-5)//数组越界保护
        start=MT9V03X_H-5;
    if(end<=5)
       end=5;
    if(start<end)//都是从下往上计算的，反了就互换一下
    {
       t=start;
       start=end;
       end=t;
    }

    for(i=start;i>=end;i--)
    {
        if(abs(Right_Line[i]-Right_Line[i-1])>=5)//连续性阈值是5，可更改
       {
            continuity_change_flag=1;
            break;
       }
    }
    return continuity_change_flag;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     左下角点检测
  @param     起始点，终止点
  @return    返回角点所在的行数，找不到返回0
  Sample     Find_Left_Down_Point(int start,int end);
  @note      角点检测阈值可根据实际值更改
-------------------------------------------------------------------------------------------------------------------*/
int Find_Left_Down_Point(int start,int end)//找四个角点，返回值是角点所在的行数
{
    int i,t;
    int left_down_line=0;
    if(Left_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没有拐点判断的意义
       return left_down_line;
    if(start<end)
    {
        t=start;
        start=end;
        end=t;
    }
    if(start>=MT9V03X_H-1-5)//下面5行数据不稳定，不能作为边界点来判断，舍弃
        start=MT9V03X_H-1-5;
    if(end<=MT9V03X_H-Search_Stop_Line)
        end=MT9V03X_H-Search_Stop_Line;
    if(end<=5)
       end=5;
    for(i=start;i>=end;i--)
    {
        if(left_down_line==0&&//只找第一个符合条件的点
           abs(Left_Line[i]-Left_Line[i+1])<=5&&//角点的阈值可以更改
           abs(Left_Line[i+1]-Left_Line[i+2])<=5&&  
           abs(Left_Line[i+2]-Left_Line[i+3])<=5&&
              (Left_Line[i]-Left_Line[i-2])>=5&&
              (Left_Line[i]-Left_Line[i-3])>=10&&
              (Left_Line[i]-Left_Line[i-4])>=10)
        {
            left_down_line=i;//获取行数即可
            break;
        }
    }
    return left_down_line;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     左上角点检测
  @param     起始点，终止点
  @return    返回角点所在的行数，找不到返回0
  Sample     Find_Left_Up_Point(int start,int end);
  @note      角点检测阈值可根据实际值更改
-------------------------------------------------------------------------------------------------------------------*/
int Find_Left_Up_Point(int start,int end)//找四个角点，返回值是角点所在的行数
{
    int i,t;
    int left_up_line=0;
    if(Left_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没有拐点判断的意义
       return left_up_line;
    if(start<end)
    {
        t=start;
        start=end;
        end=t;
    }
    if(end<=MT9V03X_H-Search_Stop_Line)//搜索截止行往上的全都不判
        end=MT9V03X_H-Search_Stop_Line;
    if(end<=5)//及时最长白列非常长，也要舍弃部分点，防止数组越界
        end=5;
    if(start>=MT9V03X_H-1-5)
        start=MT9V03X_H-1-5;
    for(i=start;i>=end;i--)
    {
        if(left_up_line==0&&//只找第一个符合条件的点
           abs(Left_Line[i]-Left_Line[i-1])<=5&&
           abs(Left_Line[i-1]-Left_Line[i-2])<=5&&  
           abs(Left_Line[i-2]-Left_Line[i-3])<=5&&
              (Left_Line[i]-Left_Line[i+2])>=8&& 
              (Left_Line[i]-Left_Line[i+3])>=15&&
              (Left_Line[i]-Left_Line[i+4])>=15)
        {
            left_up_line=i;//获取行数即可
            break;
        }
    }
    return left_up_line;//如果是MT9V03X_H-1，说明没有这么个拐点
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     右下角点检测
  @param     起始点，终止点
  @return    返回角点所在的行数，找不到返回0
  Sample     Find_Right_Down_Point(int start,int end);
  @note      角点检测阈值可根据实际值更改
-------------------------------------------------------------------------------------------------------------------*/
int Find_Right_Down_Point(int start,int end)//找四个角点，返回值是角点所在的行数
{
    int i,t;
    int right_down_line=0;
    if(Right_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没有拐点判断的意义
        return right_down_line;
    if(start<end)
    {
        t=start;
        start=end;
        end=t;
    }
    if(start>=MT9V03X_H-1-5)//下面5行数据不稳定，不能作为边界点来判断，舍弃
        start=MT9V03X_H-1-5;
    if(end<=MT9V03X_H-Search_Stop_Line)
        end=MT9V03X_H-Search_Stop_Line;
    if(end<=5)
       end=5;
    for(i=start;i>=end;i--)
    {
        if(right_down_line==0&&//只找第一个符合条件的点
           abs(Right_Line[i]-Right_Line[i+1])<=5&&//角点的阈值可以更改
           abs(Right_Line[i+1]-Right_Line[i+2])<=5&&  
           abs(Right_Line[i+2]-Right_Line[i+3])<=5&&
              (Right_Line[i]-Right_Line[i-2])<=-5&&
              (Right_Line[i]-Right_Line[i-3])<=-10&&
              (Right_Line[i]-Right_Line[i-4])<=-10)
        {
            right_down_line=i;//获取行数即可
            break;
        }
    }
    return right_down_line;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     右上角点检测
  @param     起始点，终止点
  @return    返回角点所在的行数，找不到返回0
  Sample     Find_Right_Up_Point(int start,int end);
  @note      角点检测阈值可根据实际值更改
-------------------------------------------------------------------------------------------------------------------*/
int Find_Right_Up_Point(int start,int end)//找四个角点，返回值是角点所在的行数
{
    int i,t;
    int right_up_line=0;
    if(Right_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没有拐点判断的意义
        return right_up_line;
    if(start<end)
    {
        t=start;
        start=end;
        end=t;
    }    
    if(end<=MT9V03X_H-Search_Stop_Line)//搜索截止行往上的全都不判
        end=MT9V03X_H-Search_Stop_Line;
    if(end<=5)//及时最长白列非常长，也要舍弃部分点，防止数组越界
        end=5;
    if(start>=MT9V03X_H-1-5)
        start=MT9V03X_H-1-5;
    for(i=start;i>=end;i--)
    {
        if(right_up_line==0&&//只找第一个符合条件的点
           abs(Right_Line[i]-Right_Line[i-1])<=5&&//下面两行位置差不多
           abs(Right_Line[i-1]-Right_Line[i-2])<=5&&  
           abs(Right_Line[i-2]-Right_Line[i-3])<=5&&
              (Right_Line[i]-Right_Line[i+2])<=-8&&
              (Right_Line[i]-Right_Line[i+3])<=-15&&
              (Right_Line[i]-Right_Line[i+4])<=-15)
        {
            right_up_line=i;//获取行数即可
            break;
        }
    }
    return right_up_line;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     单调性突变检测
  @param     起始点，终止行
  @return    点所在的行数，找不到返回0
  Sample     Find_Right_Up_Point(int start,int end);
  @note      前5后5它最大（最小），那他就是角点
-------------------------------------------------------------------------------------------------------------------*/
int Monotonicity_Change_Left(int start,int end)//单调性改变，返回值是单调性改变点所在的行数
{
    int i;
    int monotonicity_change_line=0;
    if(Left_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没有单调性判断的意义
       return monotonicity_change_line;
    if(start>=MT9V03X_H-1-5)//数组越界保护，在判断第i个点时
       start=MT9V03X_H-1-5; //要访问它前后5个点，数组两头的点要不能作为起点终点
    if(end<=5)
        end=5;
    if(start<=end)//递减计算，入口反了，直接返回0
      return monotonicity_change_line;
    for(i=start;i>=end;i--)//会读取前5后5数据，所以前面对输入范围有要求
    {
        if(Left_Line[i]==Left_Line[i+5]&&Left_Line[i]==Left_Line[i-5]&&
        Left_Line[i]==Left_Line[i+4]&&Left_Line[i]==Left_Line[i-4]&&
        Left_Line[i]==Left_Line[i+3]&&Left_Line[i]==Left_Line[i-3]&&
        Left_Line[i]==Left_Line[i+2]&&Left_Line[i]==Left_Line[i-2]&&
        Left_Line[i]==Left_Line[i+1]&&Left_Line[i]==Left_Line[i-1])
        {//一堆数据一样，显然不能作为单调转折点
            continue;
        }
        else if(Left_Line[i]>=Left_Line[i+5]&&Left_Line[i]>=Left_Line[i-5]&&
        Left_Line[i]>=Left_Line[i+4]&&Left_Line[i]>=Left_Line[i-4]&&
        Left_Line[i]>=Left_Line[i+3]&&Left_Line[i]>=Left_Line[i-3]&&
        Left_Line[i]>=Left_Line[i+2]&&Left_Line[i]>=Left_Line[i-2]&&
        Left_Line[i]>=Left_Line[i+1]&&Left_Line[i]>=Left_Line[i-1])
        {//就很暴力，这个数据是在前5，后5中最大的（可以取等），那就是单调突变点
            monotonicity_change_line=i;
            break;
        }
    }
    return monotonicity_change_line;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     单调性突变检测
  @param     起始点，终止行
  @return    点所在的行数，找不到返回0
  Sample     Find_Right_Up_Point(int start,int end);
  @note      前5后5它最大（最小），那他就是角点
-------------------------------------------------------------------------------------------------------------------*/
int Monotonicity_Change_Right(int start,int end)//单调性改变，返回值是单调性改变点所在的行数
{
    int i;
    int monotonicity_change_line=0;

    if(Right_Lost_Time>=0.9*MT9V03X_H)//大部分都丢线，没有单调性判断的意义
        return monotonicity_change_line;
    if(start>=MT9V03X_H-1-5)//数组越界保护
        start=MT9V03X_H-1-5;
     if(end<=5)
        end=5;
    if(start<=end)
        return monotonicity_change_line;
    for(i=start;i>=end;i--)//会读取前5后5数据，所以前面对输入范围有要求
    {
        if(Right_Line[i]==Right_Line[i+5]&&Right_Line[i]==Right_Line[i-5]&&
        Right_Line[i]==Right_Line[i+4]&&Right_Line[i]==Right_Line[i-4]&&
        Right_Line[i]==Right_Line[i+3]&&Right_Line[i]==Right_Line[i-3]&&
        Right_Line[i]==Right_Line[i+2]&&Right_Line[i]==Right_Line[i-2]&&
        Right_Line[i]==Right_Line[i+1]&&Right_Line[i]==Right_Line[i-1])
        {//一堆数据一样，显然不能作为单调转折点
            continue;
        }
        else if(Right_Line[i]<=Right_Line[i+5]&&Right_Line[i]<=Right_Line[i-5]&&
        Right_Line[i]<=Right_Line[i+4]&&Right_Line[i]<=Right_Line[i-4]&&
        Right_Line[i]<=Right_Line[i+3]&&Right_Line[i]<=Right_Line[i-3]&&
        Right_Line[i]<=Right_Line[i+2]&&Right_Line[i]<=Right_Line[i-2]&&
        Right_Line[i]<=Right_Line[i+1]&&Right_Line[i]<=Right_Line[i-1])
        {//就很暴力，这个数据是在前5，后5中最大的，那就是单调突变点
            monotonicity_change_line=i;
            break;
        }
    }
    return monotonicity_change_line;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     通过斜率，定点补线--
  @param     k       输入斜率
             startY  输入起始点纵坐标
             endY    结束点纵坐标
  @return    null
  Sample     K_Add_Boundry_Left(float k,int startY,int endY);
  @note      补得线直接贴在边线上
-------------------------------------------------------------------------------------------------------------------*/
void K_Add_Boundry_Left(float k,int startX,int startY,int endY)
{
    int i,t;
    if(startY>=MT9V03X_H-1)
        startY=MT9V03X_H-1;
    else if(startY<=0)
        startY=0;
    if(endY>=MT9V03X_H-1)
        endY=MT9V03X_H-1;
    else if(endY<=0)
        endY=0;
    if(startY<endY)//--操作，start需要大
    {
        t=startY;
        startY=endY;
        endY=t;
    }
    for(i=startY;i>=endY;i--)
    {
        Left_Line[i]=(int)((i-startY)/k+startX);//(y-y1)=k(x-x1)变形，x=(y-y1)/k+x1
        if(Left_Line[i]>=MT9V03X_W-1)
        {
            Left_Line[i]=MT9V03X_W-1;
        }
        else if(Left_Line[i]<=0)
        {
            Left_Line[i]=0;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     通过斜率，定点补线
  @param     k       输入斜率
             startY  输入起始点纵坐标
             endY    结束点纵坐标
  @return    null    直接补边线
  Sample     K_Add_Boundry_Right(float k,int startY,int endY);
  @note      补得线直接贴在边线上
-------------------------------------------------------------------------------------------------------------------*/
void K_Add_Boundry_Right(float k,int startX,int startY,int endY)
{
    int i,t;
    if(startY>=MT9V03X_H-1)
        startY=MT9V03X_H-1;
    else if(startY<=0)
        startY=0;
    if(endY>=MT9V03X_H-1)
        endY=MT9V03X_H-1;
    else if(endY<=0)
        endY=0;
    if(startY<endY)
    {
        t=startY;
        startY=endY;
        endY=t;
    }
    for(i=startY;i>=endY;i--)
    {
        Right_Line[i]=(int)((i-startY)/k+startX);//(y-y1)=k(x-x1)变形，x=(y-y1)/k+x1
        if(Right_Line[i]>=MT9V03X_W-1)
        {
            Right_Line[i]=MT9V03X_W-1;
        }
        else if(Right_Line[i]<=0)
        {
            Right_Line[i]=0;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     根据斜率划线
  @param     输入斜率，定点，画一条黑线
  @return    null
  Sample     K_Draw_Line(k, 20,MT9V03X_H-1 ,0)
  @note      补的就是一条线，需要重新扫线
-------------------------------------------------------------------------------------------------------------------*/
void K_Draw_Line(float k, int startX, int startY,int endY)
{
    int endX=0;

    if(startX>=MT9V03X_W-1)//限幅处理
        startX=MT9V03X_W-1;
    else if(startX<=0)
        startX=0;
    if(startY>=MT9V03X_H-1)
        startY=MT9V03X_H-1;
    else if(startY<=0)
        startY=0;
    if(endY>=MT9V03X_H-1)
        endY=MT9V03X_H-1;
    else if(endY<=0)
        endY=0;
    endX=(int)((endY-startY)/k+startX);//(y-y1)=k(x-x1)变形，x=(y-y1)/k+x1
    Draw_Line(startX,startY,endX,endY);
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     获取平均赛宽
  @param     int start_line,int end_line，起始行，中止行
  @return    这几行赛宽平均值
  Sample     road_wide=Get_Road_Wide(68,69);
  @note      ++运算，向下寻找，算出平均赛宽
-------------------------------------------------------------------------------------------------------------------*/
int Get_Road_Wide(int start_line,int end_line)
{
    if(start_line>=MT9V03X_H-1)
        start_line=MT9V03X_H-1;
    else if(start_line<=0)
        start_line=0;
    if(end_line>=MT9V03X_H-1)
        end_line=MT9V03X_H-1;
    else if(end_line<=0)
        end_line=0;
    int i=0,t=0;
    int road_wide=0;
    if(start_line>end_line)//++访问，坐标反了互换
    {
        t=start_line;
        start_line=end_line;
        end_line=t;
    }
    for(i=start_line;i<=end_line;i++)
    {
        road_wide+=Right_Line[i]-Left_Line[i];
    }
    road_wide=road_wide/(end_line-start_line+1);//平均赛宽
    return road_wide;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     获取左赛道边界斜率
  @param     int start_line,int end_line，起始行，中止行
  @return    两点之间的斜率
  Sample     k=Get_Left_K(68,69);
  @note      两点之间得出斜率，默认第一个参数小，第二个参数大
-------------------------------------------------------------------------------------------------------------------*/
float Get_Left_K(int start_line,int end_line)
{
    if(start_line>=MT9V03X_H-1)
        start_line=MT9V03X_H-1;
    else if(start_line<=0)
        start_line=0;
    if(end_line>=MT9V03X_H-1)
        end_line=MT9V03X_H-1;
    else if(end_line<=0)
        end_line=0;
    float k=0;
    int t=0;
    if(start_line>end_line)//++访问，坐标反了互换
    {
        t=start_line;
        start_line=end_line;
        end_line=t;
    }
    k=(float)(((float)Left_Line[start_line]-(float)Left_Line[end_line])/(end_line-start_line+1));
    return k;
}

/*-------------------------------------------------------------------------------------------------------------------
  @brief     获取右赛道边界斜率
  @param     int start_line,int end_line，起始行，中止行
  @return    两点之间的斜率
  Sample     k=Get_Right_K(68,69);
  @note      两点之间得出斜率，默认第一个参数小，第二个参数大
-------------------------------------------------------------------------------------------------------------------*/
float Get_Right_K(int start_line,int end_line)
{
    if(start_line>=MT9V03X_H-1)
        start_line=MT9V03X_H-1;
    else if(start_line<=0)
        start_line=0;
    if(end_line>=MT9V03X_H-1)
        end_line=MT9V03X_H-1;
    else if(end_line<=0)
        end_line=0;
    float k=0;
    int t=0;
    if(start_line>end_line)//++访问，坐标反了互换
    {
        t=start_line;
        start_line=end_line;
        end_line=t;
    }
    k=(float)(((float)Right_Line[start_line]-(float)Right_Line[end_line])/(end_line-start_line+1));
    return k;
}
