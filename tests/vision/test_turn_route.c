/* Actual grayscale vision -> route control -> PWM, simulated hardware. */
#define SWITCH_TEST_FUNCTION legacy_switch_main
#include "test_route_switch.c"
#include "vision_drive_internal.h"
static uint8_t turn_gray[VISION_WIDTH*VISION_HEIGHT],turn_binary[VISION_WIDTH*VISION_HEIGHT];
static int turn_shift;
static void gray_road(int bend)
{
    for(int y=0;y<VISION_HEIGHT;++y){
        int center=94+turn_shift+bend*(110-y)/2,width=40+y*3/4;
        for(int x=0;x<VISION_WIDTH;++x)
            turn_gray[y*VISION_WIDTH+x]=(uint8_t)(x>=center-width/2&&x<=center+width/2?210:30);
    }
}
static void run_gray(int bend)
{
    switch_boot(0);vision_midterm_reset();vision_midterm_route_enable(1);turn_shift=0;gray_road(bend);
    for(unsigned i=0;i<120;++i){
        vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
        CHECK(vision_drive_servo_command_us()==0||
            (vision_drive_servo_command_us()>=1430&&vision_drive_servo_command_us()<=1670));
    }
    CHECK(vision_drive_is_local()&&vision_drive_output()->observation_valid);
}
static void measured_curve(float far_error,int gaps)
{
    vd_profile_t p;vision_drive_get_profile(&p);set_lane(0);route_lane.confidence=30;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        float yf=(float)y/(VISION_HEIGHT-1);
        float t=(p.control.near_y-yf)/(p.control.near_y-p.control.far_y);
        if(t<0)t=0;if(t>1)t=1;
        int center=(int)(93.0f+far_error*t*40.0f+0.5f);
        route_lane.left[y]=(uint8_t)(center-40);route_lane.right[y]=(uint8_t)(center+40);
        route_lane.center[y]=(uint8_t)center;
        if(gaps&&yf>p.control.far_y-.036f&&yf<p.control.far_y+.036f)route_lane.row_flags[y]=0;
    }
}
static void crossed_center(int direction)
{
    vd_profile_t p;vision_drive_get_profile(&p);set_lane(0);
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        float yf=(float)y/(VISION_HEIGHT-1);
        float t=(p.control.near_y-yf)/(p.control.near_y-p.control.far_y);
        if(t<0)t=0;if(t>1)t=1;
        /* Near center now asks for correction opposite the upcoming bend. */
        int center=(int)(93.0f+direction*(-.4f+.8f*t)*40.0f+.5f);
        route_lane.left[y]=(uint8_t)(center-40);route_lane.right[y]=(uint8_t)(center+40);
        route_lane.center[y]=(uint8_t)center;
    }
}
static void successive_bends(int direction)
{
    set_lane(0);
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
        float offset=y>=90?0.0f:(y>=70?10.0f:(y>=55?-32.0f+(y-55)*42.0f/15.0f:-32.0f));
        int center=(int)(93.0f+direction*offset+.5f);
        route_lane.left[y]=(uint8_t)(center-40);route_lane.right[y]=(uint8_t)(center+40);
        route_lane.center[y]=(uint8_t)center;
    }
}
static void partial_shift(int shift)
{
    set_lane(shift);route_lane.valid=0;route_lane.status=VISION_NO_LANE;route_lane.confidence=11;
    for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)if(y<70||y>79)route_lane.row_flags[y]=0;
}
static void gray_crossing(int top,int bottom,int shift)
{
    turn_shift=shift;gray_road(0);
    for(int y=top;y<=bottom;++y)memset(turn_gray+y*VISION_WIDTH,210,VISION_WIDTH);
}
static void gray_side_opening(int side)
{
    turn_shift=0;gray_road(0);
    for(int y=40;y<=100;++y)
        for(int x=side<0?0:94;x<=(side<0?94:VISION_WIDTH-1);++x)
            turn_gray[y*VISION_WIDTH+x]=210;
}
int main(void)
{
    /* Recorded post-junction failure: a short one-edge strip centered near
     * the optical axis unwound a useful bend and hit the 300ms deadline.
     * Continue on CURRENT edge evidence, without extending a blind search. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(direction);uint16_t widths[VISION_HEIGHT];
        memcpy(widths,route_lane.row_width,sizeof(widths));
        memset(turn_gray,30,sizeof(turn_gray));
        for(int y=70;y<=85;++y){int left=93-(widths[y]-1)/2;
            for(int x=left;x<VISION_WIDTH;++x)turn_gray[y*VISION_WIDTH+x]=210;}
        for(unsigned i=0;i<90;++i){
            vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            CHECK(!route_lane.valid&&!route_lane.measured_rows&&route_lane.inferred_rows>=8);
            CHECK(vision_drive_is_local());if(i)CHECK(vision_drive_output()->recovering);
            CHECK(direction*vision_drive_output()->steering>.25f);
        }
        CHECK(vision_drive_output()->search_source==3);
        turn_shift=0;gray_road(direction);
        for(unsigned i=0;i<30;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
        CHECK(vision_drive_is_local()&&vision_drive_output()->observation_valid&&!vision_drive_output()->recovering);
        memset(turn_gray,30,sizeof(turn_gray));
        for(int y=70;y<=85;++y){int left=93-(widths[y]-1)/2;
            for(int x=left;x<VISION_WIDTH;++x)turn_gray[y*VISION_WIDTH+x]=210;}
        for(unsigned i=0;i<145;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
        CHECK(!vision_drive_is_local()&&!pwm[0]&&!pwm[1]); /* Still bounded at 1.2s. */
    }
    run_gray(0);CHECK(vision_drive_output()->duty>.16f&&vision_drive_output()->duty<=.18f);
    CHECK(info().target_right>=44&&info().target_right<=50&&info().pwm_right<=180);
    turn_shift=0;gray_road(1);vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(2,1);
    CHECK(vision_drive_output()->duty<.145f); /* Bend revokes straight boost now. */
    gray_crossing(58,110,0);vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(2,1);
    CHECK(vision_drive_output()->duty<=.066f); /* No fast intersection entry. */
    /* Real crossing failure retained no paired edges: use a learned width
     * with eight CURRENT one-edge rows plus a wide white forward opening.
     * Entry-only -> exit-only must stay running, beyond the 300ms search. */
    run_gray(0);gray_road(0);
    for(int y=0;y<VISION_HEIGHT;++y){
        if(y<=88)memset(turn_gray+y*VISION_WIDTH,210,VISION_WIDTH);
        else for(int x=0;x<=94;++x)turn_gray[y*VISION_WIDTH+x]=210;
    }
    for(unsigned i=0;i<80;++i){
        vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
        CHECK(route_lane.valid&&route_lane.junction_rows>=12&&route_lane.junction_anchors>=8);
        CHECK(!route_lane.measured_rows&&vision_drive_is_local()&&!vision_drive_output()->recovering);
    }
    CHECK(vision_drive_output()->duty<=.066f);
    memset(turn_gray,210,sizeof(turn_gray));
    for(int y=VISION_ROI_TOP;y<60;++y){
        int right=94+(40+y*3/4)/2;
        for(int x=right+1;x<VISION_WIDTH;++x)turn_gray[y*VISION_WIDTH+x]=30;
    }
    for(unsigned i=0;i<80;++i){
        vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
        CHECK(route_lane.valid&&route_lane.junction_rows>=12&&route_lane.junction_anchors>=8);
        CHECK(!route_lane.measured_rows&&vision_drive_is_local()&&!vision_drive_output()->recovering);
    }
    /* A near lateral correction must win over an opposite distant bend once
     * displaced, while a centered approach still previews either direction. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);crossed_center(direction);switch_wait(100,1);
        CHECK(vision_drive_is_local()&&direction*vision_drive_output()->lateral<-.25f);
        CHECK(direction*vision_drive_output()->steering<-.20f);
        CHECK(vision_drive_output()->duty<.080f); /* Finish near correction first. */
    }
    /* A one-sided opening alone does not establish a roundabout. Keep the
     * visible road edge until an island and connected branches are present. */
    for(int side=-1;side<=1;side+=2){
        run_gray(0);gray_side_opening(side);
        for(unsigned i=0;i<100;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
        CHECK(route_lane.valid&&!route_lane.junction_rows&&route_lane.inferred_rows>=40);
        CHECK(vision_drive_is_local()&&vision_drive_output()->steering>-.1f&&vision_drive_output()->steering<.1f);
    }
    /* The same current-frame junction can be crossed for longer than the
     * 300ms missing-lane search, with visible entry+exit and then exit only.
     * Mark virtual junction rows honestly, never as both measured. */
    for(int shift=-8;shift<=8;shift+=8){
        run_gray(0);gray_crossing(58,95,shift);
        vision_midterm_process(turn_gray,turn_binary,&route_lane);
        CHECK(route_lane.valid&&route_lane.junction_rows>=30&&route_lane.measured_rows>=12);
        CHECK(route_lane.row_flags[90]==4);
        for(unsigned i=0;i<80;++i){
            vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            CHECK(vision_drive_is_local()&&!vision_drive_output()->recovering);
        }
        CHECK(vision_drive_output()->duty<=.066f);
        CHECK(vision_drive_output()->steering>-.5f&&vision_drive_output()->steering<.5f);
        for(int top=58;top<=94;top+=6){
            gray_crossing(top,110,shift);
            for(unsigned i=0;i<40;++i){
                vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
                CHECK(route_lane.valid&&route_lane.junction_rows>=4&&route_lane.measured_rows>=12);
                CHECK(vision_drive_is_local()&&!vision_drive_output()->recovering);
                CHECK(vision_drive_output()->duty<=.066f);
            }
        }
        turn_shift=0;gray_road(0);
        for(unsigned i=0;i<100;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
        CHECK(!route_lane.junction_rows&&vision_drive_is_local()&&vision_drive_output()->duty>.10f);
    }
    run_gray(0);gray_crossing(58,95,0);
    memset(turn_gray+75*VISION_WIDTH,30,VISION_WIDTH); /* Dark gap, not a white crossing. */
    vision_midterm_process(turn_gray,turn_binary,&route_lane);CHECK(!route_lane.junction_rows);
    run_gray(0);gray_crossing(58,110,0);
    for(int y=VISION_ROI_TOP;y<58;++y)memset(turn_gray+y*VISION_WIDTH,30,VISION_WIDTH);
    vision_midterm_process(turn_gray,turn_binary,&route_lane);CHECK(!route_lane.junction_rows&&!route_lane.valid);
    vision_midterm_reset();gray_crossing(58,110,0);
    vision_midterm_process(turn_gray,turn_binary,&route_lane);CHECK(!route_lane.junction_rows);
    /* Continuous A->B->right-hand main course->A scenes, repeated without a
     * reboot, OFF cycle or re-arm at A/B. Cross straight twice around the
     * central 270-degree bend; keep main line beside island and top S bends.
     * This checks code continuity, not unmeasured real vehicle dynamics. */
    {const int lap[]={0,-1,-1,1,0,1,1,-1,-1,0,-1,0,
                     -1,2,-1,-1,-1,2,1,-1,-1,3,-1,-1,1,1,-1,-1,1,0};
     vd_trace_info_t first,last;unsigned starts=0;
     switch_boot(0);vision_midterm_reset();vision_midterm_route_enable(1);turn_shift=0;gray_road(0);
     for(unsigned i=0;i<100;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
     vision_drive_trace_info(&first);CHECK(vision_drive_is_local());
     for(unsigned round=0;round<3;++round){
        for(unsigned scene=0;scene<sizeof(lap)/sizeof(lap[0]);++scene){
            int direction=lap[scene];
            if(direction==2)gray_crossing(62,110,0);
            else if(direction==3)gray_side_opening(-1);
            else {turn_shift=0;gray_road(direction);}
            for(unsigned i=0;i<160;++i){
                vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
                CHECK(vision_drive_is_local()&&!vision_drive_output()->faults);
                CHECK(info().target_right>0&&info().target_left>0);
                CHECK(vision_drive_servo_command_us()>=1430&&vision_drive_servo_command_us()<=1670);
            }
            if(direction==2)CHECK(route_lane.junction_rows>=4);
            if(direction==3)CHECK(!route_lane.junction_rows&&vision_drive_output()->steering>-.1f&&vision_drive_output()->steering<.1f);
        }
        ++starts;
     }
     vision_drive_trace_info(&last);CHECK(starts==3&&now>120000U&&last.run==first.run&&last.active&&!last.frozen);
     switch_a=1;switch_wait(1,1);CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_servo_command_us());}
    /* A zero total-run limit is intentional only for C26. Frame freshness and
     * a finite configured duration still operate, including clock wraparound. */
    {vd_profile_t profile;vc_controller_t c;vision_drive_get_profile(&profile);
     CHECK(!profile.control.max_run_ms&&vc_config_valid(&profile.control));
     vc_init(&c,&profile.control);set_lane(0);
     uint32_t at=0xffffff00U;
     for(unsigned i=1;i<=40;++i){at+=20;vc_observe(&c,&route_lane,i,at,at);
        if(i==6)CHECK(vc_arm_local(&c,at,1));
        if(i>=6)CHECK(c.output.state==VC_RUNNING&&!c.output.faults);}
     vc_poll(&c,at+profile.control.frame_timeout_ms);CHECK(c.output.faults&VC_FAULT_FRAME);
     profile.control.max_run_ms=200;vc_init(&c,&profile.control);at=0;
     for(unsigned i=1;i<=20;++i){at+=20;vc_observe(&c,&route_lane,i,at,at);if(i==6)CHECK(vc_arm_local(&c,at,1));}
     CHECK(c.output.faults&VC_FAULT_RUNTIME);}
    /* Centered bends get the C26_18 increase, symmetrically. Low coverage,
     * modeled-only rows and missing preview do not get the extra clear-lane
     * boost; displaced lanes retain the low floor. Controller -> wheel PI/PWM. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);measured_curve(direction*.6f,0);
        route_lane.confidence=85;route_lane.measured_rows=87;switch_wait(140,1);
        CHECK(vision_drive_is_local()&&vision_drive_output()->duty>.103f&&vision_drive_output()->duty<.113f);
        CHECK(info().target_right>=18&&info().target_left>=18);
        CHECK(info().target_right>3&&info().target_left>3);
        CHECK(direction>0?info().target_right<info().target_left:info().target_left<info().target_right);
        route_lane.confidence=30;switch_wait(60,1);
        CHECK(vision_drive_output()->duty>=.073f&&vision_drive_output()->duty<.077f);
        route_lane.confidence=85;route_lane.measured_rows=0;switch_wait(60,1);
        CHECK(vision_drive_output()->duty>=.073f&&vision_drive_output()->duty<.077f);
        route_lane.measured_rows=87;
        for(int y=VISION_ROI_TOP;y<85;++y)route_lane.row_flags[y]=0;
        switch_wait(60,1);CHECK(!vision_drive_output()->far_valid);
        CHECK(vision_drive_output()->duty>=.073f&&vision_drive_output()->duty<.077f);
        set_lane(direction*45);route_lane.measured_rows=87;switch_wait(80,1);
        CHECK(direction*vision_drive_output()->lateral>1.0f);
        CHECK(vision_drive_output()->duty>=.065f&&vision_drive_output()->duty<.066f);
        /* Enter a tight curve directly from cruise: its requested steering
         * must cap speed before the rate-limited servo has reached the bend. */
        set_lane(0);route_lane.confidence=100;switch_wait(180,1);
        CHECK(vision_drive_output()->duty>.13f);
        measured_curve(direction*1.0f,0);route_lane.confidence=100;route_lane.measured_rows=87;
        switch_wait(2,1);CHECK(vision_drive_output()->steering>-.10f&&vision_drive_output()->steering<.10f);
        CHECK(vision_drive_output()->duty<=.091f);
        switch_a=1;switch_wait(1,1);CHECK(!pwm[0]&&!pwm[1]);
    }
    /* Actual PWM contract for the progressive larger range: monotonic and
     * symmetric, small-center response nearly unchanged, both endpoints used. */
    run_gray(0);
    {uint16_t previous=2000;
     for(int k=-100;k<=100;++k){
        vd.control.output.steering=k*.01f;vd_output_apply();
        uint16_t pulse=vision_drive_servo_command_us();
        CHECK(pulse>=1430&&pulse<=1670&&pulse<=previous);previous=pulse;
        if(k==0)CHECK(pulse==1550);
        if(k==10)CHECK(pulse>=1540&&pulse<=1544);
        if(k==-10)CHECK(pulse>=1556&&pulse<=1560);
        if(k==50)CHECK(pulse>=1502&&pulse<=1506);
        if(k==-50)CHECK(pulse>=1592&&pulse<=1596);
        if(k==100)CHECK(pulse==1430);
        if(k==-100)CHECK(pulse==1670);
     }
     switch_a=1;switch_wait(1,1);CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_servo_command_us());}
    /* Inner wheel speed request follows steering direction without raising
     * outer speed, straight requests remain balanced, OFF stops both. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(direction);vd_speed_info_t bend=info();
        CHECK(direction>0?bend.target_right<bend.target_left:bend.target_left<bend.target_right);
        CHECK(bend.target_right>3&&bend.target_left>3&&bend.target_right<=50&&bend.target_left<=50);
        vd_trace_info_t before,after;vd_trace_sample_t sample;
        vision_drive_trace_info(&before);CHECK(before.active&&!before.frozen&&before.count>10);
        CHECK(!vision_drive_trace_read(0,&sample));
        switch_a=1;switch_wait(5,1);vision_drive_trace_info(&after);
        CHECK(!pwm[0]&&!pwm[1]&&!after.active&&after.frozen&&after.run==before.run);
        CHECK(vision_drive_trace_read(0,&sample)&&sample.frame>0);
        CHECK(!vision_drive_trace_read(after.count,&sample));
        int saw_bend=0;
        for(unsigned i=0;i<after.count;++i){CHECK(vision_drive_trace_read(i,&sample));
            if(direction*sample.steer>400&&sample.target_right>0&&sample.target_left>0){
                CHECK(direction>0?sample.target_right<sample.target_left:sample.target_left<sample.target_right);saw_bend=1;}}
        CHECK(saw_bend);
    }
    run_gray(0);CHECK(info().target_right-info().target_left>=-1&&info().target_right-info().target_left<=1);
    {vd_trace_info_t before,loss,recovered;
     vision_drive_trace_info(&before);route_lane.valid=0;switch_wait(40,1);
     vision_drive_trace_info(&loss);CHECK(loss.frozen&&!loss.active);
     set_lane(0);switch_wait(30,1);vision_drive_trace_info(&recovered);
     CHECK(recovered.active&&recovered.run==before.run&&recovered.count>before.count&&recovered.revision==loss.revision);}
    /* Retain the latest ~60 seconds of a longer continuous run. Memory stays
     * bounded; crossing the old two-minute deadline must not stop a lap. */
    switch_wait(12100,1);
    CHECK(vision_drive_is_local()&&pwm[0]&&pwm[1]);
    switch_a=1;switch_wait(1,1);
    {vd_trace_info_t trace;vd_trace_sample_t first,last;
     vision_drive_trace_info(&trace);CHECK(trace.frozen&&!trace.active&&trace.count>=990&&trace.count<=VD_TRACE_CAPACITY);
     CHECK(vision_drive_trace_read(0,&first)&&vision_drive_trace_read(trace.count-1U,&last));
     CHECK(last.ms-first.ms>=59000U&&last.ms-first.ms<=62000U);
     CHECK(trace.count==VD_TRACE_CAPACITY&&first.ms>58000U);
     CHECK(last.state==VC_DISARMED&&!last.faults);}
    {ws_pi_t pi;ws_pi_reset(&pi);
     CHECK(ws_pi_step_ff(&pi,20,0,256,12,150,1,75)>=95);
     CHECK(ws_pi_step_ff(&pi,0,0,256,12,150,1,75)==0&&pi.integral_q8==0);
     for(unsigned i=0;i<1000;++i)CHECK(ws_pi_step_ff(&pi,40,0,256,12,150,1,150)<=150);
     CHECK(pi.integral_q8==0);}
    run_gray(0);CHECK(vision_drive_servo_command_us()>=1546&&vision_drive_servo_command_us()<=1552);
    run_gray(1);CHECK(vision_drive_servo_command_us()<=1530); /* right -> lower pulse */
    run_gray(-1);CHECK(vision_drive_servo_command_us()>=1570); /* left -> higher pulse */
    /* Interior 13px highlights cannot become a lane, even on a cold start.
     * A mature width also rejects a wider but still implausibly thin strip. */
    vision_midterm_reset();memset(turn_gray,30,sizeof(turn_gray));
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=88;x<=100;++x)turn_gray[y*VISION_WIDTH+x]=210;
    vision_midterm_process(turn_gray,turn_binary,&route_lane);
    CHECK(!route_lane.valid&&!route_lane.row_flags[90]&&!route_lane.row_flags[70]);
    run_gray(0);memset(turn_gray,30,sizeof(turn_gray));
    for(int y=0;y<VISION_HEIGHT;++y)for(int x=79;x<=108;++x)turn_gray[y*VISION_WIDTH+x]=210;
    vision_midterm_process(turn_gray,turn_binary,&route_lane);
    CHECK(!route_lane.valid&&!route_lane.row_flags[90]);
    /* Lane center is OUTSIDE view, but a substantial real white strip and
     * measured boundary remain at the edge. Keep their return direction. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(0);float centered_steer=vision_drive_output()->steering;
        turn_shift=direction*110;gray_road(direction);
        for(unsigned i=0;i<80;++i){
            vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            CHECK(route_lane.valid&&route_lane.inferred_rows>0&&vision_drive_is_local());
            CHECK(route_lane.center[90]==(direction>0?VISION_WIDTH-1:0));
            if(i>=1)CHECK(direction*vision_drive_output()->lateral>1.5f);
        }
        CHECK(direction*vision_drive_output()->steering>.95f);
        CHECK(direction>0?vision_drive_servo_command_us()==1430:vision_drive_servo_command_us()==1670);
        turn_shift=0;gray_road(0);
        for(unsigned i=0;i<60;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
        CHECK(vision_drive_is_local()&&vision_drive_output()->steering>centered_steer-.015f&&vision_drive_output()->steering<centered_steer+.015f);
    }
    /* No discontinuous jump at the former partial-path .12 threshold. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(0);partial_shift(direction*5);switch_wait(18,1);
        CHECK(vision_drive_output()->search_source==2);
        {float before=vision_drive_output()->steering;
         partial_shift(direction*6);switch_wait(6,1);
         float delta=vision_drive_output()->steering-before;if(delta<0)delta=-delta;
         CHECK(delta<.07f&&vision_drive_is_local());}
    }
    /* An already displaced car can build corrective steering at 3/s. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);set_lane(0);switch_wait(30,1);
        {float before=vision_drive_output()->steering;
         set_lane(direction*24);switch_wait(2,1);
         CHECK(direction*(vision_drive_output()->steering-before)>.05f);
         CHECK(direction*(vision_drive_output()->steering-before)<.061f);}
    }
    /* First turn is determined by this frame, with no warm-up right/left
     * sequence. Test both independent cold starts all the way through PWM. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);vision_midterm_reset();turn_shift=direction*20;gray_road(direction);
        for(unsigned i=0;i<100;++i){
            vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            if(vision_drive_is_local())CHECK(direction*vision_drive_output()->steering>=-.02f);
        }
        CHECK(vision_drive_is_local()&&direction*vision_drive_output()->steering>.25f);
    }
    /* A separated white patch is not the lane when it is disconnected from
     * the current lower corridor. Opposite prior frame must not select it. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(0);turn_shift=-direction*30;gray_road(-direction);
        for(unsigned i=0;i<30;++i)vision_midterm_process(turn_gray,turn_binary,&route_lane);
        turn_shift=direction*25;gray_road(direction);
        for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y)
            for(int x=2;x<=24;++x)turn_gray[y*VISION_WIDTH+(direction>0?x:VISION_WIDTH-1-x)]=210;
        for(unsigned i=0;i<60;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            CHECK(route_lane.valid&&route_lane.row_flags[90]);
            CHECK(direction*((int)route_lane.center[90]-94)>20);}
        CHECK(vision_drive_is_local()&&direction*vision_drive_output()->steering>.3f);
    }
    /* New partial white corridor is OPPOSITE the previous valid bend. It must
     * override that stale turn while still marked partial, within the same
     * bounded 300ms search window. No synthetic valid flag or deadline reset. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(direction);memset(turn_gray,30,sizeof(turn_gray));
        for(int y=70;y<=79;++y){int center=93-direction*32;
            for(int x=center-40;x<=center+40;++x)turn_gray[y*VISION_WIDTH+x]=210;}
        vision_midterm_process(turn_gray,turn_binary,&route_lane);
        CHECK(!route_lane.valid&&route_lane.measured_rows==10);
        switch_wait(2,1);CHECK(vision_drive_output()->recovering&&vision_drive_output()->search_source==2);
        CHECK(!vision_drive_output()->observation_valid);
        switch_wait(23,1);
        CHECK(vision_drive_is_local()&&direction*vision_drive_output()->steering<-.05f);
        turn_shift=0;gray_road(-direction);vision_midterm_process(turn_gray,turn_binary,&route_lane);
        switch_wait(2,1);CHECK(vision_drive_is_local()&&vision_drive_output()->observation_valid&&!vision_drive_output()->search_source);
    }
    /* White partial path straight ahead must unwind the old turn, not impose
     * a minimum right/left search angle just because the near band is absent. */
    run_gray(1);memset(turn_gray,30,sizeof(turn_gray));
    for(int y=70;y<=79;++y)for(int x=53;x<=133;++x)turn_gray[y*VISION_WIDTH+x]=210;
    vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(25,1);
    CHECK(vision_drive_is_local()&&vision_drive_output()->search_source==2);
    CHECK(vision_drive_output()->steering>-.08f&&vision_drive_output()->steering<.08f);
    /* After a zero-demand invalid straight frame, a newly visible current
     * partial path must ramp low drive back up instead of retaining duty=0. */
    run_gray(0);route_lane.valid=0;memset(route_lane.row_flags,0,sizeof(route_lane.row_flags));switch_wait(2,1);
    CHECK(vision_drive_output()->duty==0);
    memset(turn_gray,30,sizeof(turn_gray));
    for(int y=70;y<=79;++y)for(int x=23;x<=103;++x)turn_gray[y*VISION_WIDTH+x]=210;
    vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(20,1);
    CHECK(vision_drive_is_local()&&vision_drive_output()->search_source==2&&vision_drive_output()->steering<-.2f);
    CHECK(vision_drive_output()->duty>.02f&&info().target_right>3);
    switch_boot(0);measured_curve(.4f,0);switch_wait(80,1);
    CHECK(vision_drive_is_local()&&vision_drive_output()->far_valid&&vision_drive_servo_command_us()<=1530);
    measured_curve(.4f,1);switch_wait(30,1);
    CHECK(vision_drive_is_local()&&vision_drive_output()->far_valid&&vision_drive_servo_command_us()<=1530);
    measured_curve(-.4f,1);switch_wait(80,1);
    CHECK(vision_drive_is_local()&&vision_drive_output()->far_valid&&vision_drive_servo_command_us()>=1570);
    /* S transition: near band centered, current bend visible in middle rows,
     * next opposite bend stronger in distant rows. Do not turn toward it early. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);successive_bends(direction);switch_wait(80,1);
        CHECK(vision_drive_is_local()&&vision_drive_output()->far_valid);
        CHECK(direction*vision_drive_output()->steering>.08f);
        successive_bends(-direction);switch_wait(80,1);
        CHECK(-direction*vision_drive_output()->steering>.08f);
        set_lane(0);switch_wait(70,1);CHECK(vision_drive_output()->steering>-.03f&&vision_drive_output()->steering<.03f);
    }
    /* A->B and B->A, explicitly checked against the map: reverse starts with
     * bottom straight/right90/right return/left return/short straight/left90/
     * right return/top straight. No map order is compiled into the controller.
     * These are visual scenes, not timed driving or a vehicle dynamics model. */
    {const int scene[]={0,-1,-1,1,0,1,1,-1,-1,0,-1,0};
     for(unsigned reverse=0;reverse<2;++reverse){
        unsigned count=(unsigned)(sizeof(scene)/sizeof(scene[0]));switch_boot(0);
        for(unsigned s=0;s<count;++s){
            int direction=reverse?-scene[count-1U-s]:scene[s];
            measured_curve(direction*.45f,0);
            for(unsigned i=0;i<60;++i){switch_step(1);CHECK(vision_drive_is_local()||s==0);
                CHECK(vision_drive_servo_command_us()==0||(vision_drive_servo_command_us()>=1430&&vision_drive_servo_command_us()<=1670));}
            CHECK(vision_drive_is_local()&&!vision_drive_output()->faults);
            if(direction)CHECK(direction*vision_drive_output()->steering>.2f);
        }
        switch_wait(300,1);CHECK(vision_drive_is_local()&&vision_drive_output()->steering>-.03f&&vision_drive_output()->steering<.03f);
     }}
    /* B->A's consecutive reverse bends must keep drive while unwinding the
     * old turn. Newly visible opposite white pixels govern the new direction;
     * passing the steering center must not release a straight-speed burst. */
    switch_boot(0);measured_curve(.9f,0);route_lane.confidence=90;route_lane.measured_rows=87;
    switch_wait(120,1);CHECK(vision_drive_output()->steering>.7f);
    for(int direction=-1;direction<=1;direction+=2){
        measured_curve(direction*.9f,0);route_lane.confidence=90;route_lane.measured_rows=87;
        float previous=vision_drive_output()->duty;
        for(unsigned i=0;i<35;++i){
            switch_wait(2,1);float duty=vision_drive_output()->duty;
            CHECK(vision_drive_is_local()&&duty>=.065f&&duty<=.113f);
            CHECK(duty<=previous+.0041f);previous=duty;
            CHECK(info().target_right>3&&info().target_left>3);
        }
        CHECK(direction*vision_drive_output()->steering>.7f);
    }
    set_lane(0);switch_wait(160,1);
    CHECK(vision_drive_output()->steering>-.03f&&vision_drive_output()->steering<.03f);
    CHECK(vision_drive_output()->duty>.12f);
    /* Current edge re-enters after a ten-row strip. Do not synthesize the gap,
     * and do not stop tracing all farther rows because the strip was missing. */
    run_gray(0);gray_road(0);
    for(int y=75;y<=84;++y)memset(turn_gray+y*VISION_WIDTH,30,VISION_WIDTH);
    vision_midterm_process(turn_gray,turn_binary,&route_lane);
    CHECK(route_lane.valid&&route_lane.row_flags[70]==3&&route_lane.row_flags[80]==0);
    switch_wait(15,1);CHECK(vision_drive_is_local()&&vision_drive_output()->far_valid);
    for(int y=VISION_ROI_TOP;y<=74;++y)memset(turn_gray+y*VISION_WIDTH,30,VISION_WIDTH);
    for(int y=66;y<=69;++y)for(int x=54;x<=134;++x)turn_gray[y*VISION_WIDTH+x]=210;
    vision_midterm_process(turn_gray,turn_binary,&route_lane);
    CHECK(route_lane.row_flags[67]==0); /* Four isolated remote edge rows are insufficient. */
    /* Curved/oblique two-edge rows must not poison already matured widths. */
    run_gray(0);
    {uint16_t initial=route_lane.row_width[90];
     for(int y=0;y<VISION_HEIGHT;++y){int width=60+y*3/4;
        for(int x=0;x<VISION_WIDTH;++x)turn_gray[y*VISION_WIDTH+x]=(uint8_t)(x>=94-width/2&&x<=94+width/2?210:30);}
     for(unsigned i=0;i<30;++i)vision_midterm_process(turn_gray,turn_binary,&route_lane);
     turn_shift=30;gray_road(1);vision_midterm_process(turn_gray,turn_binary,&route_lane);
     CHECK(route_lane.valid&&route_lane.row_flags[90]==1&&route_lane.row_width[90]==initial);}
    /* Full previous turn -> crossed near center -> opposite correction ->
     * straight. Actual servo output must unwind without losing motor demand. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);measured_curve(direction*.6f,0);switch_wait(80,1);
        CHECK(direction*vision_drive_output()->steering>.35f);
        while(now%20U)switch_step(1);
        {float previous=vision_drive_output()->steering;int fast_release=0;
         crossed_center(direction);
         for(unsigned i=0;i<18;++i){
            switch_wait(2,1);float next=vision_drive_output()->steering;
            float change=next-previous;if(change<0)change=-change;
            CHECK(change<=.0601f);if(change>.045f)fast_release=1;
            CHECK(vision_drive_is_local()&&vision_drive_output()->duty>=.065f);
            previous=next;
         }
         CHECK(fast_release);}
        CHECK(direction*vision_drive_output()->lateral<-.3f);
        CHECK(direction*vision_drive_output()->steering<-.10f);
        CHECK(direction>0?vision_drive_servo_command_us()>1550:vision_drive_servo_command_us()<1550);
        set_lane(0);switch_wait(60,1);
        CHECK(vision_drive_output()->steering>-.03f&&vision_drive_output()->steering<.03f);
        CHECK(vision_drive_servo_command_us()>=1547&&vision_drive_servo_command_us()<=1552);
    }
    /* Learn this camera's widths, then follow a current edge clipped out of
     * the image for >100 frames (well past the old 5-frame expiry). */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(0);turn_shift=30*direction;gray_road(direction);
        for(unsigned i=0;i<140;++i){
            vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            CHECK(route_lane.inferred_rows>0&&route_lane.valid);
            CHECK(vision_drive_is_local()&&vision_drive_output()->observation_valid);
        }
        CHECK(direction>0?vision_drive_servo_command_us()<=1520:vision_drive_servo_command_us()>=1580);
        CHECK(vision_drive_output()->duty>=.035f); /* Valid bend target is not multiplied to a crawl. */
        memset(turn_gray,210,sizeof(turn_gray));vision_midterm_process(turn_gray,turn_binary,&route_lane);
        switch_wait(10,1);
        CHECK(vision_drive_is_local()&&vision_drive_output()->recovering);
        CHECK(!vision_drive_output()->observation_valid&&info().target_right<=19);
        CHECK(direction>0?vision_drive_servo_command_us()<=1520:vision_drive_servo_command_us()>=1580);
        gray_road(direction);
        for(unsigned i=0;i<30;++i){vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);}
        CHECK(vision_drive_is_local()); /* Current edge + retained geometry after exposure loss. */
    }
    /* Regression: a current, unambiguous lane beyond 1.25 half-widths
     * must steer under bounded output, not fault after ~80ms of take-up. */
    for(int direction=-1;direction<=1;direction+=2){
        switch_boot(0);set_lane(0);route_lane.confidence=85;
        for(int y=VISION_ROI_TOP;y<=VISION_ROI_BOTTOM;++y){
            int center=93+direction*48;
            route_lane.left[y]=(uint8_t)(center-18);route_lane.right[y]=(uint8_t)(center+18);
            route_lane.center[y]=(uint8_t)center;
        }
        switch_wait(120,1);
        CHECK(vision_drive_is_local()&&!vision_drive_output()->faults);
        CHECK(direction>0?vision_drive_output()->lateral>1.25f:vision_drive_output()->lateral< -1.25f);
        CHECK(direction>0?vision_drive_servo_command_us()==1430:vision_drive_servo_command_us()==1670);
        CHECK(vision_drive_output()->duty>=.065f&&info().target_right>3&&info().pwm_right>14);
        CHECK(vision_drive_output()->duty<=.145f&&info().pwm_right<=180&&info().pwm_left<=180);
        vision_drive_stop();
        {vd_last_stop_t stop;vision_drive_last_stop(&stop);CHECK(stop.previous_servo_us>=1430&&stop.previous_servo_us<=1670);}
    }
    /* A bend visible above the old fixed near band must still steer. Old
     * near-row gate rejected this scene regardless of visible far centerline. */
    for(int direction=-1;direction<=1;direction+=2){
        run_gray(0);gray_road(direction);
        for(int y=80;y<VISION_HEIGHT;++y)memset(turn_gray+y*VISION_WIDTH,30,VISION_WIDTH);
        for(unsigned i=0;i<80;++i){
            vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_step(1);
            CHECK(route_lane.valid&&vision_drive_is_local()&&vision_drive_output()->far_valid);
        }
        CHECK(direction>0?vision_drive_servo_command_us()<=1530:vision_drive_servo_command_us()>=1570);
    }
    /* Turn search is finite; seeing a fresh image is distinct from seeing a
     * valid lane. Reacquisition resumes immediately without OFF or re-arming. */
    run_gray(1);memset(turn_gray,210,sizeof(turn_gray));
    vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(15,1);
    CHECK(vision_drive_output()->recovering&&vision_drive_is_local()&&info().target_right>0);
    gray_road(1);vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(3,1);
    CHECK(!vision_drive_output()->recovering&&vision_drive_output()->observation_valid&&vision_drive_is_local());
    memset(turn_gray,210,sizeof(turn_gray));vision_midterm_process(turn_gray,turn_binary,&route_lane);
    switch_wait(340,1);CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_is_local());
    run_gray(-1);memset(turn_gray,210,sizeof(turn_gray));
    vision_midterm_process(turn_gray,turn_binary,&route_lane);switch_wait(10,1);
    CHECK(vision_drive_output()->recovering);switch_a=1;switch_wait(1,1);
    CHECK(!pwm[0]&&!pwm[1]&&!vision_drive_servo_command_us());
    run_gray(1);switch_wait(20,0);CHECK(!pwm[0]&&!pwm[1]); /* Camera stream stall never searches. */
    switch_boot(0);turn_shift=30;vision_midterm_reset();gray_road(1);
    vision_midterm_process(turn_gray,turn_binary,&route_lane);
    CHECK(route_lane.inferred_rows==0); /* No invented width at cold start. Actual two-edge rows may remain visible. */
    route_lane.valid=0;
    memset(route_lane.row_flags,1,sizeof(route_lane.row_flags));switch_wait(20,1);
    CHECK(!pwm[0]&&!pwm[1]); /* Inferred/single edges cannot create a steering path. */
    switch_a=1;switch_wait(5,1);CHECK(!pwm[0]&&!pwm[1]&&vision_drive_servo_command_us()==0);
    printf("TURN PATH PASS: %u checks; straight/left/right grayscale through actual vision and servo PWM; no physical bend claim.\n",checks);
    return 0;
}
