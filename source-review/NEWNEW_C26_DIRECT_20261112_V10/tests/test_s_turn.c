#include "target_from_v3.h"
#include <stdio.h>
#include <math.h>
static unsigned failures;
#define CHECK(c,msg) do{if(!(c)){failures++;printf("S_TURN_FAIL: %s\n",msg);}}while(0)
int main(void){
CHECK(fabsf(mapped_target(5)-1562.2222f)<.01f,"small-error V3 gain unchanged");
CHECK(mapped_target(.5f)==1550,"straight deadband retained");
CHECK(mapped_target(90)==1670&&mapped_target(-90)==1430,"physical endpoints unchanged");

for(int side=-1;side<=1;side+=2){servo_smoother_t s={1550,1550};float target=mapped_target(side*25.0f);
 CHECK(side*(target-1550)>=80,"25px bend requests at least 80us correction within calibrated endpoints");
 for(int i=0;i<10;i++)servo_smooth_step(&s,target,1);
 printf("25px side=%d target=%.1f after100ms=%.1f\n",side,(double)target,(double)s.command_us);
 CHECK(side*(s.command_us-1550)>=75,"25px bend reaches useful authority within100ms");
 for(int i=0;i<30;i++)servo_smooth_step(&s,side>0?1670.0f:1430.0f,1);
 for(int i=0;i<10;i++)servo_smooth_step(&s,side>0?1430.0f:1670.0f,1);
 printf("reverse side=%d after100ms=%.1f\n",side,(double)s.command_us);
 CHECK(side*(s.command_us-1550)<=-108,"S reversal reaches90percent of opposite endpoint within100ms");
 CHECK(servo_smooth_step(&s,1430,0)==1550,"OFF immediately centers and clears memory");
}
printf("S_TURN_RESULT failures=%u; software authority/timing regression, not physical track simulation\n",failures);return failures?1:0;}
