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
    DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
    bool needAdjust = false;
    
    // Проверяем флаг потери питания
  #ifdef TEST_RTC
    bool powerLost = !rtc.isrunning();
  #else
    bool powerLost = rtc.lostPower();
  #endif
    
    if(powerLost){
      printf("RTC OSF flag is set (power was lost)\n");
      
      // Читаем текущее время из RTC
      DateTime currentTime = rtc.now();
      
      printf("Current RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
             currentTime.year(), currentTime.month(), currentTime.day(),
             currentTime.hour(), currentTime.minute(), currentTime.second());
      
      printf("Compile time: %04d-%02d-%02d %02d:%02d:%02d\n",
             compileTime.year(), compileTime.month(), compileTime.day(),
             compileTime.hour(), compileTime.minute(), compileTime.second());
      
      // Проверяем валидность времени
      // Если год < 2020 - время точно невалидно
      if(currentTime.year() < 2020){
        printf("RTC time is invalid (year < 2020), setting compile time\n");
        needAdjust = true;
      }
      // Если время в RTC старше времени компиляции - время невалидно
      else if(currentTime.unixtime() < compileTime.unixtime()){
        printf("RTC time is older than compile time, setting compile time\n");
        needAdjust = true;
      }
      else{
        printf("RTC time is valid, keeping current time and clearing OSF flag\n");
        // Очищаем флаг OSF без изменения времени
        rtc.adjust(currentTime);
        needAdjust = false;
      }
    }
    else{
      printf("RTC is running normally (OSF flag clear)\n");
    }
    
    if(needAdjust){
      printf("Setting RTC to compile time\n");
      rtc.adjust(compileTime);
    }
  }
}

void printRTCStatus(){
  if(foundRtc){
    printf("\n=== RTC Diagnostic ===\n");
    printf("RTC Address: 0x%02X\n", rtcAddress);
    
  #ifdef TEST_RTC
    printf("RTC Type: DS1307 (TEST_RTC mode)\n");
    printf("Running: %s\n", rtc.isrunning() ? "YES" : "NO");
  #else
    printf("RTC Type: DS3231\n");
    printf("Power Lost Flag (OSF): %s\n", rtc.lostPower() ? "SET" : "CLEAR");
  #endif
    
    DateTime now = rtc.now();
    printf("Current RTC Time: %04d-%02d-%02d %02d:%02d:%02d (%s)\n",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(),
           daysOfTheWeek[now.dayOfTheWeek()]);
    
    DateTime compile = DateTime(F(__DATE__), F(__TIME__));
    printf("Compile Time: %04d-%02d-%02d %02d:%02d:%02d\n",
           compile.year(), compile.month(), compile.day(),
           compile.hour(), compile.minute(), compile.second());
    
    printf("Unix Time RTC: %lu\n", now.unixtime());
    printf("Unix Time Compile: %lu\n", compile.unixtime());
    
    if(now.unixtime() >= compile.unixtime()){
      printf("Time difference: +%lu seconds (RTC is ahead)\n", 
             now.unixtime() - compile.unixtime());
    }else{
      printf("Time difference: -%lu seconds (RTC is behind)\n", 
             compile.unixtime() - now.unixtime());
    }
    
    printf("======================\n\n");
  }
}

void getDateTime(){
  if(foundRtc){
    static unsigned long previousMillis=0;
    unsigned long currentMillis=millis();
    if (currentMillis-previousMillis>=500){ // 500ms = half second
      previousMillis=currentMillis;
      now=rtc.now();
      // printf("\r%02d/%02d/%02d %02d:%02d:%02d",now.day(),now.month(),now.year(),now.hour(),now.minute(),now.second());
    }
  }
}