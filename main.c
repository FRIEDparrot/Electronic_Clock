#include "public.h"
#include "LED_controller.h"
#include "timer_controller.h"

#define LED_COL_PORT P0 
sbit K1 = P3^1; 
sbit K2 = P3^0; 
sbit K3 = P3^2; 
sbit K4 = P3^3;
sbit BEEP = P2^5;

static u8 hour = 0, min = 0,  sec = 0, day = 0;   // note day from Sunday to Saturday
static u8 Alm_hour = 0, Alm_min = 0, Alm_sec = 0; 
static u8 Tim_hour = 0, Tim_min = 0, Tim_sec = 0, Tim_xms = 0;    // for timer
static u8 Stp_hour = 0, Stp_min = 0, Stp_sec = 0, Stp_xms = 0; // for stopwatch 

// trigger interruption when timer0 overflows
void TIMER0_triggered() interrupt 1{
    static u8 msecnt = 0;
    TH0 = 0x4c; TL0 = 0x00;  // trigger every 50ms 
    msecnt++;
    if (msecnt == 20){   // reset the clock
        msecnt = 0;
        if (sec <59) {sec += 1;}
        else if (min < 59) { min +=1; sec = 0;}
        else if (hour < 23) { hour +=1; min =0; sec = 0;}
        else {day = (day+1)%7; hour = 0; min = 0; sec = 0;} 
    }
}

// Delay approx to 16 ms in func 
void ShowCurrentTime(u8 state){ // because second is self-increasing, we need another function for set correct time
    // calculate current time.
    if (sec >= 60 || sec < 0){ sec = (sec+60)%60;}
    if (min >= 60 || min < 0){ min = (min+60) % 60;}
    if (hour>= 24 || hour < 0){ hour = (hour+24) %24;}
    if (day >= 7  || day < 0) { day = (day+7) % 7;}
    // represent which day in a week
    LED_ShowDateTime(hour, min, sec, day, state);
}

u16 blink_tick = 0; // record the blink time 
u8 setstate = 0; u16 setstatetick = 0; // must be put outside the loop function (calculate each loop) 
u8 Timer_On = 0 , StpWatch_On = 0;
u8 StopWatch_Overflow = 0;
u8 AlarmRing = 0;   // alarmring signal  

// Change the state after each cycle 
void StateTickChange(void){
    if (setstatetick > 5000){ // if the button K1 is not pressed about 6 seconds, exit setting mode
        setstatetick = 0; setstate = 0;
    }else if (setstate!=0){   // check if setstate is on; 
        setstatetick+= 15;
    }
    blink_tick = (blink_tick + 20)%1000;
}

// choose which timer should be set in which way 
void SetTime(u8 clockNum, u8 setway){
    // unit 3 clocks into 1 calculate way
    u8 h_add = 0, m_add = 0, s_add = 0, d_add = 0;
    switch (setway) // 0: set to 0, 1: +1;  2: -1;
    {
    case 0:
        switch (clockNum) // set to zero
        {
        case 0: h_add = 24 - hour; m_add = 60 - min; s_add = 60 - sec; d_add = 7 - day; break;
        case 1: h_add = 24 - Alm_hour; m_add = 60 - Alm_min; s_add = 60 - Alm_sec; break;
        case 2: h_add = 100 - Tim_hour; m_add = 60 - Tim_min; s_add = 60 - Tim_sec; break; // note the range of timer is 100 hours
        default: break;
        } break;
    case 1: h_add = 1; m_add = 1; s_add = 1; d_add = 1; break;
    case 2: if(clockNum == 2)
                h_add = 99;
            else
                h_add = 23;
            m_add = 59; s_add = 59; d_add = 6; break;
    default: break;
    }

    switch (clockNum)
    {
    case 0:
        switch (setstate)
        {
        case 1: hour = (hour+h_add)%24; break;
        case 2: min =  (min+m_add)%60; break;
        case 3: sec =  (sec+s_add)%60; break;  // reset the crystal timer 
        case 4: day =  (day+d_add)%7; break;
        default: break;
        } break;
    case 1: 
        switch (setstate)
        {
        case 1: Alm_hour =(Alm_hour + h_add)%24; break;
        case 2: Alm_min = (Alm_min + m_add)%60; break;
        case 3: Alm_sec = (Alm_sec + s_add)%60; break;
        default: break;
        }break;
    case 2: // only use in timer
        switch (setstate)
        {
        case 1: Tim_hour =(Tim_hour + h_add)%100; break;
        case 2: Tim_min = (Tim_min + m_add)%60; break;
        case 3: Tim_sec = (Tim_sec + s_add)%60; break;
        default: break;
        } break;
    default:
        break;
    }
    setstatetick = 0; // reset the blink time memory
}

// called when change the state 
void ResetStateTimer(void){
    setstate = 0; setstatetick = 0; blink_tick = 0;
}

/*timer1 Region*/
// trigger interruption when timer1 overflows

// for stopwatch, we only need to write the function in the 
void TIMER1_triggered() interrupt 3{
    TH1 = 0xdc; TL1 = 0x00; // trigger every 10ms
    if (Timer_On){
        Tim_xms++;
        if (Tim_xms == 100){
            if (Tim_sec > 0) Tim_sec--;
            else if (Tim_min > 0) {Tim_sec = 59; Tim_min--;}
            else if (Tim_hour > 0){Tim_sec = 59; Tim_min = 59; Tim_hour--;}
            Tim_xms = 0;
        }
    }
    if (StpWatch_On){
        Stp_xms++;
        if (Stp_xms == 100){
            Stp_xms = 0;
            if (Stp_sec < 59) {Stp_sec++;}
            else if (Stp_min < 59) { Stp_min++; Stp_sec = 0;}
            else if (Stp_hour < 99) {Stp_hour++; Stp_min = 0; Stp_sec = 0;}
            else {// Overflow and reset the StopWatch 
                StopWatch_Overflow = 1; AlarmRing = 1;
                StpWatch_On = 0;
                Stp_hour = 0; Stp_sec = 0; Stp_min = 0;
            }
        }
    }
}

void RingBeep(u8 ringmode){  // ringmode = 0: cl ringmode = 1: err
    u16 wtime = 0;
    blink_tick = 0;
    while (AlarmRing){
        if (ringmode== 0){
            LED_ShowChar(3, 0x39); 
            LED_ShowChar(4, 0x38);
        }else{
            LED_ShowChar(0, 0x79);
            LED_ShowChar(1, 0x50);
            LED_ShowChar(2, 0x50);
        }

        if (wtime < 750 && blink_tick < 120){
            BEEP = !BEEP;
        }
        Delay(1);
        wtime = wtime > 1200? 0: wtime + 1;
        blink_tick = blink_tick > 200 ? 0 : blink_tick + 1;
        if (!(K1 && K2 && K3 && K4)){
            Delay(10);
            if (!(K1 && K2 && K3 && K4)){
                AlarmRing = 0;
            }
        }
    }
    while (!(K1 && K2 && K3 && K4));
    BEEP = 1; blink_tick = 0;
}

void main(){
    u8 mode = 0;
    u8 AlarmOn = 0;   // if using the alarm
    LED_SelfCheck();
    P2_6 = 1, P2_7 = 1;
    TIMER0_START();     // start the timer 
    TIMER1_START();  
    while (1){
        /* Clock Mode */ 
        if (mode == 0){ // clock mode 
            // switch between extinguished or lightened
            if (blink_tick < 500){ // 0.5s as blink loop    
                ShowCurrentTime(setstate);
            }else{
                ShowCurrentTime(0);
            }
            StateTickChange();
            if (!K1){
                u16 K1_time = 0; // don't use u8
                Delay(10);
                while (!K1){
                    if (K1_time < 1000) K1_time++;
                    Delay(1);
                }
                if (K1_time > 700){ // change to mode 1
                    mode = 1; P2_7 = 0, P2_6 = 1; // change to mode 1
                    ResetStateTimer();
                } else if (K1_time > 0){
                    setstate = (setstate + 1)%5; 
                    setstatetick = 0;
                    // reset the tick and wait for another 5s
                }
                // no need to add while (!K1) again
            }
            if (!K2){
                Delay(10);
                if (!K2 && setstate != 0){
                    SetTime(0,2);
                    while (!K2);
                }
            }
            if (!K3){
                Delay(10);
                if (!K3 && setstate != 0){
                    SetTime(0, 1);
                    while (!K3);
                }
            }
            if (!K4){
                Delay(10);
                if (!K4){
                    SetTime(0,0);
                    if (setstate == 3) {
                        TH0 = 0xee; TL0 = 0x00;
                    }
                    while (!K4);            
                }
            }
            // only ring alarm when in clock mode 
            if (AlarmOn&& hour == Alm_hour && min == Alm_min && sec == Alm_sec){
                AlarmRing = 1;
            }
        }/* Alarm Setting Mode*/
        else if (mode == 1){ // alarm mode 
            if (blink_tick < 500){ // blink cycle 0.6s
                LED_ShowDateTime(Alm_hour, Alm_min, Alm_sec, 7, 0);
            }else{
                LED_ShowDateTime(Alm_hour, Alm_min, Alm_sec, 7, setstate);
            }
            if (AlarmOn == 0) LED_ShowChar(7, 0x71); // F,  from high digit to low digit
            else LED_ShowChar(7, 0x73);              // P
            StateTickChange();
            if (!K1) {
                u16 cnt = 0;
                Delay(10);
                while (!K1) {
                    if (cnt < 2000) cnt++;
                    Delay(1);
                }
                if (cnt > 700) {
                    mode =2; P2_6 = 0; P2_7 = 1;
                    ResetStateTimer();
                }
                else if (cnt > 0){
                    setstate = (setstate + 1)%4; // from 0 to 3 to set h, m and s
                    setstatetick = 0; // reset the setting
                }
            }
            if (!K2){
                Delay(10);
                if (!K2 && setstate!=0){
                    SetTime(1, 2);
                    while (!K2);
                }
            }
            if (!K3){
                Delay(10);
                if (!K3 && setstate!=0){
                    SetTime(1, 1);
                    while (!K3);
                }
            }
            if (!K4){
                Delay(10);
                if(!K4){
                    if (setstate == 0){
                        AlarmOn = !AlarmOn;
                    }else{
                        SetTime(1, 0);
                    }
                    while (!K4);
                }
            }
            
        }
        else if (mode == 2){ // Timer mode
            if (blink_tick < 500){ 
                LED_ShowDateTime(Tim_hour, Tim_min, Tim_sec, 7, 0);
            }else{
                LED_ShowDateTime(Tim_hour, Tim_min, Tim_sec, 7, setstate);
            }
            StateTickChange();
            if (!K1) {
                u16 cnt = 0;
                Delay(10);
                while (!K1) {
                    if (cnt < 1000) cnt++;
                    Delay(1);
                }
                if (cnt > 800) {
                    mode = 3; P2_6 = 0; P2_7 = 0;
                    // not reset timer 1 here;
                    ResetStateTimer();
                }
                else if (cnt > 0){
                    setstate = (setstate + 1)%4; // from 0 to 3 to set h, m and s
                    setstatetick = 0; // reset the setting 
                }
            }
            if (!K2) {
                Delay(10);
                if (!K2){
                    SetTime(2, 2);
                    while(!K2);
                }
            }
            if (!K3) {
                Delay(10);
                if (!K3){
                    SetTime(2,1);
                    while(!K3);
                }
            }
            if (!K4) { // 
                u16 k4ms = 0;
                Delay(10);
                while (!K4){
                    if (k4ms < 1500) k4ms++;
                    Delay(1);
                }
                if (k4ms > 800){
                    Timer_On = 0; // reset the timer 
                    Tim_hour = 0;  Tim_min = 0; Tim_sec = 0; Tim_xms = 0;
                }
                else if (k4ms > 0){
                    if (setstate == 0) { 
                    // start the timer or pause the Timer 
                    Timer_On = !Timer_On;
                    }
                    else{
                        SetTime(2,0);
                    }
                }
            }
        }
        else if (mode == 3){ // stopwatch mode 
            u8 xms_show = 0;
            xms_show = Stp_xms;
            LED_ShowExactTime(Stp_hour,Stp_min, Stp_sec, xms_show); // show 
            if (!K1) {
                u16 cnt = 0;
                Delay(10);
                while (!K1) {
                    if (cnt < 1000) cnt++;
                    Delay(1);
                }
                if (cnt > 800) {
                    mode = 0; P2_6 = 1; P2_7 = 1;
                    ResetStateTimer();
                }
                else if (cnt > 0){
                    StpWatch_On = !StpWatch_On;
                }
            }
            if (!K4) {
                u16 cnt = 0;
                Delay(10);
                if (!K4){
                    StpWatch_On = 0;
                    Stp_hour = 0;  Stp_min = 0; Stp_sec = 0; Stp_xms = 0;
                }
                while (!K4);
            }
        }
        
        // if need to ring the beep 
        if (Timer_On && Tim_sec == 0 && Tim_min == 0 && Tim_hour == 0){
            Timer_On = 0; Tim_xms = 0; AlarmRing = 1;
        }
        if (AlarmRing) RingBeep(StopWatch_Overflow); // check if beep need to ring
    }
}