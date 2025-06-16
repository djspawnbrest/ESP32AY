#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "defines.h"
#include "rtc.h"
#include "amp.h"
#include "keypad.h"
#include "tftui.h"
#include "config.h"
#include "playlist.h"
#include "browser.h"

#include "sound.h"
#include "eq.h"
#include "ay.h"
#include "ayflyw.h"

#include "players/ASCPlay.h"
#include "players/PSCPlay.h"
#include "players/PT1Play.h"
#include "players/PT2Play.h"
#include "players/PT3Play.h"
#include "players/SQTPlay.h"
#include "players/STCPlay.h"
#include "players/STPPlay.h"
#include "players/AYPlay.h"
#include "players/PSGPlay.h"
#include "players/MODPlay.h"
#include "players/S3MPlay.h"

// #include "sdTest.h"
#include "player.h"
#include "uart.h"

void setup(){
  checkHeap();
  initSemaphore();
  i2cInit();
  blPinSetup();
  display_brightness(0);
  startup_config_load();
  checkStartUpConfig();
  initRTC();
  setRTC();
  initVoltage();
  TFTInit();
  buttonsSetup();
  DACInit();
  AYInit();
  ampInit();
  introTFT();
  delay(2000);
  show_frame();
  playerSourceChange(); // ay playcore or uart playcore
  checkSDonStart();
  muteAYBeep();
}

void loop(){
  getDateTime();
  generalTick();
  player();
  switch(PlayerCTRL.screen_mode){
    case SCR_PLAYER:
      player_screen();
      break;
    case SCR_BROWSER:
      browser_screen(sdConfig.isBrowserPlaylist);
      break;
    case SCR_CONFIG:
      config_screen();
      break;
    case SCR_RESET_CONFIG:
      config_reset_default_screen();
      break;
    case SCR_DATETIME:
      time_date_screen();
      break;
    case SCR_ABOUT:
      config_about_screen();
      break;
    case SCR_NOFILES:
      noFilesFound();
      checkSDonStart();
      break;
    case SCR_SDEJECT:
      sdEject();
      checkSDonStart();
      break;
  }
  vTaskDelay(pdMS_TO_TICKS(1));
  scrTimeout();
  esp_task_wdt_reset();
}
