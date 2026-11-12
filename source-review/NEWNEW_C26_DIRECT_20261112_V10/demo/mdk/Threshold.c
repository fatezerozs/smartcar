#include "Threshold.h"
#include "IMG_DEAL.h"
struct STR   //¶¨ÒåµÄ½á¹¹Ìå£¬ÓÃÓÚ´æ·Å±ß½çÐÅÏ¢
{
  uint8 cam_i;   //´æ·Å±ß½çµÄÐÐ
  uint8 cam_j;   //´æ·Å±ß½çµÄÁÐ
};

uint8 cam_static=90;//µ±ÈüµÀ³ö½ç£¬»òÕß¼ÆËãµÄ»Ò¶ÈãÐÖµÒì³£Ê±Ê¹ÓÃ
uint8 Threshold0=90;
uint8 cam_thr_mid=0,cam_thr_max=0,cam_thr_min=0;
uint8 cam_thr=90,cam_thr0=100;//ÉãÏñÍ·µÄãÐÖµ
uint8_t HistoGram[256]; //ÔÚ½øÐÐ´ó½òµÄ¼ÆËããÐÖµÊ±ÓÃÓÚÍ³¼ÆÈüµÀÖ¸¶¨ÇøÓòµÄ¸÷¸öãÐÖµµÄ¸öÊýÊý×é
int LQ_ary[256];

uint8 cam_strong_light=0;//Ç¿¹â±êÖ¾Î»
uint8 cam_normal_light=0;//Õý³£¹â±êÖ¾Î»
uint8 cam_wesk_light=0;  //Èõ¹â±êÖ¾Î»
int C_W= COL;


//***************************************************************
//* º¯ÊýÃû³Æ£ºuint8_t GetOSTU(uint8_t tmImage[IMAGEH][IMAGEW])
//* ¹¦ÄÜËµÃ÷£ºÇóãÐÖµ´óÐ¡
//* ²ÎÊýËµÃ÷£ºÓÐÈý¸öÊäÈë±äÁ¿£¬µÚÒ»¸öÊÇÖ¸ÕëP£¬Ö¸Ïò´ý´¦ÀíµÄÍ¼ÏñµÄµÚÒ»¸öÎ»ÖÃ£¬Ö¸ÕëÎªÈ«¾Ö±äÁ¿£¬
//*           »¹ÓÐÁ½¸öºê¶¨ÒåLQOSTU_MIN02£¬LQOSTU_MAX02 £¬ËûÃÇ·Ö±ð±íÊ¾Òª¼ÆËããÐÖµµÄÇøÓòµÄ´óÐ¡
//* º¯Êý·µ»Ø£ºãÐÖµ´óÐ¡ cam_thr
//* ÐÞ¸ÄÊ±¼ä£º2018Äê3ÔÂ27ÈÕ
//*           µÚÒ»´ÎÐÞ¸Ä
//* ±¸ ×¢£º
//Ostu·½·¨ÓÖÃû×î´óÀà¼ä²î·½·¨£¬Í¨¹ýÍ³¼ÆÕû¸öÍ¼ÏñµÄÖ±·½Í¼ÌØÐÔÀ´ÊµÏÖÈ«¾ÖãÐÖµTµÄ×Ô¶¯Ñ¡È¡£¬ÆäËã·¨²½ÖèÎª£º
//1) ÏÈ¼ÆËãÍ¼ÏñµÄÖ±·½Í¼£¬¼´½«Í¼ÏñËùÓÐµÄÏñËØµã°´ÕÕ0~255¹²256¸öbin£¬Í³¼ÆÂäÔÚÃ¿¸öbinµÄÏñËØµãÊýÁ¿
//2) ¹éÒ»»¯Ö±·½Í¼£¬Ò²¼´½«Ã¿¸öbinÖÐÏñËØµãÊýÁ¿³ýÒÔ×ÜµÄÏñËØµã
//3) i±íÊ¾·ÖÀàµÄãÐÖµ£¬Ò²¼´Ò»¸ö»Ò¶È¼¶£¬´Ó0¿ªÊ¼µü´ú
//4) Í¨¹ý¹éÒ»»¯µÄÖ±·½Í¼£¬Í³¼Æ0~i »Ò¶È¼¶µÄÏñËØ(¼ÙÉèÏñËØÖµÔÚ´Ë·¶Î§µÄÏñËØ½Ð×öÇ°¾°ÏñËØ) ËùÕ¼Õû·ùÍ¼ÏñµÄ±ÈÀýw0£¬²¢Í³¼ÆÇ°¾°ÏñËØµÄÆ½¾ù»Ò¶Èu0£»Í³¼Æi~255»Ò¶È¼¶µÄÏñËØ(¼ÙÉèÏñËØÖµÔÚ´Ë·¶Î§µÄÏñËØ½Ð×ö±³¾°ÏñËØ) ËùÕ¼Õû·ùÍ¼ÏñµÄ±ÈÀýw1£¬²¢Í³¼Æ±³¾°ÏñËØµÄÆ½¾ù»Ò¶Èu1£»
//5) ¼ÆËãÇ°¾°ÏñËØºÍ±³¾°ÏñËØµÄ·½²î g = w0*w1*(u0-u1) (u0-u1)
//6) i++£»×ªµ½4)£¬Ö±µ½iÎª256Ê±½áÊøµü´ú
//7£©½«×î´ógÏàÓ¦µÄiÖµ×÷ÎªÍ¼ÏñµÄÈ«¾ÖãÐÖµ
//È±ÏÝ:OSTUËã·¨ÔÚ´¦Àí¹âÕÕ²»¾ùÔÈµÄÍ¼ÏñµÄÊ±ºò£¬Ð§¹û»áÃ÷ÏÔ²»ºÃ£¬ÒòÎªÀûÓÃµÄÊÇÈ«¾ÖÏñËØÐÅÏ¢¡£
//******************************************************************************
// ???? IMG_DATA[ROW][COL] ?????
// LQOSTU_MIN / LQOSTU_MAX ??????????(?)
uint8_t zzkLQGetOSTU0200(uint8 LQOSTU_MIN, uint8 LQOSTU_MAX)
{
    // 1. ??:?????,????
    if (LQOSTU_MIN >= ROW)  LQOSTU_MIN = 0;
    if (LQOSTU_MAX >= ROW)  LQOSTU_MAX = ROW - 1;
    if (LQOSTU_MIN >  LQOSTU_MAX) {
        uint8 tmp      = LQOSTU_MIN;
        LQOSTU_MIN     = LQOSTU_MAX;
        LQOSTU_MAX     = tmp;
    }

    // 2. ?????
    for (int i = 0; i < 256; i++) {
        HistoGram[i] = 0;
    }

    // 3. ? [LQOSTU_MIN, LQOSTU_MAX] × [0, COL) ???????
    uint32_t Amount = 0;
    for (int r = LQOSTU_MIN; r <= LQOSTU_MAX; r++) {
        for (int c = 0; c < COL; c++) {
            uint8_t v = IMG_DATA[r][c];
            HistoGram[v]++;
            Amount++;
        }
    }
    if (Amount == 0) {
        // ???????,????
        return Threshold0;   // ?????
    }

    // 4. ?????? / ????
    int16_t MinValue, MaxValue;
    for (MinValue = 0; MinValue < 256 && HistoGram[MinValue] == 0; MinValue++);
    for (MaxValue = 255; MaxValue > MinValue && HistoGram[MaxValue] == 0; MaxValue--);

    if (MaxValue <= MinValue) {
        Threshold0 = MinValue;
        return (uint8_t)MinValue;
    }

    // 5. ???????
    uint32_t PixelIntegral = 0;
    for (int i = MinValue; i <= MaxValue; i++) {
        PixelIntegral += (uint32_t)HistoGram[i] * i;
    }

    // 6. OTSU ???
    uint32_t PixelBack = 0;
    uint32_t PixelIntegralBack = 0;
    double   SigmaB = -1.0;
    uint8_t  Threshold = MinValue;

    for (int t = MinValue; t < MaxValue; t++) {
        PixelBack         += HistoGram[t];
        if (PixelBack == 0) continue;

        uint32_t PixelFore = Amount - PixelBack;
        if (PixelFore == 0) break;

        PixelIntegralBack += (uint32_t)HistoGram[t] * t;
        uint32_t PixelIntegralFore = PixelIntegral - PixelIntegralBack;

        double w0 = (double)PixelBack  / Amount;
        double w1 = (double)PixelFore  / Amount;
        double u0 = (double)PixelIntegralBack / PixelBack;
        double u1 = (double)PixelIntegralFore / PixelFore;
        double sigma = w0 * w1 * (u0 - u1) * (u0 - u1);

        if (sigma > SigmaB) {
            SigmaB   = sigma;
            Threshold = (uint8_t)t;
        }
    }

    // 7. ??????,?????????
    if (Threshold0 == 0) {
        Threshold0 = Threshold;
    } else {
        // ???????? ±20
        if (Threshold > Threshold0 + 20) Threshold = Threshold0 + 20;
        if (Threshold + 20 < Threshold0) Threshold = Threshold0 - 20;
        Threshold0 = Threshold;
    }

    return Threshold;
}


//***************************************************************
//* º¯ÊýÃû³Æ£ºuint8_t zzkLQGetOSTU0200_col(uint8_t tmImage[IMAGEH][IMAGEW])
//* ¹¦ÄÜËµÃ÷£º°´ÁÐÇóãÐÖµ´óÐ¡
//******************************************************************************
uint8_t zzkLQGetOSTU0200_col(uint8 LQOSTU_MIN,uint8 LQOSTU_MAX,uint8 lie_min,uint8 lie_max)
{
  int16 i,j;
  uint32 Amount = 0;
  uint32 PixelBack = 0;
  uint32 PixelIntegralBack = 0;
  uint32 PixelIntegral = 0;
  int32 PixelIntegralFore = 0;
  int32 PixelFore = 0;
  double OmegaBack, OmegaFore, MicroBack, MicroFore, SigmaB, Sigma; // Àà¼ä·½²î;
  int16 MinValue, MaxValue;
  uint8 Threshold = 0;             //
  uint8 *tmImagep;
  uint8 *imgp;
  imgp=(uint8 *)IMG_DATA;
  tmImagep=imgp+C_W*LQOSTU_MIN+lie_min;
  //int tmImage_size=(lie_max-lie_min+1)*(LQOSTU_MAX-LQOSTU_MIN+1)/3-(lie_max-lie_min+1)*(LQOSTU_MAX-LQOSTU_MIN+1)%3;
  for (j = 0; j < 256; j++)
  {
    HistoGram[j] = 0; //³õÊ¼»¯»Ò¶ÈÖ±·½Í¼
    LQ_ary[j]=0;
  }
  // while(tmImage_size)
  // {
    // HistoGram[*tmImagep]++; //Í³¼Æ»Ò¶È¼¶ÖÐÃ¿¸öÏñËØÔÚÕû·ùÍ¼ÏñÖÐµÄ¸öÊý
    // tmImagep=tmImagep+3;
    // if(tmImagep%C_W>=lie_max)
        // while(tmImagep%C_W<lie_min)
            // tmImagep=tmImagep+3;
    // tmImage_size--;
  // }
  for(i=LQOSTU_MIN;i<LQOSTU_MAX;i++)
  {
      for(j=lie_min;j<lie_max;j+=3)
      {
         // tmImagep=img[i][j];
          HistoGram[IMG_DATA[i][j]]++;
      }
  }

  for (MinValue = 0; MinValue < 256 && HistoGram[MinValue] == 0; MinValue++) ;        //»ñÈ¡×îÐ¡»Ò¶ÈµÄÖµ
  for (MaxValue = 255; MaxValue > MinValue && HistoGram[MinValue] == 0; MaxValue--) ; //»ñÈ¡×î´ó»Ò¶ÈµÄÖµ

  if (MaxValue == MinValue)
    return MaxValue;         // Í¼ÏñÖÐÖ»ÓÐÒ»¸öÑÕÉ«
  if (MinValue + 1 == MaxValue)
    return MinValue;        // Í¼ÏñÖÐÖ»ÓÐ¶þ¸öÑÕÉ«

  for (j = MinValue; j <= MaxValue; j++)
    Amount += HistoGram[j];        //  ÏñËØ×ÜÊý  256

  PixelIntegral = 0;
  for (j = MinValue; j <= MaxValue; j++)
  {
    PixelIntegral += HistoGram[j] * j;//»Ò¶ÈÖµ×ÜÊý  256
  }
  SigmaB = -1;
  //  str.Format("%d",MinValue);AfxMessageBox(str);
  for (j = MinValue; j < MaxValue; j++)
  {
    PixelBack = PixelBack + HistoGram[j];    //Ç°¾°ÏñËØµãÊý
    PixelFore = Amount - PixelBack;         //±³¾°ÏñËØµãÊý
    OmegaBack = (double)PixelBack / Amount;//Ç°¾°ÏñËØ°Ù·Ö±È
    OmegaFore = (double)PixelFore / Amount;//±³¾°ÏñËØ°Ù·Ö±È
    PixelIntegralBack += HistoGram[j] * j;  //Ç°¾°»Ò¶ÈÖµ
    PixelIntegralFore = PixelIntegral - PixelIntegralBack;//±³¾°»Ò¶ÈÖµ
    MicroBack = (double)PixelIntegralBack / PixelBack;   //Ç°¾°»Ò¶È°Ù·Ö±È
    MicroFore = (double)PixelIntegralFore / PixelFore;   //±³¾°»Ò¶È°Ù·Ö±È
    Sigma = OmegaBack * OmegaFore * (MicroBack - MicroFore) * (MicroBack - MicroFore);//¼ÆËãÀà¼ä·½²î
    if (Sigma > SigmaB)                    //±éÀú×î´óµÄÀà¼ä·½²îg //ÕÒ³ö×î´óÀà¼ä·½²îÒÔ¼°¶ÔÓ¦µÄãÐÖµ
    {
      SigmaB = Sigma;
      Threshold = j;
    }
  }
  //=================================================================
  Threshold0=Threshold;
  return Threshold;                        //·µ»Ø×î¼ÑãÐÖµ;
}
