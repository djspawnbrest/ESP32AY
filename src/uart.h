#include <Arduino.h>
#include "driver/uart.h"

bool reg=false;
byte reg_num=0;
bool prevIsPlayState=true;

TaskHandle_t uartTaskHandle=NULL;

void UARTTask(void *pvParameters){
  while(1){
    if(Serial.available()){
      byte r=Serial.read(); // read byte from FIFO
      if(!reg){
        if(r<=15){
          reg_num=r;
          reg=true;
        }
      }else{
        ay_write_remap(0,reg_num,r);
        reg=false;
      }
    }
    vTaskDelay(1);
    esp_task_wdt_reset();
  }
  vTaskDelete(NULL);
}

void UARTPlayCoreInit(){
  xTaskCreatePinnedToCore(
    UARTTask,  // Function to implement the task
    "UARTTask",    // Name of the task
    4096,       // Stack size in words
    NULL,        // Task input parameter
    1,           // Priority of the task
    &uartTaskHandle,     // Task handle.
    0            // Core where the task should run
  );
}

void playerSourceChange(){
  if(Config.playerSource==PLAYER_MODE_SD){
    muteAYBeep();
    PlayerCTRL.isPlay=prevIsPlayState;
    if(uartTaskHandle!=NULL){
      vTaskDelete(uartTaskHandle);
      uartTaskHandle=NULL;
      Serial.end();
    }
    if(ayPlayTaskHandle==NULL){
      AYPlayCoreInit();
    }
  }
  if(Config.playerSource==PLAYER_MODE_UART){
    prevIsPlayState=PlayerCTRL.isPlay;
    PlayerCTRL.isPlay=false;
    muteAYBeep();
    if(ayPlayTaskHandle!=NULL){
      vTaskDelete(ayPlayTaskHandle);
      ayPlayTaskHandle=NULL;
    }
    if(uartTaskHandle==NULL){
      Serial.begin(57600);
      UARTPlayCoreInit();
    }
  }
}
