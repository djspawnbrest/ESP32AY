#include <Wire.h>
#include <RTClib.h>

#ifndef TEST_RTC
RTC_DS3231 rtc;
#else
RTC_DS1307 rtc;
#endif

DateTime now;
DateTime setTime;
const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char daysOfTheWeekShort[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char monthsOfTheYear[13][12] = {"","January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

void initRTC(){
  if(foundRtc){
    if(!rtc.begin()){
      printf("Couldn't find RTC\n");
      return;
    }
    printf("RTC running!\n");
  }
}

void setRTC(){
  if(foundRtc){
    setTime = DateTime(F(__DATE__), F(__TIME__));
  #ifdef TEST_RTC
    if(!rtc.isrunning()){
      printf("RTC is NOT running, let's set the time!\n");
  #else
    if(rtc.lostPower()){
      printf("RTC is NOT running, let's set the time!\n");
  #endif
      rtc.adjust(setTime);
      printf("RTC set!\n");
    }
  }
}

void getDateTime(){
  if(foundRtc){
    // static unsigned long previousMillis = 0;
    // unsigned long currentMillis = millis();
    // if (currentMillis - previousMillis >= 500) { // 500ms = half second
    //   previousMillis = currentMillis;
      now = rtc.now();
      // printf("\r%02d/%02d/%02d %02d:%02d:%02d",now.day(),now.month(),now.year(),now.hour(),now.minute(),now.second());
    // }
  }
}