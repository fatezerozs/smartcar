#include "IMG_DEAL.h"
//#define  IMG_DATA    mt9v03x_image
 #include <math.h>
 #include "Threshold.h"
//----------------±äÁ¿¶¨ÒåÇø--------------
// 新增：斑马线检测全局变量
ZebraState zebra_state = ZEBRA_NONE;
int zebra_stop_delay = 0;
// 在 IMG_DEAL.c 开头全局变量区域添加
int zebra_detect_count = 0;  // 斑马线识别次数计数器，初始为0
 bool in_zebra_zone = false;        // 是否处于当前斑马线识别区间（避免同一段多帧计数）
int zebra_not_detected_frames = 0; // 连续未识别到斑马线的帧数（判定离开）
#define ZEBRA_LEAVE_FRAMES 15      // 连续15帧未识别到 = 离开斑马线（可根据帧率调整，比如10-20帧）
#define ZEBRA_ENTER_FRAMES 3       // 连续3帧识别到 = 确认进入斑马线（防抖）
int zebra_detected_continuous = 0; // 连续识别到斑马线的帧数（防抖）
 
 
int  dj_end;
unsigned char stop=1;//Í£³µ±êÖ¾Î»£¬ÖÃ1±íÊ¾Í£³µ
unsigned char go=0;//Ö±ÐÐ±êÖ¾Î»£¬1±íÊ¾Ö±ÐÐ
unsigned char deal_flag=0;//´¦Àí±êÖ¾Î»
unsigned char fork_flag=0;//Èý²æ±êÖ¾Î»
unsigned char T_go_flag[2]={1,0};//TÍä²¹Ïß±êÖ¾Î»£¬0ÓÒ1×ó
unsigned char T_go_flag_pin=0;//TÍä²¹Ïß±êµÚ¶þ£¬¾ö¶¨±êÖ¾Î»£¬0ÓÒ1×ó
unsigned char fork_turn=2;//1×ó£¬2ÓÒ
uint8 distance_flag=0;
uint8 fork_times=0;
uint8 Boost_flag=0;
ALL_enum element_flag=NO_JUGED;
FORK_enum Fork_Flag=NO_FORK;
T_CONNER_enum T_flag=NO_T;
Garage_enmum Garage_flag=GET_OUT;
Roundabout_enmum Roundabout_flag=NO_ROUND;
Roundabout_position_enmum Roundabout_flag_position=NOROUND;
Ramp_enmum ramp_flag=NO_RAMP;
int speed_flag=254;
int round_row_flag;
int row_lim=20;
//int i = 0,j = 0;

//********************************************

//Ñ²Ïß
int right_flag = 0;		//ÓÒÆðµã´æÔÚ±êÖ¾
int left_flag = 0;
int right_start = 0;	//ÓÒ±ßÏßÆðµãÐÐ
int left_start = 0;
int right_end_row = 0;		//ÓÒ±ßÏßÖÕµãÐÐ
int left_end_row = 0;
int right_end_col = 0;      //ÓÒ±ßÏßÖÕµãÐÐ
int left_end_col = 0;
int middle_start = 0;	//ÖÐÏßÆðµãÐÐ
int middle_end = 0;
int middleline[240];
int middleline0[240];//ÉÏÒ»ÖÐÏß
int middleline_l[240];
int middleline_r[240];
int SUM=0;
int wild1,wild10;
struct DIV 
{ 
	int Row[240]; 
	int Col[240]; 
};

struct AG
{
	float k;
	float SIN;
};

struct APEX
{
	int Apex_Row;
	int Apex_Col;
	int Mark;
};

struct Vector
{
    int star[2];
    int end[2];
};

struct DIV RacingLine;//Ä¿±êÏßÂ·
struct DIV left,right;//×óÓÒ±ßÏß
struct DIV forck_L,forck_R;//Èý²æÅÐ¶ÏÏßÂ·
struct DIV T_line;//TÍäÅÐ¶ÏÏß
struct DIV forck_line_L,forck_line_R;//Èý²æ²¹ÏßÏßÂ·
struct DIV Round_L,Round_R;//»·µº²¹ÏßÏßÂ·
struct APEX left_apex,right_apex;
struct AG right_ag,left_ag;//Ñ°ÕÒÍäÐÄËùÐè±äÁ¿
struct Vector Vector_r,Vector_l;//ÆðÖ¹µã½üËÆÇÐÏßÏòÁ¿

int right_line[120] = {159};//´Ë´¦ÐèÒª¸Ä³É159£¬µ«²»ÖªµÀÎªºÎÔËÐÐÃ»³öÎÊÌâ£¬¿ÉÄÜÊÇÃ»Óöµ½Ö»ÓÐ×óµ¥±ßµÄÇé¿ö£¬È¥Ñ§Ð£ÔÙÅÜÒ»ÏÂ³µÊÔÊÔ

int left_line[120] = {0};
int max = 0;
int min  = 0;
int line[120] = {0};
int Threshold;
unsigned char IMG_DATA[ROW][COL];
int mid_row_R[240];
int mid_row_L[240];
int mid_row[240];
int mid_flag=0;
int L_lenth,R_lenth;
//static int COUNT=0;
float Angle_end;
float Angle_End[10]={0};
float ang_l,ang_r,ang;//Èý²æ½Ç¶ÈÅÐ¶Ï
float L_S,R_S;//±ß½ç·½²î
float K_left,K_right;
float CorssCol=0;
//*********************************************

//**********************************************
//Ò»Ð©ÈÃÈË¿´×ÅÊæ·þµãµÄ¶¨Òå
#define WHITE_IMG    255
#define BLACK_IMG    0
#define RED_IMG      128
#define BLUE_IMG     254
#define GREEN_IMG    100

#define u8      unsigned char   
#define u16     unsigned short

#define uint16_t unsigned short
#define uint32_t unsigned int
#define int16_t  short int
#define int32_t  int
//**********************************************



//--------------------------
//ÉãÏñÍ·Ç°Õ°Á¿£¬¸ù¾ÝÉãÏñÍ·Î»ÖÃÉèÖÃ£¬Êý×Ö´ú±íÈ¨ÖØ´óÐ¡£¬ÀýÈçµÚ1¸öÊýÎª1´ú±íµÚ0ÐÐµÄÈ¨ÖØÎª1
int weight[120]={
2,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,2,2,2,2,//21-30
4,4,4,4,4,4,4,4,4,4,//1-10
8,8,8,8,8,8,8,8,8,8,//11-20
10,10,10,10,10,10,10,10,10,10,//21-30
12,12,12,12,12,12,12,12,12,12,//31-40
14,14,14,14,14,14,14,14,14,14,//41-50
16,16,16,16,16,16,16,16,16,16,//51-60
10,10,10,10,10,10,10,10,10,10,//61-70
8,8,8,8,8,8,8,8,8,8,//71-80
4,4,4,4,4,4,4,4,4,4,//81-90
3,3,3,3,3,3,3,3,3,3,//91-100
};//11120

//--------------------------
int TrackWild[120]={
        2, 3, 5, 6, 7, 8,10,11,12,14,15,16,17,19,20,21,22,24,25,
        26,27,29,30,31,33,34,35,36,38,39,40,41,43,44,45,47,48,49,
        50,52,53,54,55,57,58,59,61,62,63,64,66,67,68,69,71,72,73,
        75,76,77,78,80,81,82,83,85,86,87,89,90,91,92,94,95,96,97,
        99,100,101,103,104,105,106,108,109,110,111,113,114,115,116,118,119,120,122,
        123,124,125,127,128,129,130,132,133,134,136,137,138,139,141,142,143,144,146,
        147,148,150,151,152,153};


//***************************************************************
//* º¯ÊýÃû³Æ£º 
//* ¹¦ÄÜËµÃ÷£º Í¼Ïñ´¦Àí×ÜÈë¿Ú
//* º¯Êý·µ»Ø£º ÎÞ
//* ±¸ ×¢£º    
//***************************************************************

//----------Ñ²Ïß------------------------

// ÏÖÔÚÎÒÃÇµÃµ½ÁËÒ»·ùÍ¼£¬·´Ó³ÁËÐ¡³µµ±Ç°Ê±¿ÌÔÚÈüµÀÉÏµÄÎ»ÖÃ¡£
// ÄÇÃ´ÎÒÃÇÎªÁË±ÜÃâÐ¡³µÅÜ³öÈüµÀ£¬Ê×ÏÈ¾ÍÒªÅÐ¶Ï³öÍ¼ÉÏÄÄÒ»ÇøÓòÎªÈüµÀ¡£
// °×É«µÄÈüµÀÓëÀ¶É«±³¾°£¬Ò»°ãÇé¿öÏÂÔÚÉãÏñÍ·¿´À´Çø±ðÊ®·ÖÃ÷ÏÔ¡£


void Deal_Init()
{
    int i,j;
   //²ÎÊý³õÊ¼»¯
    right_flag = 0;left_flag = 0;
    right_start = 254;left_start = 254;
    middle_start = 0;middle_end = 0;
//    for(i=0;i<ROW;i++)
//    {
//        for(j=0;j<COL;j++)
//        {
//            IMG_DATA[i][j]=mt9v03x_image[i][j];
//        }
//    }
    Threshold=zzkLQGetOSTU0200(LQOSTU_MIN02,LQOSTU_MAX02);
    for(i=0;i<ROW;i++)
    {
        for(j=0;j<COL;j++)
        {
            if(IMG_DATA[i][j] >= Threshold)
            {
                IMG_DATA[i][j]=WHITE_IMG;
            }
            else
            {
                IMG_DATA[i][j]=BLACK_IMG;
            }
        }
    }    
}

 void InitData() //³õÊ¼»¯±ß½çÏß
{ 
        int pin; 
        for(pin=0;pin<240;pin++) 
	{ 
		left.Row[pin]=254;//254´ú±íÎÞÐ§Êý¾Ý
		left.Col[pin]=254; 
		right.Row[pin]=254;
		right.Col[pin]=254;
		if(Fork_Flag==NO_FORK)
		{
		    forck_L.Row[pin]=254;
		    forck_L.Col[pin]=254;
		    forck_R.Row[pin]=254;
		    forck_R.Col[pin]=254;
		}
		if(Roundabout_flag==NO_ROUND)
		{
		    Round_L.Row[pin]=254;
		    Round_L.Col[pin]=254;
		    Round_R.Row[pin]=254;
		    Round_R.Col[pin]=254;
		}
		T_line.Row[pin]=254;
		T_line.Col[pin]=254;
        RacingLine.Row[pin]=254;
        RacingLine.Col[pin]=254;
        middleline[pin]=254;
        middleline_l[pin]=254;
        middleline_r[pin]=254;
        mid_row_R[pin]=254;
        mid_row_L[pin]=254;
        mid_row[pin]=254;
	}
    //gpio_set(FMQ,0);
    speed_flag=254;
    L_S=0;
    R_S=0;
    left_apex.Mark=254;
	right_apex.Mark=254;
	Vector_r.star[0]=254;
	Vector_r.star[1]=254;
	Vector_r.end[0]=254;
	Vector_r.end[1]=254;
    Vector_l.star[0]=254;
    Vector_l.star[1]=254;
    Vector_l.end[0]=254;
    Vector_l.end[1]=254;
    L_lenth=0;
    R_lenth=0;
    fork_flag=0;
    ang_l=0;
    ang_r=0;
    ang=0;
}

 ///***************************************************************
 //* º¯ÊýÃû³Æ£º Black_White_Zone
 //* ¹¦ÄÜËµÃ÷£º ºÚ°×Çø£¬É¨ÃèÇøÓòÄÚºÚ°×µã
 //* º¯Êý·µ»Ø£º 0£¬1£¨ºÚÇø»ò°×Çø£©
 //* ±¸ ×¢£º    ×óÉÏµ½ÓÒÏÂ,xÁÐ,yÐÐ
 //***************************************************************
 int Black_White_Zone(int x1,int y1,int x2,int y2)
 {
     int row,col;
     int point=0;
     int Black_point=0;
     int White_point=0;
     float B_percent,W_percent;
     float k1,k2;
     k1=1.0*(y2-y1)/(x2-x1);
     k2=1.0*(y2-y1)/(x1-x2);
     for(col=x1;col<x2;col++)
     {
         row=(int)(k1*(col-x1)+y1);
         if(IMG_DATA[row][col]==BLACK_IMG) Black_point++;
         else White_point++;
         point++;
     }
     for(col=x1;col<x2;col++)
     {
         row=(int)(k1*(col-x1)+y2);
         if(IMG_DATA[row][col]==BLACK_IMG) Black_point++;
         else White_point++;
         point++;
     }
     row=(y1+y2)/2;
     for(col=x1;col<x2;col++)
     {
         if(IMG_DATA[row][col]==BLACK_IMG) Black_point++;
         else White_point++;
         point++;
     }
     col=(x1+x2)/2;
     for(row=y1;row<y2;row++)
     {
         if(IMG_DATA[row][col]==BLACK_IMG) Black_point++;
         else White_point++;
         point++;
     }
     B_percent=1.0*Black_point/point;
     W_percent=1.0*White_point/point;
     if(B_percent>0.6) return 1;
     else if(W_percent>0.6) return 2;
     else return 0;
 }

 ///***************************************************************
 //* º¯ÊýÃû³Æ£º Cross_col
 //* ¹¦ÄÜËµÃ÷£º ¹áÍ¨ÁÐ£¬¸¨ÖúÅÐ¶ÏÖ±µÀ
 //* º¯Êý·µ»Ø£º °×µãÕ¼±È£¨float£©
 //* ±¸ ×¢£º
 //***************************************************************
 float Cross_col()
 {
     float W_percent=0;
     int i,j;
     int White_point=0;
     int Black_point=0;
     int point=600;
     int col[5]={84,90,94,98,102};
     for(i=0;i<5;i++)
     {
         for(j=119;j>0;j--)
         {
             if(IMG_DATA[j][col[i]]==WHITE_IMG)
                 White_point++;
             else
             {
                 Black_point++;
                 if(Black_point>4)
                 {
                     Black_point=0;
                     break;
                 }
             }
         }
     }
     W_percent=1.0*White_point/point;
     return W_percent;
 }
 float CrossRow_L(int star)
 {
      float W_percent=0;
      int i,j;
      int White_point=0;
      int Black_point=0;
      int point=470;
      int row[5];
      if(star>=100) return -1;
      for(i=0;i<5;i++) row[i]=star+5*i;
      for(i=0;i<5;i++)
      {
          for(j=94;j>5;j--)
          {
              if(IMG_DATA[row[i]][j]==WHITE_IMG)
                  White_point++;
              else if(IMG_DATA[row[i]][j]==BLACK_IMG)
              {
                  Black_point++;
                  if(Black_point>4)
                  {
                      Black_point=0;
                      break;
                  }
              }
          }
      }
      W_percent=(float)White_point/point;
      return W_percent;
 }

 float CrossRow_R(int star)
 {
      float W_percent=0;
      int i,j;
      int White_point=0;
      int Black_point=0;
      int point=470;
      int row[5];
      if(star>=100) return -1;
      for(i=0;i<5;i++) row[i]=star+5*i;
      for(i=0;i<5;i++)
      {
          for(j=94;j<183;j++)
          {
              if(IMG_DATA[row[i]][j]==WHITE_IMG)
                  White_point++;
              else if(IMG_DATA[row[i]][j]==BLACK_IMG)
              {
                  Black_point++;
                  if(Black_point>4)
                  {
                      Black_point=0;
                      break;
                  }
              }
          }
      }
      W_percent=(float)White_point/point;
      return W_percent;
 }
 ///***************************************************************
 //* º¯ÊýÃû³Æ£º S_jugde
 //* ¹¦ÄÜËµÃ÷£º ±ê×¼²îÅÐ¶Ï£¬ÐÐÎªy
 //* º¯Êý·µ»Ø£º ±ß½çÏßÓëÄâºÏÇúÏß±ê×¼²î
 //* ±¸ ×¢£º    ÅÐ¶Ï±ß½ç±ê×¼Îó²î
 //***************************************************************
 float S_jugde(int row[],int col[],int N)
 {
     int xy=0;
     int x_sq=0;
     float k,b;
     float S;
     float x_avg=0;
     float y_avg=0;
     float x_avg_sq=0;
     int y_tg;
     int i,num;
     S=254;
     for(i=0,num=0;i<240;i++)
     {
         if(col[i]==254||i==N)
         {
             x_avg=x_avg/num;
             break;
         }
         if(row[i]==row[i+1]) continue;
         x_avg+=col[i];
         num++;
     }
     x_avg_sq=x_avg*x_avg;
     for(i=0;i<240;i++)
     {
         if(row[i]==254||i==N)
         {
             y_avg=y_avg/num;
             break;
         }
         if(row[i]==row[i+1]) continue;
         y_avg+=row[i];
     }
     for(i=0;i<240;i++)
     {
         if(row[i]==254||col[i]==254||i==N) break;
         if(row[i]==row[i+1]) continue;
         xy+=(row[i]*col[i]);
     }
     for(i=0;i<240;i++)
     {
         if(col[i]==254||i==N) break;
         if(row[i]==row[i+1]) continue;
         x_sq+=(col[i]*col[i]);
     }
     k=(xy-num*x_avg*y_avg)/(x_sq-num*x_avg_sq);
     b=y_avg-k*x_avg;
     for(i=0;i<240;i++)
     {
         if(row[i]==254||i==N)
         {
             S=sqrt(S/(num-1));
             break;
         }
         if(row[i]==row[i+1]) continue;
         y_tg=k*col[i]+b;
         S+=(y_tg-row[i])*(y_tg-row[i]);
     }
     S=S/sqrt(1.0*num);
     return S;
 }


///***************************************************************
//* º¯ÊýÃû³Æ£º 
//* ¹¦ÄÜËµÃ÷£º ÕÒ×óÆðµã
//* º¯Êý·µ»Ø£º 
//* ±¸ ×¢£º²ÎÕÕÍ¼Ïñ´¦ÀíÖ¸ÄÏ±àÐ´    
//***************************************************************
void LeftStartFind() 
{ 
  int row,col;
  for(row=ROW-5;row>40 && left.Row[0]==254;row--)
  {
    for(col = 5;col<COL - 55 && left.Col[0] == 254;col++)
    {
      if(IMG_DATA[row][col]==BLACK_IMG && IMG_DATA[row][col+1]==BLACK_IMG)
      {
        if(IMG_DATA[row][col+2]==WHITE_IMG && IMG_DATA[row][col+3]==WHITE_IMG)
        {
           left.Row[0]=row;
           left.Col[0]=col+1;
           left_flag=1;
        }
      }
    }
  }
  left_start=left.Row[0];
}
  
//***************************************************************
//* º¯ÊýÃû³Æ£º 
//* ¹¦ÄÜËµÃ÷£º ÕÒÓÒÆðµã
//* º¯Êý·µ»Ø£º ÎÞ
//* ±¸ ×¢£º    
//***************************************************************
void RightStartFind() 
{
  int row,col;
  for(row=ROW-5;row > 40 && right.Row[0] == 254;row--)
  {
    for(col=COL - 5;col > 55 && right.Col[0] == 254;col--)
    {
      if(IMG_DATA[row][col]==BLACK_IMG && IMG_DATA[row][col-1]==BLACK_IMG)
      {
        if(IMG_DATA[row][col-2]==WHITE_IMG && IMG_DATA[row][col-3]==WHITE_IMG)
        {
        right.Row[0]=row;
        right.Col[0]=col-1;
        right_flag=1;
        }
      }
    }
  }
  right_start=right.Row[0];
}




void left_jump()
{
	int pin;//, i, j;
	int row, col,find;
//	int row_temp, col_temp;
//	int whitecounter = 0;
//	int rowcheack[12] = {-1, 1, -1, 0, 2, 0, 0, -1, -1, 1, 0, 1};
//	int colcheack[12] = {1, 0, -1, -1, 2, -1, -1, 0, 0, 1, -2, 1};
    int colmin, colmax;
    if(left.Row[0] == 254)
	{
		return;
	}
    left_flag=1;
    pin=1;
    for(row=left.Row[0] - pin;pin<240;pin++)
	{
		find = 0;
		row = left.Row[0] - pin;
		if(row<=row_lim)break;
		colmin = left.Col[pin - 1] - 10;
		colmax = left.Col[pin - 1] + 10;
		for(col = colmin;col <= colmax;col++)
		{
		    if(col<5) col=5;
			if(col < COL - 5)
			{
				if(IMG_DATA[row][col] == BLACK_IMG && IMG_DATA[row][col + 1] == BLACK_IMG)
				{
					if(IMG_DATA[row][col + 2] == WHITE_IMG && IMG_DATA[row][col + 3] == WHITE_IMG)
					{
						left.Row[pin] = row;
						left.Col[pin] = col + 1;
						IMG_DATA[row][col + 1] = RED_IMG;
						find = 1;
						L_lenth++;
						left_end_row=row;
						left_end_col=col;
						break;
					}
				}
			}
		}
		if(find == 0)
		{
			break;
		}
    }
	if(L_lenth<=5)
	{
	    left_flag=0;
	    left.Row[0]=254;
	    left.Col[0]=254;
	}

}
void right_jump()
{
	int pin;//, i, j;
	int row, col,find;
//	int row_temp, col_temp;
//	int whitecounter = 0;
    int colmin, colmax;
//	int rowcheack[12] = {-1, 1, -1, 0, 2, 0, 0, -1, -1, 1, 0, 1};
//	int colcheack[12] = {-1, 0, 1, 1, -2, 1, 1, 0, 0, -1, 2, -1};
    if(right.Row[0] == 254)
	{
		return;
	}
    right_flag=1;
    for(pin=1,row=right.Row[0] - pin;pin<240;pin++)
	{
		find = 0;
		row = right.Row[0] - pin;
		if(row<=row_lim)break;
		colmin = right.Col[pin - 1] - 10;
		colmax = right.Col[pin - 1] + 10;
		for(col = colmax;col >= colmin;col--)
		{
		    if(col > COL - 5) col=COL - 5;
			if(col > 5)
			{
				if(IMG_DATA[row][col] == BLACK_IMG && IMG_DATA[row][col - 1] == BLACK_IMG)
				{
					if(IMG_DATA[row][col - 2] == WHITE_IMG && IMG_DATA[row][col - 3] == WHITE_IMG)
					{
						right.Row[pin] = row;
						right.Col[pin] = col - 1;
						IMG_DATA[row][col - 1] = BLUE_IMG;
						find = 1;
						R_lenth++;
						right_end_row=row;
						right_end_col=col;
						break;
					}
				}
			}
		}
		if(find == 0)
		{
			break;
		}
	}
	if(R_lenth<=5)
	{
	    right_flag=0;
	    right.Row[0]=254;
	    right.Col[0]=254;
	}
  

}

//***************************************************************
//* º¯ÊýÃû³Æ£º fixpoint()
//* ¹¦ÄÜËµÃ÷£º×óÓÒÆðÊ¼µãÐÞÕý
//* º¯Êý·µ»Ø£º 
//* ±¸ ×¢£º    
//***************************************************************
void fixpoint()//×óÓÒ±ßÏßÆðÊ¼µãµÄÐÞÕý
{
    if(left.Col[0] == 254 && left.Row[0]==254)
	{
		return;
	}
	if(right.Col[0] == 254 && right.Row[0] == 254)
	{
		return;
	}
	float k = 0;
	int UpPointCol = 254, UpPointRow = 254;
	int DownPointCol = 254,DownPointRow = 254;
	int row, col, pin, BP;
	BP = 0;
	if(left.Row[0] < right.Row[0])
	{
		UpPointCol = left.Col[0];
		UpPointRow = left.Row[0];
		DownPointCol = right.Col[0];
		DownPointRow = right.Row[0];
	}
	else
	{
		UpPointCol = right.Col[0];
		UpPointRow = right.Row[0];
		DownPointCol = left.Col[0];
		DownPointRow = left.Row[0];
	}
	if(UpPointRow != DownPointRow)
	{
		k = (float)(UpPointCol - DownPointCol) / (UpPointRow - DownPointRow);
	}
	for(pin=0, row = DownPointRow;row >= UpPointRow;pin++,row--)
	{
		col = (int)(DownPointCol - k * pin);
		if(col > -1 && col < COL && row >-1 && row < ROW)
		{
			if(IMG_DATA[row][col] == BLACK_IMG)
			{
				BP++;
			}
		}
	}
	if(BP > 8)
	{
		if(left.Row[0] < right.Row[0])
		{
			left.Col[0]=254;
			left.Row[0]=254;
            left_flag=0;
		}
		else if(left.Row[0] > right.Row[0])
		{
			right.Col[0]=254;
			right.Row[0]=254;
            right_flag=0;
		}
	}
}

void Left_Apex()
{
    if(left.Row[0]==254&&left.Col[0]==254)
    {
        return;
    }
    int pin,col_max=0;
    int row_flag,col_flag;
    for(pin=0;pin<240;pin++)
    {
        if(left.Row[pin]==254)
        {
            break;
        }
        if(col_max <= left.Col[pin])
        {
            col_max=left.Col[pin];
            row_flag=left.Row[pin];
            col_flag=left.Col[pin];
            left_apex.Mark=pin;
        }
    }
    left_apex.Apex_Row=row_flag;
    left_apex.Apex_Col=col_flag;
	L_S=S_jugde(left.Row,left.Col,240);
}//Ñ°ÕÒ×óÍäÐÄ

void Right_Apex()
{
    if(right.Row[0]==254&&right.Col[0]==254)
    {
        return;
    }
    int pin;
    int row_flag,col_flag;
    int col_min=188;
    for(pin=0;pin<240;pin++)
    {
        if(right.Row[pin]==254)
        {
            break;
        }
        if(right.Col[pin] <= col_min)
        {
            col_min=right.Col[pin];
            row_flag=right.Row[pin];
            col_flag=right.Col[pin];
            right_apex.Mark=pin;
        }
        //if(right.Row[pin]<row_min) break;
    }
    right_apex.Apex_Row=row_flag;
    right_apex.Apex_Col=col_flag;
	R_S=S_jugde(right.Row,right.Col,240);
}//Ñ°ÕÒÓÒÍäÐÄ

//***************************************************************
//* º¯ÊýÃû³Æ£º Get_Center_Line
//* ¹¦ÄÜËµÃ÷£º ÕÒÖÐÏß
//* º¯Êý·µ»Ø£º 
//* ±¸ ×¢£º    line[]Êý×éÐèÒª×Ô¼º¶¨Òå£¬ÓÃÀ´±íÊ¾ÈüµÀµÄ¿í¶È£¬±ÈÈçline[5]±íÊ¾µÚÎåÐÐÈüµÀµÄÊµ¼Ê¿í¶È
//ÐèÒª¿¼ÂÇµ½ÉãÏñÍ·¿´Èüµ½Ê±£¬¿´µÄÔ½Ô¶£¬ÈüµÀÍ¼ÏñÏÔÊ¾µÄÔ½Õ­Õâ¸öÒòËØ
//***************************************************************
void Racing_Line()
{
        int pin,i;
        int row,col;
        int flag;
        int mark;
        int l_flag,r_flag;
        l_flag=0;
        r_flag=0;
        i=0;
        if(left_flag==0&&right_flag==0)
        {
          return;
        }
        if(left_flag==0)
        {
            mark=right_apex.Mark+(115-right.Row[5]);
            row=right.Row[5];
            i=5;
            for(pin=0;pin<mark;pin++)
            {
                flag=119-pin;
                if(flag>right.Row[5])
                {
                  middleline[pin]=right.Col[5]-TrackWild[row]/2;
                  col=middleline[pin];
                  mid_row[pin]=flag;
                  IMG_DATA[flag][col]=GREEN_IMG;
                }
                else
                {
                  row=right.Row[i];
                  col=right.Col[i]-TrackWild[row]/2;
                  middleline[pin]=col;
                  mid_row[pin]=row;
                  i++;
                  if(row>5&&row<ROW-5)
                  {
                    if(col>5&&col<COL-5)
                    {
                        IMG_DATA[row][col]=GREEN_IMG;
                    }
                  }
                }
            }
            return;
        }
        else if(right_flag==0)
        {
          mark=left_apex.Mark+(115-left.Row[5]);
          row=left.Row[5];
          i=5;
          for(pin=0;pin<mark;pin++)
            {
              flag=119-pin;
              if(flag>left.Row[5])
              {
                middleline[pin]=left.Col[5]+TrackWild[row]/2;
                  col=middleline[pin];
                  IMG_DATA[flag][col]=GREEN_IMG;
                  mid_row[pin]=flag;
              }
              else
              {
                row=left.Row[i];
                col=left.Col[i]+TrackWild[row]/2;
                middleline[pin]=col;
                mid_row[pin]=row;
                i++;
                if(row>5&&row<ROW-5)
                {
                  if(col>5&&col<COL-5)
                  {
                      IMG_DATA[row][col]=GREEN_IMG;
                  }
                }
              }
            }
            return;
        }
        if(left_apex.Mark>=right_apex.Mark)
        {
            mark=left_apex.Mark+(115-left.Row[5]);
            row=left.Row[5];
            i=5;
            for(pin=0;pin<mark;pin++)
            {
              flag=119-pin;
              if(flag>left.Row[5])
              {
                middleline[pin]=left.Col[5]+TrackWild[row]/2;
                  col=middleline[pin];
                  mid_row[pin]=flag;
                  IMG_DATA[flag][col]=GREEN_IMG;
              }
              else
              {
                row=left.Row[i];
                col=left.Col[i]+TrackWild[row]/2;
                middleline[pin]=col;
                mid_row[pin]=row;
                i++;
                if(row>5&&row<ROW-5)
                {
                  if(col>5&&col<COL-5)
                  {
                      IMG_DATA[row][col]=GREEN_IMG;
                  }
                }
              }
              l_flag++;
            }
            return;
        }
        else if(left_apex.Mark<right_apex.Mark)
        {
            mark=right_apex.Mark+(115-right.Row[5]);
            row=right.Row[5];
            i=5;
            for(pin=0;pin<mark;pin++)
            {
              flag=119-pin;
              if(flag>right.Row[5])
              {
                middleline[pin]=right.Col[5]-TrackWild[row]/2;
                  col=middleline[pin];
                  mid_row[pin]=flag;
                  IMG_DATA[flag][col]=GREEN_IMG;
              }
              else
              {
                row=right.Row[i];
                col=right.Col[i]-TrackWild[row]/2;
                middleline[pin]=col;
                i++;
                mid_row[pin]=row;
                if(row>5&&row<ROW-5)
                {
                  if(col>5&&col<COL-5)
                  {
                      IMG_DATA[row][col]=GREEN_IMG;
                  }
                }
              }
              r_flag++;
            }
            return;
        }
}

 //***************************************************************
//* º¯ÊýÃû³Æ£º void protect()
//* ¹¦ÄÜËµÃ÷£º ³µÈç¹ûÅÜ³öÈüµÀ£¬Í£³µ
//* º¯Êý·µ»Ø£º 
//* ±¸ ×¢£º    0±íÊ¾ºÚÉ« 255±íÊ¾°×É«£¬´ó¼Ò×Ô¼ºË¼¿¼Ò»ÏÂÔõÃ´Ð´
//***************************************************************


//***************************************************************
//* º¯ÊýÃû³Æ£º Conner_cheack
//* ¹¦ÄÜËµÃ÷£º ¼ì²éÍäµÀ
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void Conner_cheack()
{

}

//***************************************************************
//* º¯ÊýÃû³Æ£º Roundabout_Deal
//* ¹¦ÄÜËµÃ÷£º »·µº´¦Àíº¯Êý
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void Roundabout_Deal()
{
    int row,col,i,find;
    int col_min,col_max;
    float cross_row_L,cross_row_R;
    float cross_col,crossrow_juged;
    float round_l_s,round_r_s;
    float k;
    cross_col=CorssCol;
    find=0;
    switch(Roundabout_flag)
    {
        case NO_ROUND:
            cross_row_L=CrossRow_L(65);
            cross_row_R=CrossRow_R(65);
            if(element_flag==IN_JUGED && Roundabout_flag==NO_ROUND)
            {
                Roundabout_flag_position=NOROUND;
                Roundabout_flag=NO_ROUND;
                return;
            }
            if(cross_row_L>0.9 && cross_row_R<0.8 && R_S<0.4)
            {
                Roundabout_flag_position=ROUND_L;
                Roundabout_flag=ROUND_READY;
                element_flag=NO_JUGED;
            }
            else if(cross_row_L<0.8 && cross_row_R>0.8 && L_S<0.4)
            {
                Roundabout_flag_position=ROUND_R;
                Roundabout_flag=ROUND_READY;
                element_flag=NO_JUGED;
            }
            if(right_apex.Apex_Col<78 || left_apex.Apex_Col>110)
            {
                Roundabout_flag_position=NOROUND;
                Roundabout_flag=NO_ROUND;
                return;
            }
            break;
        case ROUND_READY:
            round_l_s=L_S;
            round_r_s=R_S;
            if(right_apex.Apex_Col<78 || left_apex.Apex_Col>110)
            {
                Roundabout_flag_position=NOROUND;
                Roundabout_flag=NO_ROUND;
                return;
            }
            if(element_flag==IN_JUGED)
            {
                Roundabout_flag_position=NOROUND;
                Roundabout_flag=NO_ROUND;
                return;
            }
            if(CorssCol<0.75)
            {
                Roundabout_flag=NO_ROUND;
                element_flag=NO_JUGED;
                return;
            }
            switch (Roundabout_flag_position)
            {
                case ROUND_L:
                    if(right_apex.Apex_Col<78 || R_lenth<80 || L_lenth<20)
                    {
                        Roundabout_flag_position=NOROUND;
                        Roundabout_flag=NO_ROUND;
                        return;
                    }
                    if(round_l_s>0.9 && round_r_s<0.4)
                    {
                        Roundabout_flag=GET_IN_READY;
                        element_flag=NO_JUGED;
                    }
                    else if(round_r_s>0.5)
                    {
                        Roundabout_flag=NO_ROUND;
                        element_flag=NO_JUGED;
                    }
                    break;
                case ROUND_R:
                    if(left_apex.Apex_Col>110 || L_lenth<80 || R_lenth<20)
                    {
                        Roundabout_flag_position=NOROUND;
                        Roundabout_flag=NO_ROUND;
                        return;
                    }
                    if(round_l_s<0.4 && round_r_s>0.9)
                    {
                        Roundabout_flag=GET_IN_READY;
                        element_flag=NO_JUGED;
                    }
                    else if(round_l_s>0.5)
                    {
                        Roundabout_flag=NO_ROUND;
                        element_flag=NO_JUGED;
                    }
                    break;
                case NOROUND:
                    break;
            }
            break;
        case GET_IN_READY:
            round_l_s=L_S;
            round_r_s=R_S;
            cross_row_L=CrossRow_L(65);
            cross_row_R=CrossRow_R(65);
            switch (Roundabout_flag_position)
            {
                case ROUND_L:
                    col_min=left_apex.Apex_Col-10;
                    col_max=COL/2;
                    if(round_r_s>0.5)
                    {
                        Roundabout_flag=NO_ROUND;
                        element_flag=NO_JUGED;
                    }
                    for (row = 50; row > 40; row--)
{
    for (col = col_min; col < col_max; col++)
    {
        // ? ????????(?->? ? ?->?),??????
        if ( (IMG_DATA[row][col]   == WHITE_IMG && IMG_DATA[row][col+1] == BLACK_IMG) ||
             (IMG_DATA[row][col]   == BLACK_IMG && IMG_DATA[row][col+1] == WHITE_IMG) )
        {
            Round_L.Row[0] = row;
            Round_L.Col[0] = col + 1;   // ??????

            crossrow_juged = CrossRow_L(row);

            // ? ??????,??“?????”???
            if (crossrow_juged > 0.6f && cross_col > 0.6f)
            {
                find = 1;
                Roundabout_flag = GET_IN_ROUND;
                element_flag    = IN_JUGED;
                break;
            }
        }
    }
    if (find) break;
}

                    break;
                case ROUND_R:
                    col_min=COL/2;
                    col_max=right_apex.Apex_Col+10;
                    if(round_l_s>0.5)
                    {
                        Roundabout_flag=NO_ROUND;
                        element_flag=NO_JUGED;
                    }
                   for (row = 50; row > 40; row--)
{
    for (col = col_max; col > col_min; col--)
    {
        if ( (IMG_DATA[row][col]   == WHITE_IMG && IMG_DATA[row][col-1] == BLACK_IMG) ||
             (IMG_DATA[row][col]   == BLACK_IMG && IMG_DATA[row][col-1] == WHITE_IMG) )
        {
            Round_R.Row[0] = row;
            Round_R.Col[0] = col - 1;

            crossrow_juged = CrossRow_R(row);

            if (crossrow_juged > 0.6f && cross_col > 0.6f)
            {
                find = 1;
                Roundabout_flag = GET_IN_ROUND;
                element_flag    = IN_JUGED;
                break;
            }
        }
    }
    if (find) break;
}

                    break;
                case NOROUND:
                    break;
            }
            break;
        case GET_IN_ROUND:
            //gpio_set(FMQ,1);
            row_lim=25;
            switch (Roundabout_flag_position)
            {
                case ROUND_L:
                    if(right_apex.Apex_Col<47) Roundabout_flag=IN_ROUND;
                    break;
                case ROUND_R:
                    if(left_apex.Apex_Col>141) Roundabout_flag=IN_ROUND;
                    break;
                case NOROUND:
                    break;
            }
            break;
        case IN_ROUND:
            row_lim=15;
            round_row_flag=254;
            switch (Roundabout_flag_position)
            {
                case ROUND_L:
                    if(right_apex.Apex_Col>94)
                    {
                        Roundabout_flag=GET_OUT_ROUND;
                        element_flag=IN_JUGED;
                        k=1.0*(right_apex.Apex_Col-right.Col[0])/(right_apex.Apex_Row-right.Row[0]);
                        for(i=0;i<120;i++)
                        {
                            if(i<right_apex.Mark)
                            {
                                Round_L.Row[i]=right.Row[i];
                                Round_L.Col[i]=right.Col[i];
                            }
                            else
                            {
                                Round_L.Row[i]=Round_L.Row[i-1]-1;
                                Round_L.Col[i]=k*(Round_L.Row[i]-right_apex.Apex_Row)+right_apex.Apex_Col;
                            }
                        }
                    }
                    break;
                case ROUND_R:
                    if(left_apex.Apex_Col<94)
                    {
                        Roundabout_flag=GET_OUT_ROUND;
                        element_flag=IN_JUGED;
                        k=1.0*(left_apex.Apex_Col-left.Col[0])/(left_apex.Apex_Row-left.Row[0]);
                        for(i=0;i<120;i++)
                        {
                            if(i<left_apex.Mark)
                            {
                                Round_R.Row[i]=left.Row[i];
                                Round_R.Col[i]=left.Col[i];
                            }
                            else
                            {
                                Round_R.Row[i]=Round_R.Row[i-1]-1;
                                Round_R.Col[i]=k*(Round_R.Row[i]-left_apex.Apex_Row)+left_apex.Apex_Col;
                            }
                        }
                    }
                    break;
                case NOROUND:
                    break;
            }
            break;
        case GET_OUT_ROUND:
            switch (Roundabout_flag_position)
            {
                case ROUND_L:
                    cross_row_R=CrossRow_R(75);
                    if(cross_row_R>0.9)
                    {
                        Roundabout_flag=OUTTING;
                        element_flag=IN_JUGED;
                    }
                    break;
                case ROUND_R:
                    cross_row_L=CrossRow_L(75);
                    if(cross_row_L>0.9)
                    {
                        Roundabout_flag=OUTTING;
                        element_flag=IN_JUGED;
                    }
                    break;
                case NOROUND:
                    break;
            }
            break;
         case OUTTING:
             switch (Roundabout_flag_position)
             {
                 case ROUND_L:
                     cross_row_R=CrossRow_R(75);
                     if(cross_row_R<0.8)
                     {
                         Roundabout_flag=OUT_WAY;
                         element_flag=IN_JUGED;
                     }
                     break;
                 case ROUND_R:
                     cross_row_L=CrossRow_L(75);
                     if(cross_row_L<0.8)
                     {
                         Roundabout_flag=OUT_WAY;
                         element_flag=IN_JUGED;
                     }
                     break;
                 case NOROUND:
                     break;
             }
             break;
         case OUT_WAY:
             switch (Roundabout_flag_position)
             {
                 case ROUND_L:
                     cross_row_L=CrossRow_L(75);
                     cross_row_R=CrossRow_R(75);
                     if(cross_row_L>0.9 && cross_row_R<0.6)
                     {
                         Roundabout_flag=OUT_ROUND;
                         element_flag=IN_JUGED;
                         }
                     break;
                 case ROUND_R:
                     cross_row_L=CrossRow_L(75);
                     cross_row_R=CrossRow_R(75);
                     if(cross_row_R>0.9 && cross_row_L<0.6)
                     {
                         Roundabout_flag=OUT_ROUND;
                         element_flag=IN_JUGED;
                     }
                     break;
                 case NOROUND:
                     break;
             }
             break;
         case OUT_ROUND:
             switch (Roundabout_flag_position)
             {
                 case ROUND_L:
                     cross_row_L=CrossRow_L(75);
                     if(cross_row_L<0.7)
                     {
                         Roundabout_flag=NO_ROUND;
                         element_flag=NO_JUGED;
                     }
                     break;
                 case ROUND_R:
                     cross_row_R=CrossRow_R(75);
                     if(cross_row_R<0.7)
                     {
                         Roundabout_flag=NO_ROUND;
                         element_flag=NO_JUGED;
                     }
                     break;
                 case NOROUND:
                     break;
             }
             break;
    }
}

//**************************************************************
//* º¯ÊýÃû³Æ£º T_Conner_Deal
//* ¹¦ÄÜËµÃ÷£º TÍä´¦Àíº¯Êý
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void T_Conner_Deal()
{
    float L_cross,R_cross,Col_cross;
    int star_row;
    Col_cross=Cross_col();
    star_row=(ROW-(int)ROW*Col_cross)+5;
    if(star_row>100) return;
    L_cross=CrossRow_L(star_row);
    R_cross=CrossRow_R(star_row);
    if(element_flag==IN_JUGED)
    {
        if(T_flag==NO_T) return;
    }
    if(T_flag==IN_T)
    {
        speed_flag=75;
        if(Col_cross>0.7 || R_lenth>40 || L_lenth>40)
        {
            T_flag=NO_T;
//            if(T_go_flag_pin==1) T_go_flag_pin=0;
//            else T_go_flag_pin=1;
            element_flag=NO_JUGED;
        }
        else
        {
            //gpio_set(FMQ,1);
            return;
        }
    }
    if((left_apex.Apex_Row>80 || right_apex.Apex_Row>80) && Col_cross<0.75 && T_flag==NO_T)
    {
        int row_min,row_max,row,col;
        int sum=0;
        row_min=star_row-10;
        row_max=star_row+10;
        for(col=5;col<COL-5;col+=5)
        {
            for(row=row_max;row>row_min;row--)
            {
                if(row<5) break;
                if(row>115) row=115;
                if(IMG_DATA[row][col]==WHITE_IMG&&IMG_DATA[row-1][col]==WHITE_IMG)
                {
                    if(IMG_DATA[row-2][col]==BLACK_IMG && IMG_DATA[row-3][col]==BLACK_IMG)
                    {
                        sum++;
                        break;
                    }
                }
            }
        }
        if(sum>24)
        {
            element_flag=IN_JUGED;
            T_flag=IN_T;
            //gpio_set(FMQ,1);
        }
    }
}

//***************************************************************
//* º¯ÊýÃû³Æ£º Fork_Deal
//* ¹¦ÄÜËµÃ÷£º Èý²æ´¦Àíº¯Êý
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void Fork_Deal()
{
    int i,j,pin;
    int F_L_flag=0;
    int F_R_flag=0;
    int B_L,B_R,B_Top;
    B_L=Black_White_Zone(5,80,25,110);
    B_Top=Black_White_Zone(60,15,115,30);
    B_R=Black_White_Zone(170,80,185,110);
    if(element_flag==IN_JUGED)
    {
        if(Fork_Flag==NO_FORK) return;
    }
    if(Fork_Flag==IN_FORK)
    {
        if(B_Top!=1)
        {
            fork_times++;
            Fork_Flag=NO_FORK;
            element_flag=NO_JUGED;
            return;
        }
    }
    if(B_Top!=1) return;
    for (i=70;i>40;i--)
    {
        int min=COL/2-20,max=COL/2+20;
        for(j=min;j<max;j++)
        {
            if(IMG_DATA[i][j]==WHITE_IMG && IMG_DATA[i][j+1]==BLACK_IMG)
            {
                forck_L.Row[0]=i;
                forck_L.Col[0]=j+1;
                F_L_flag=1;
            }
            if(IMG_DATA[i][j]==BLACK_IMG && IMG_DATA[i][j+1]==WHITE_IMG)
            {
                forck_R.Row[0]=i;
                forck_R.Col[0]=j-1;
                F_R_flag=1;
            }
        }
        if(F_L_flag==0 && F_R_flag==1)
        {
            return;
        }
        if(F_R_flag==0 && F_L_flag==1)
        {
            return;
        }
        if(forck_R.Row[0]<forck_L.Row[0]) return;//·ÀÎóÅÐ2
        if(F_R_flag==1 && F_L_flag==1)
        {
            break;
        }
    }
    int find;
    int row,col;
    int colmin,colmax;
    //ÏÂÎª×ó
    pin=1;
    for(row=forck_L.Row[0] - pin;pin<15;pin++)
    {
        find = 0;
        row = forck_L.Row[0] - pin;
        colmin = forck_L.Col[pin - 1] - 10;
        colmax = forck_L.Col[pin - 1] + 10;
        for(col = colmin;col <= colmax;col++)
        {
            if(col < COL - 5 && col > 5)
            {
                if(IMG_DATA[row][col] == WHITE_IMG && IMG_DATA[row][col + 1] == WHITE_IMG)
                {
                    if(IMG_DATA[row][col + 2] == BLACK_IMG && IMG_DATA[row][col + 3] == BLACK_IMG)
                    {
                        forck_L.Row[pin] = row;
                        forck_L.Col[pin] = col + 2;
                        find = 1;
                        break;
                    }
                }
            }
        }
        if(find == 0)
        {
            break;
        }
    }
    //ÏÂÎªÓÒ
    pin=1;
    for(row=right.Row[0] - pin;pin<15;pin++)
    {
        find = 0;
        row = forck_R.Row[0] - pin;
        colmin = forck_R.Col[pin - 1] - 10;
        colmax = forck_R.Col[pin - 1] + 10;
        for(col = colmax;col >= colmin;col--)
        {
            if(col < COL - 5 && col > 5)
            {
                if(IMG_DATA[row][col] == WHITE_IMG && IMG_DATA[row][col - 1] == WHITE_IMG)
                {
                    if(IMG_DATA[row][col - 2] == BLACK_IMG && IMG_DATA[row][col - 3] == BLACK_IMG)
                    {
                        forck_R.Row[pin] = row;
                        forck_R.Col[pin] = col - 2;
                        find = 1;
                        break;
                    }
                }
            }
        }
        if(find == 0)
        {
            break;
        }
    }
    int V_L[2],V_R[2];
    if(forck_L.Row[4]==254||forck_L.Row[4]==254) return;
    V_L[0]=forck_L.Row[4]-forck_L.Row[0];
    V_L[1]=forck_L.Col[4]-forck_L.Col[0];
    V_R[0]=forck_R.Row[4]-forck_R.Row[0];
    V_R[1]=forck_R.Col[4]-forck_R.Col[0];
    ang_l=atan(1.0*V_L[0]/V_L[1]);
    ang_r=atan(1.0*V_R[0]/V_R[1]);
    ang_l=ang_l*180/3.14;
    ang_r=ang_r*180/3.14+180;
    ang=ang_r-ang_l;
    if(ang>150 && ang<180)
    {
        float cross_r,cross_l;
        cross_r=CrossRow_R(forck_R.Row[0]-2);
        cross_l=CrossRow_L(forck_L.Row[0]-2);
        if(cross_r>0.6 && cross_l>0.6)
        {
            element_flag=IN_JUGED;
            Fork_Flag=IN_FORK;
        }
    }
}

//***************************************************************
//* º¯ÊýÃû³Æ£º Garage_Deal
//* ¹¦ÄÜËµÃ÷£º ³µ¿âº¯Êý
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void Garage_Deal()
{
    Boost();
    if(Garage_flag==GET_OUT)
    {
        element_flag=IN_JUGED;
        if(CorssCol>0.7 || R_lenth>40 || L_lenth>40)
        {
            Garage_flag=NO_GARAGE;
            element_flag=NO_JUGED;
            fork_times=0;
        }
        return;
    }
    else if(Garage_flag!=GET_OUT)
    {
        if(element_flag==IN_JUGED) return;
        int row,col;
        int col_min,col_max;
        int T,T0,count,BM_count;
        T=0,T0=0;
        count=0;
        BM_count=0;
        for(row=110;row>40;row-=5)
        {
            col_min=COL/2-TrackWild[row]/2;
            col_max=COL/2+TrackWild[row]/2;
            for(col=col_min;col<col_max;col++)
            {
                if(IMG_DATA[row][col]==BLACK_IMG && IMG_DATA[row][col+1]==BLACK_IMG && IMG_DATA[row][col+2]==WHITE_IMG && IMG_DATA[row][col+3]==WHITE_IMG)
                {
                    if(T-T0>-5 && T-T0<5) count++;
                    T0=T;
                    T=0;
                }
                else
                {
                    T++;
                }
                if(count>=5)
                {
                    count=0;
                    BM_count++;
                    break;
                }
            }
        }
        if(BM_count>=2 && Garage_flag==NO_GARAGE)
        {
            T_go_flag_pin=1;
            Garage_flag=FIRST;
        }
        else if(BM_count==0 && Garage_flag==FIRST)
            Garage_flag=READY;
        else if(BM_count>=1 && Garage_flag==READY)
        {
            Garage_flag=GET_IN;
            element_flag=IN_JUGED;
        }
    }
}



//***************************************************************
//* º¯ÊýÃû³Æ£º RacingLine_L
//* ¹¦ÄÜËµÃ÷£º ×ó±ßÏßÑ°µ½µ×£¬µ«ÊÇ¸üÆ«(1/3)
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void RacingLine_L(int k)
{
    int pin,i;
    int row,col;
    int flag;
    int mark;
    int l_flag,r_flag;
    l_flag=0;
    r_flag=0;
    i=0;
    mark=L_lenth+(115-left.Row[0]);
    if(mark>240) mark=240;
    row=left.Row[0];
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(left.Row[i]==254) break;
        if(flag>left.Row[0])
        {
            middleline[pin]=left.Col[0]+TrackWild[row]/k;
            col=middleline[pin];
            IMG_DATA[flag][col]=GREEN_IMG;
            mid_row[pin]=flag;
        }
        else
        {
            row=left.Row[i];
            col=left.Col[i]+TrackWild[row]/k;
            middleline[pin]=col;
            mid_row[pin]=row;
            i++;
            if(row>5&&row<ROW-5)
            {
                if(col>5&&col<COL-5)
                {
                    IMG_DATA[row][col]=GREEN_IMG;
                }
            }
        }

    }
    return;
}

//***************************************************************
//* º¯ÊýÃû³Æ£º RacingLine_R
//* ¹¦ÄÜËµÃ÷£º ÓÒ±ßÏßÑ°µ½µ×£¬µ«ÊÇ¸üÆ«(1/3)
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void RacingLine_R(int k)
{
    int pin,i;
    int row,col;
    int flag;
    int mark;
    int l_flag,r_flag;
    l_flag=0;
    r_flag=0;
    i=0;
    if(left_flag==0&&right_flag==0)
    {
        return;
    }
    mark=R_lenth+(115-right.Row[0]);
    if(mark>240) mark=240;
    row=right.Row[0];
    for(pin=0;pin<mark;pin++)
    {
        flag=119-pin;
        if(right.Row[i]==254) break;
        if(flag>right.Row[0])
        {
            middleline[pin]=right.Col[0]-TrackWild[row]/k;
            col=middleline[pin];
            mid_row[pin]=flag;
            IMG_DATA[flag][col]=GREEN_IMG;
        }
        else
        {
            row=right.Row[i];
            col=right.Col[i]-TrackWild[row]/k;
            middleline[pin]=col;
            mid_row[pin]=row;
            i++;
            if(row>5&&row<ROW-5)
            {
                if(col>5&&col<COL-5)
                {
                    IMG_DATA[row][col]=GREEN_IMG;
                }
            }
        }
    }
    return;
}

//***************************************************************
//* º¯ÊýÃû³Æ£º Racing_Line_Fork_L
//* ¹¦ÄÜËµÃ÷£º Èý²æÑ²Ïß²ßÂÔ£¬²¹ÓÒÑ°×ó
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void Racing_Line_Fork_L()
{
    int pin,i;
    int row,col;
    int flag;
    int l_flag,r_flag;
    l_flag=0;
    r_flag=0;
    i=0;
    row=forck_L.Row[0];
    //gpio_set(FMQ,1);
    if(right_apex.Apex_Col<94)
    {
        RacingLine_R(2);
        return;
    }
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(forck_L.Row[i]==254) break;
        if(flag>forck_L.Row[0])
        {
            middleline[pin]=forck_L.Col[0]-TrackWild[row]/2;
            col=middleline[pin];
            mid_row[pin]=flag;
            IMG_DATA[flag][col]=GREEN_IMG;
        }
        else
        {
              row=forck_L.Row[i];
              col=forck_L.Col[i]-TrackWild[row]/2;
              middleline[pin]=col;
              mid_row[pin]=row;
              i++;
              if(row>5&&row<ROW-5)
              {
                    if(col>5&&col<COL-5)
                    {
                        IMG_DATA[row][col]=GREEN_IMG;
                    }
              }
        }
     }
    return;
}

//***************************************************************
//* º¯ÊýÃû³Æ£º Racing_Line_Fork_R
//* ¹¦ÄÜËµÃ÷£º Èý²æÑ²Ïß²ßÂÔ£¬²¹×óÑ°ÓÒ
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//***************************************************************
void Racing_Line_Fork_R()
{
    int pin,i;
    int row,col;
    int flag;
    int l_flag,r_flag;
    l_flag=0;
    r_flag=0;
    i=0;
    row=forck_R.Row[0];
    //gpio_set(FMQ,1);
    if(left_apex.Apex_Col>94)
    {
        RacingLine_L(2);
        return;
    }
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(forck_R.Row[i]==254) break;
        if(flag>forck_L.Row[0])
        {
            middleline[pin]=forck_R.Col[0]+TrackWild[row]/2;
            col=middleline[pin];
            mid_row[pin]=flag;
            IMG_DATA[flag][col]=GREEN_IMG;
        }
        else
        {
              row=forck_R.Row[i];
              col=forck_R.Col[i]+TrackWild[row]/2;
              middleline[pin]=col;
              mid_row[pin]=row;
              i++;
              if(row>5&&row<ROW-5)
              {
                    if(col>5&&col<COL-5)
                    {
                        IMG_DATA[row][col]=GREEN_IMG;
                    }
              }
        }
     }
    return;
}

//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_T_L
//* ¹¦ÄÜËµÃ÷£º T²¹Ïß£¬Ïò×ó
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void Raceing_line_T_L()
{
    int i,pin;
    if(left_flag==1)
    {
        RacingLine_L(10);
    }
    else
    {
        right_flag=1;
        left_flag=1;
        for(i=120,pin=0;i>60;i--)
        {
            middleline[pin]=0;
            mid_row[pin]=i;
            pin++;
        }
    }
}

//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_T_R
//* ¹¦ÄÜËµÃ÷£º T²¹ÏßÏòÓÒ
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void Raceing_line_T_R()
{
    int i,pin;
    if(right_flag==1)
    {
        RacingLine_R(10);
    }
    else
    {
        right_flag=1;
        left_flag=1;
        for(i=120,pin=0;i>60;i--)
        {
            middleline[pin]=COL;
            mid_row[pin]=i;
            pin++;
        }
    }
}

//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_G_L
//* ¹¦ÄÜËµÃ÷£º ³öÈë³µ¿â£¨Ïò×ó£©
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
//**************************************************************
void Raceing_line_G_L()
{
    int i, pin;

    // -------- ?????:???,????? --------
    if (Roundabout_flag == GET_OUT_ROUND || Roundabout_flag == OUTTING)
    {
        for (i = 120, pin = 0; i > 60; i--, pin++)
        {
            int w = TrackWild[i < ROW ? i : ROW - 1]; /* virtual row 120 has no table entry */
            if (w <= 0) w = 40;                 // ????

            // ??:??????? 1/4 ???,???? 0 ?
            int col = (COL / 2) - w / 4;
            if (col < 5) col = 5;

            middleline[pin] = col;
            mid_row[pin]    = i;

            // ??:??????
            if (i > 5 && i < ROW - 5 && col > 5 && col < COL - 5)
                IMG_DATA[i][col] = GREEN_IMG;
        }
        return;
    }

    // -------- ????(????),???????? --------
    if (left_flag == 0 || Garage_flag == GET_IN)
    {
        right_flag = 1;
        left_flag  = 1;
        for (i = 120, pin = 0; i > 60; i--, pin++)
        {
            middleline[pin] = 0;
            mid_row[pin]    = i;
        }
    }
}



//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_G_R
//* ¹¦ÄÜËµÃ÷£º ³öÈë³µ¿â£¨ÏòÓÒ£©
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
void Raceing_line_G_R()
{
    int i, pin;

    if (Garage_flag == GET_IN)
    {
        speed_flag = 60;   // ????????
    }

    // -------- ?????:???,????? --------
    if (Roundabout_flag == GET_OUT_ROUND || Roundabout_flag == OUTTING)
    {
        for (i = 120, pin = 0; i > 60; i--, pin++)
        {
            int w = TrackWild[i < ROW ? i : ROW - 1]; /* virtual row 120 has no table entry */
            if (w <= 0) w = 40;

            // ??:??????? 1/4 ???,???? 188
            int col = (COL / 2) + w / 4;
            if (col > COL - 5) col = COL - 5;

            middleline[pin] = col;
            mid_row[pin]    = i;

            if (i > 5 && i < ROW - 5 && col > 5 && col < COL - 5)
                IMG_DATA[i][col] = GREEN_IMG;
        }
        return;
    }

    // -------- ????(????),???????? --------
    if (right_flag == 0 || Garage_flag == GET_IN)
    {
        right_flag = 1;
        left_flag  = 1;
        for (i = 120, pin = 0; i > 60; i--, pin++)
        {
            middleline[pin] = 188;   // ??????
            mid_row[pin]    = i;
        }
    }
}


//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_RoundIn_L
//* ¹¦ÄÜËµÃ÷£º ×ó»·µº½øÈë²¹Ïß
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void Raceing_line_RoundIn_L()
{
    int pin,pinn;//, i, j;
    int row, col,find;
    int colmin, colmax;
    int flag,i;
    pinn=0;
    i=0;
    if(Round_L.Row[0] == 254)
    {
        return;
    }
    for(pin=1,row=Round_L.Row[0] - pin;pin<240;pin++)
    {
        find = 0;
        row = Round_L.Row[0] - pin;
        if(row<=15)break;
        colmin = Round_L.Col[pin - 1] - 10;
        colmax = Round_L.Col[pin - 1] + 10;
        for(col = colmax;col >= colmin;col--)
        {
            if(col > COL - 5) col=COL - 5;
            if(col > 5)
            {
                if(IMG_DATA[row][col] == BLACK_IMG && IMG_DATA[row][col - 1] == BLACK_IMG)
                {
                    if(IMG_DATA[row][col - 2] == WHITE_IMG && IMG_DATA[row][col - 3] == WHITE_IMG)
                    {
                        Round_L.Row[pin] = row;
                        Round_L.Col[pin] = col - 1;
                        find = 1;
                        break;
                    }
                }
            }
        }
        if(find == 0)
        {
            break;
        }
    }
    row=Round_L.Row[0];
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(flag>Round_L.Row[0])
        {
            if(right.Col[pinn]!=254)
            {
                middleline[pin]=Round_L.Col[0]-TrackWild[row]/2;
                col=middleline[pin];
                mid_row[pin]=right.Row[pinn];
                IMG_DATA[flag][col]=GREEN_IMG;
                pinn++;
            }
        }
        else
        {
            if(Round_L.Row[i]==254) break;
            row=Round_L.Row[i];
            col=Round_L.Col[i]-TrackWild[row]/2;
            middleline[pin]=col;
            mid_row[pin]=row;
            i++;
            if(row>5&&row<ROW-5)
            {
                if(col>5&&col<COL-5)
                {
                    IMG_DATA[row][col]=GREEN_IMG;
                }
            }
        }
    }
    return;
}

//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_RoundIn_R
//* ¹¦ÄÜËµÃ÷£º ÓÒ»·µº½øÈë²¹Ïß
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void Raceing_line_RoundIn_R()
{
    int pin,pinn;//, i, j;
    int row, col,find;
    int colmin, colmax;
    int flag,i;
    i=0;
    pinn=0;
    if(Round_R.Row[0] == 254)
    {
        return;
    }
    pin=1;
    for(row=Round_R.Row[0] - pin;pin<240;pin++)
    {
        find = 0;
        row = Round_R.Row[0] - pin;
        if(row<=15)break;
        colmin = Round_R.Col[pin - 1] - 10;
        colmax = Round_R.Col[pin - 1] + 10;
        for(col = colmin;col <= colmax;col++)
        {
            if(col<5) col=5;
            if(col < COL - 5)
            {
                if(IMG_DATA[row][col] == BLACK_IMG && IMG_DATA[row][col + 1] == BLACK_IMG)
                {
                    if(IMG_DATA[row][col + 2] == WHITE_IMG && IMG_DATA[row][col + 3] == WHITE_IMG)
                    {
                        Round_R.Row[pin] = row;
                        Round_R.Col[pin] = col + 1;
                        find = 1;
                        break;
                    }
                }
            }
        }
        if(find == 0)
        {
            break;
        }
    }
    row=Round_R.Row[0];
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(flag>Round_R.Row[0])
        {
            if(left.Col[pinn]!=254)
            {
                middleline[pin]=Round_R.Col[0]+TrackWild[row];
                col=middleline[pin];
                mid_row[pin]=left.Row[pinn];
                IMG_DATA[flag][col]=GREEN_IMG;
                pinn++;
            }
        }
        else
        {
            if(Round_R.Row[i]==254) break;
            row=Round_R.Row[i];
            col=Round_R.Col[i]+TrackWild[row];
            middleline[pin]=col;
            mid_row[pin]=row;
            i++;
            if(row>5&&row<ROW-5)
            {
                if(col>5&&col<COL-5)
                {
                    IMG_DATA[row][col]=GREEN_IMG;
                }
            }
        }
    }
    return;
}

//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_RoundIn_L
//* ¹¦ÄÜËµÃ÷£º ×ó»·µºÀë¿ª²¹Ïß
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void Raceing_line_RoundOUT_L()
{
    int i,pin;
    if(right_flag==1)
    {
        RacingLine_R(2);
        return;
    }
    for(i=120,pin=0;i>60;i--)
    {
        middleline[pin]=0;
        mid_row[pin]=i;
        pin++;
    }
    return;
}

//**************************************************************
//* º¯ÊýÃû³Æ£º Raceing_line_RoundIn_L
//* ¹¦ÄÜËµÃ÷£º ÓÒ»·µºÀë¿ª²¹Ïß
//* º¯Êý·µ»Ø£º
//* ±¸ ×¢£º
//**************************************************************
void Raceing_line_RoundOUT_R()
{
    int i,pin;
    if(left_flag==1)
    {
        RacingLine_L(2);
        return;
    }
    for(i=120,pin=0;i>60;i--)
    {
        middleline[pin]=188;
        mid_row[pin]=i;
        pin++;
    }
    return;
}

void Raceing_line_RoundIn_L_Blan_B()
{
    int row,col,pin,find,i;
    int col_min,col_max,flag;
    int star_row,star_col,star_flag;
    int max_row,max_col;
    float k;
    find=0;max_row=0;max_col=0;
    i=1;
    col_min=left_apex.Apex_Col+1;
    col_max=right_apex.Apex_Col;
    for(col=col_max;col>col_min;col--)
    {
        if(Roundabout_flag==IN_ROUND)
        {
            max_row=right.Row[0];
            max_col=right.Col[0];
            break;
        }
        if(col<5) break;
        if(left_apex.Apex_Row<right_apex.Apex_Row+20 && left.Row[0]<100)
        {
            max_row=left.Row[0];
            max_col=left.Col[0];
            break;
        }
        for(row=left_end_row-5;row>5;row--)
        {
            if(IMG_DATA[row][col]==WHITE_IMG && IMG_DATA[row-1][col]==WHITE_IMG)
            {
                if(IMG_DATA[row-2][col]==BLACK_IMG && IMG_DATA[row-3][col]==BLACK_IMG)
                {
                    if(row>max_row)
                    {
                        max_row=row;
                        max_col=col;
                    }
                    break;
                }
            }
        }
    }//ÖÕµã
    Round_L.Row[0]=max_row;
    Round_L.Col[0]=max_col;
    star_row=115;
    star_col=180;
    for(pin=0;pin<240;pin++)
    {
        if(Roundabout_flag==IN_ROUND)
        {
            star_row=115;
            star_col=180;
            break;
        }
        if(right.Row[pin]==254) break;
        if(left_apex.Apex_Row<right_apex.Apex_Row+20 && left.Row[0]<100)
        {
            star_row=115;
            star_col=180;
            break;
        }
        if(right.Row[pin]<=left_apex.Apex_Row)
        {
            star_row=right.Row[pin];
            star_col=right.Col[pin];
            star_flag=pin;
            break;
        }
    }//Æðµã
    IMG_DATA[star_row][star_col]=GREEN_IMG;
    k=(star_col*1.0-Round_L.Col[0])/(star_row*1.0-Round_L.Row[0]);
    row=star_row;
    col=star_col;
    for(pin=1;pin<240;pin++)
    {
        if(star_row-pin>=Round_L.Row[0])
        {
            Round_L.Row[pin]=star_row-pin;
            Round_L.Col[pin]=star_col-(int)pin*k;
            IMG_DATA[Round_L.Row[pin]][Round_L.Col[pin]]=BLUE_IMG;
        }
        else if(Roundabout_flag==IN_ROUND)
        {
            if(right.Row[pin]==254) break;
            Round_L.Row[pin]=right.Row[pin];
            Round_L.Col[pin]=right.Col[pin];
        }
        else
        {
            find = 0;
            row = star_row - pin;
            if(row<=15)break;
            col_min = Round_L.Col[pin - 1] - 10;
            col_max = Round_L.Col[pin - 1] + 10;
            for(col = col_max;col >= col_min;col--)
            {
                if(col > COL - 5) col=COL - 5;
                if(col > 5)
                {
                    if(IMG_DATA[row][col] == BLACK_IMG && IMG_DATA[row][col - 1] == BLACK_IMG)
                    {
                        if(IMG_DATA[row][col - 2] == WHITE_IMG && IMG_DATA[row][col - 3] == WHITE_IMG)
                        {
                            Round_L.Row[pin] = row;
                            Round_L.Col[pin] = col - 1;
                            if(pin>1) IMG_DATA[Round_L.Row[pin]][Round_L.Col[pin]]=BLUE_IMG;
                            find = 1;
                            break;
                        }
                    }
                }
            }
            if(find == 0)
            {
                break;
            }
        }
    }//²¹Ïß
    row=Round_L.Row[1];
    i=1;
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(Round_L.Row[i]==254) break;
        if(flag>Round_L.Row[1])
        {
            middleline[pin]=Round_L.Col[1]-TrackWild[row]/2;
            col=middleline[pin];
            mid_row[pin]=flag;
            IMG_DATA[flag][col]=GREEN_IMG;
        }
        else
        {
            row=Round_L.Row[i];
            col=Round_L.Col[i]-TrackWild[row]/2;
            middleline[pin]=col;
            mid_row[pin]=row;
            i++;
            if(row>5&&row<ROW-5)
            {
                if(col>5&&col<COL-5)
                {
                    IMG_DATA[row][col]=GREEN_IMG;
                }
            }
        }
    }//Ñ²Ïß
}

void Raceing_line_RoundIn_R_Blan_B()
{
    int row,col,pin,find,i;
    int col_min,col_max,flag;
    int star_row,star_col,star_flag;
    int max_row,max_col;
    float k;
    find=0;max_row=0;max_col=188;
    i=1;
    col_min=left_apex.Apex_Col;
    col_max=right_apex.Apex_Col-1;
    for(col=col_min;col<col_max;col++)
    {
        if(col>COL-5) break;
        if(left_apex.Apex_Row+20>right_apex.Apex_Row && right.Row[0]<100)
        {
            max_row=right.Row[0];
            max_col=right.Col[0];
            break;
        }
        for(row=right_end_row-5;row>5;row--)
        {
            if(IMG_DATA[row][col]==WHITE_IMG && IMG_DATA[row-1][col]==WHITE_IMG)
            {
                if(IMG_DATA[row-2][col]==BLACK_IMG && IMG_DATA[row-3][col]==BLACK_IMG)
                {
                    if(row>max_row)
                    {
                        max_row=row;
                        max_col=col;
                    }
                    break;
                }
            }
        }
    }//ÖÕµã
    Round_R.Row[0]=max_row;
    Round_R.Col[0]=max_col;
    round_row_flag=max_row;
    star_row=115;
    star_col=8;
    for(pin=0;pin<240;pin++)
    {
        if(left.Row[pin]==254) break;
        if(left_apex.Apex_Row+20>right_apex.Apex_Row && right.Row[0]<100)
        {
            star_row=115;
            star_col=8;
            break;
        }
        if(left.Row[pin]==right_apex.Apex_Row)
        {
            star_row=left.Row[pin];
            star_col=left.Col[pin];
            star_flag=pin;
            break;
        }
    }//Æðµã
    IMG_DATA[star_row][star_col]=GREEN_IMG;
    k=(star_col*1.0-Round_R.Col[0])/(star_row*1.0-Round_R.Row[0]);
    row=star_row;
    col=star_col;
    for(pin=1;pin<240;pin++)
    {
        if(star_row-pin>=Round_R.Row[0])
        {
            Round_R.Row[pin]=star_row-pin;
            Round_R.Col[pin]=star_col-(int)pin*k;
            IMG_DATA[Round_R.Row[pin]][Round_R.Col[pin]]=BLUE_IMG;
        }
        else
        {
            find = 0;
            row = star_row - pin;
            if(row<=15)break;
            col_min = Round_R.Col[pin - 1] - 10;
            col_max = Round_R.Col[pin - 1] + 10;
            for(col = col_min;col <= col_max;col++)
            {
                if(col < 5) col=5;
                if(col < COL - 5)
                {
                    if(IMG_DATA[row][col] == BLACK_IMG && IMG_DATA[row][col + 1] == BLACK_IMG)
                    {
                        if(IMG_DATA[row][col + 2] == WHITE_IMG && IMG_DATA[row][col + 3] == WHITE_IMG)
                        {
                            Round_R.Row[pin] = row;
                            Round_R.Col[pin] = col + 1;
                            if(pin>1) IMG_DATA[Round_R.Row[pin]][Round_R.Col[pin]]=BLUE_IMG;
                            find = 1;
                            break;
                        }
                    }
                }
            }
            if(find == 0)
            {
                break;
            }
        }
    }//²¹Ïß
    row=Round_R.Row[1];
    i=1;
    for(pin=0;pin<240;pin++)
    {
        flag=119-pin;
        if(Round_R.Row[i]==254) break;
        if(flag>Round_R.Row[1])
        {
            middleline[pin]=Round_R.Col[1]+TrackWild[row]/2;
            col=middleline[pin];
            mid_row[pin]=flag;
            IMG_DATA[flag][col]=GREEN_IMG;
        }
        else
        {
            row=Round_R.Row[i];
            col=Round_R.Col[i]+TrackWild[row]/2;
            middleline[pin]=col;
            mid_row[pin]=row;
            i++;
            if(row>5&&row<ROW-5)
            {
                if(col>5&&col<COL-5)
                {
                    IMG_DATA[row][col]=GREEN_IMG;
                }
            }
        }
    }//Ñ²Ïß
}

void RaceingLine_ramp()
{
    Racing_Line();
    speed_flag=60;
}

void RaceingLine_ramp_Plan_B()
{
    int pin,row,col,i;
    int mark_flag=0;;
    if(left_flag==0 || right_flag==0)
    {
        Racing_Line();
        middleline[60]=254;
        mid_row[60]=254;
    }
    else
    {
        if(left.Row[0]<right.Row[0])
        {
            for(pin=0;pin<240;pin++)
                if(right.Row[pin]==left.Row[0])
                    mark_flag=pin;
            row=119;i=0;
            for(pin=0;pin<240;pin++)
            {
                if(row<=60) break;
                if(row>left.Row[0])
                {
                    middleline[pin]=(left.Col[0]+right.Col[mark_flag])/2;
                    col=middleline[pin];
                    mid_row[pin]=row;
                    row--;
                    if(row>5&&row<ROW-5)
                    {
                        if(col>5&&col<COL-5)
                        {
                            IMG_DATA[row][col]=GREEN_IMG;
                        }
                    }
                }
                else
                {
                    if(left.Col[i]==254||right.Col[mark_flag+i]==254) break;
                    middleline[pin]=(left.Col[i]+right.Col[mark_flag+i])/2;
                    col=middleline[pin];
                    mid_row[pin]=row;
                    row--;
                    i++;
                    if(row>5&&row<ROW-5)
                    {
                        if(col>5&&col<COL-5)
                        {
                            IMG_DATA[row][col]=GREEN_IMG;
                        }
                    }
                }
            }
        }
        else
        {
            for(pin=0;pin<240;pin++)
                if(left.Row[pin]==right.Row[0])
                    mark_flag=pin;
            row=119;i=0;
            for(pin=0;pin<240;pin++)
            {
                if(row<=60) break;
                if(row>right.Row[0])
                {
                    middleline[pin]=(right.Col[0]+left.Col[mark_flag])/2;
                    col=middleline[pin];
                    mid_row[pin]=row;
                    row--;
                    if(row>5&&row<ROW-5)
                    {
                        if(col>5&&col<COL-5)
                        {
                            IMG_DATA[row][col]=GREEN_IMG;
                        }
                    }
                }
                else
                {
                    if(right.Col[i]==254||left.Col[mark_flag+i]==254) break;
                    middleline[pin]=(right.Col[i]+left.Col[mark_flag+i])/2;
                    col=middleline[pin];
                    mid_row[pin]=row;
                    row--;
                    i++;
                    if(row>5&&row<ROW-5)
                    {
                        if(col>5&&col<COL-5)
                        {
                            IMG_DATA[row][col]=GREEN_IMG;
                        }
                    }
                }
            }
        }
    }
}

void RaceLine()
{
    
    if(Roundabout_flag==GET_IN_ROUND)
    {
        //gpio_set(FMQ,1);
        if(Roundabout_flag_position==ROUND_L)
            Raceing_line_RoundIn_L_Blan_B();
        else if(Roundabout_flag_position==ROUND_R)
            Raceing_line_RoundIn_R_Blan_B();
    }
    else if(Roundabout_flag==IN_ROUND)
    {
        //gpio_set(FMQ,1);
        if(Roundabout_flag_position==ROUND_L)
        {
            if(right.Col[0]<140)
                RacingLine_R(2);
            else
                Racing_Line();
        }
        else if(Roundabout_flag_position==ROUND_R)
        {
            if(left.Col[0]>48)
                RacingLine_L(2);
            else
                Racing_Line();
        }
    }
    else if(Roundabout_flag==GET_OUT_ROUND)
    {
        //gpio_set(FMQ,1);
        if(Roundabout_flag_position==ROUND_L)
            Raceing_line_G_L();
        else if(Roundabout_flag_position==ROUND_R)
            Raceing_line_G_R();
    }
    else if(Roundabout_flag==OUTTING)
    {
        //gpio_set(FMQ,1);
        if(Roundabout_flag_position==ROUND_L)
            Raceing_line_G_L();
        else if(Roundabout_flag_position==ROUND_R)
            Raceing_line_G_R();
    }
    else if(Roundabout_flag==OUT_ROUND)
    {
        //gpio_set(FMQ,1);
        if(Roundabout_flag_position==ROUND_L)
            RacingLine_R(2);
        else if(Roundabout_flag_position==ROUND_R)
            RacingLine_L(2);
    }
    else if(ramp_flag==IN_RAMP) RaceingLine_ramp_Plan_B();
    else Racing_Line();
}

void Boost()
{
    if(Garage_flag==FIRST && Boost_flag==0)
    {
        Boost_flag=1;
        distance_flag=1;
    }
    if(Boost_flag)
    {
        speed_flag=120;
        
    }
}





void Zebra_Detect(void)
{
    if (zebra_state != ZEBRA_NONE) return;      // 已触发过就不再检测

    int zebra_row = 0;                          // 满足条件的行数
    const int row_start = 30;                   // 检测行范围
    const int row_end   = 100;
    /* 改为全幅宽检测，防止斑马线在右侧被漏掉 */
    const int col_start = 20;
    const int col_end   = 100;

    for (int row = row_start; row < row_end; row++)
    {
        int continuous_black = 0;   // 当前连续黑像素长度
        int trans_cnt        = 0;   // 黑白跳变次数（横向条纹特征）

        /*----- 横向扫描 -----*/
        for (int col = col_start + 1; col < col_end; col++)
        {
            /* 连续黑像素统计 */
            if (IMG_DATA[row][col] == BLACK_IMG)
                continuous_black++;
            else
            {
                if (continuous_black >= ZEBRA_THRESHOLD)
                    trans_cnt++;                // 完成一段黑条
                continuous_black = 0;
            }

            /* 跳变沿统计 */
            if (IMG_DATA[row][col] != IMG_DATA[row][col - 1])
                trans_cnt++;
        }
        /* 行尾再检查一次 */
        if (continuous_black >= ZEBRA_THRESHOLD) trans_cnt++;

        /* 认为本行是斑马线条纹：跳变次数合理且存在合格黑条 */
        if (trans_cnt >= 5 && trans_cnt <= 25)
            zebra_row++;
    }
		
		    // 2. 新增：防抖判定（连续N帧识别到才算“真识别到”）
    bool current_frame_detected = (zebra_row >= ZEBRA_ROW_COUNT);
    if (current_frame_detected) {
        zebra_detected_continuous++;  // 连续识别到的帧数+1
        zebra_not_detected_frames = 0; // 重置离开帧数
    } else {
        zebra_detected_continuous = 0; // 重置连续识别帧数
        zebra_not_detected_frames++;   // 连续未识别帧数+1
    }

    // 3. 核心：仅“从非斑马线区进入斑马线区”时，计数+1
    bool is_enter_zebra = (zebra_detected_continuous >= ZEBRA_ENTER_FRAMES) && !in_zebra_zone;
    bool is_leave_zebra = (zebra_not_detected_frames >= ZEBRA_LEAVE_FRAMES) && in_zebra_zone;

    if (is_enter_zebra) {
        // 进入新的斑马线区 → 计数+1
        zebra_detect_count++;
        in_zebra_zone = true; // 标记进入斑马线区

    }
    if (is_leave_zebra) {
        // 离开斑马线区 → 标记退出
        in_zebra_zone = false;
        zebra_not_detected_frames = 0; // 重置离开帧数
    }

    // 4. 仅第二次进入斑马线区时，触发停车
    if (zebra_detect_count >= 2 && in_zebra_zone) {
        zebra_state = ZEBRA_DETECTED;
    } else {
        // 未到第二次，保持正常行驶（避免误触发）
        if (zebra_state != ZEBRA_STOPPED) {
            zebra_state = ZEBRA_NONE;
        }
    }
		
		//		// 在 Zebra_Detect() 中检测完成后添加
//tft180_show_string(0, 80, "Zebra Row:");
tft180_show_int(0, 100, zebra_row, 2);  // 显示满足条件的行数
//tft180_show_string(60, 80, "State:");
tft180_show_int(60, 100, zebra_state, 1);  // 显示当前状
tft180_show_int(0, 125, in_zebra_zone, 1); // 显示是否在斑马线区
tft180_show_int(100, 100, zebra_detect_count, 1);  // 显示识别次数
}

/*============ 125×100 屏幕右上角显示最长连续黑像素长度 ==============*/
static void Draw_Max_Black_Streak(void)
{
    int max_streak = 0;
    for (int row = 50; row < 80; row++){
        int streak = 0;
        for (int col = 0; col < COL; col++){
            if (IMG_DATA[row][col] == BLACK_IMG)
                streak++;
            else{
                if (streak > max_streak) max_streak = streak;
                streak = 0;
            }
        }
        if (streak > max_streak) max_streak = streak;
    }
    /* 右上角可见区域：x 最大 124-16=108，y 0 */
    tft180_show_string(0, 10, "B:");
    tft180_show_int(0, 60, max_streak, 2);   // 3 位整数，占 9 像素，不越界
}









//***************************************************************
//* º¯ÊýÃû³Æ£º Img_Deal
//* ¹¦ÄÜËµÃ÷£º Í¼Ïñ´¦ÀíÖ÷º¯Êý
//* º¯Êý·µ»Ø£º 
//* ±¸ ×¢£º    
//***************************************************************
void Img_Deal()
{ 
    Deal_Init();
    InitData();
    deal_flag=1;
    CorssCol=Cross_col();
    if(CorssCol<0.1) CorssCol=0.1;
    //protect();
	LeftStartFind() ;
	RightStartFind();
	fixpoint();
	left_jump();
	right_jump();
    Left_Apex();
    Right_Apex();
    //Garage_Deal();
    //Fork_Deal();
    //Ramp_deal();
    //T_Conner_Deal();
    Roundabout_Deal();
	

    Zebra_Detect();            // 检测斑马线
	
    RaceLine();
	 

   // track();
    //if(show_img) lcd_displayimage032(*IMG_DATA,188,120);
		//tft180_show_int(80, 30, left_apex.Apex_Col, 4);
//   //tft180_show_int(80, 40, right_apex.Apex_Col, 4);
//    tft180_show_int  (0,  10, Roundabout_flag,          2); 
//		tft180_show_float(0,  50, CrossRow_L(75), 4, 2);
//    tft180_show_float(0,  60, CrossRow_R(75), 4, 2);
//		tft180_show_float(0, 20, L_S, 4, 2);
    deal_flag=0;
}

void Img_Deal1()
{Deal_Init();
	 Zebra_Detect();
	 Draw_Max_Black_Streak();   // <-- 新增
   
}