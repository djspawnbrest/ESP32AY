#include <Arduino.h>
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  #include "s3wrapper.h"
#endif
#include <SPI.h>
#include <SdFat.h>
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef DEBUG_RAM
#include "debug.h"
#endif

#include "defines.h"
#include "rtc.h"
#include "amp.h"
#include "keypad.h"
#include "tftui.h"
#undef FILE_READ
#undef FILE_WRITE
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
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "zp/zpscreen.h"      // the Z-Player look: its screen, font, logo
#include "zp/zpmod.h"         // ... what it shows of a module
#include "players/GSPlay.h"   // .mod through General Sound
#include "zp/zplayout.h"      // ... the player screens' layout
#include "zp/zpplayer.h"      // ... its player screen
#include "zp/zpbrowser.h"     // ... its file browser
#include "zp/zpgen.h"         // ... for what the ESP plays itself
#include "zp/zpay.h"          // ... and for the AY formats
#include "zp/zpintro.h"       // ... its boot screen
#include "zp/zpconfig.h"      // ... its Set-Up page
#else
#include "zp/zpnone.h"        // the classic ESP32: no PSRAM, upstream's look and MOD player
#endif
#include "players/S3MPlay.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "players/XMPlay.h" 
#endif
#include "players/TAPPlay.h"
#include "players/TZXPlay.h"
#include "players/MP3Play.h"
#include "players/WAVPlay.h"

// #include "sdTest.h"
#include "player.h"
#include "uart.h"

void setup(){
#ifdef DEBUG_RAM
  printf("Setup start\n");
  checkHeap();
#endif
  initSemaphore();
  i2cInit();
  blPinSetup();
  display_brightness(0);
  startup_config_load();
  ampInit();
  muteAmp();
  checkStartUpConfig();
  initRTC();
  setRTC();
  printRTCStatus();
  initVoltage();
  TFTInit();
  buttonsSetup();
  DACInit();
  AYInit();
  GS_Init();       // General Sound: its ROM starts in core 0's spare time
  if(lfsConfig.skin==SKIN_WILD) introTFT(); else zpIntro();  // the skin's boot screen
  delay(2000);
  show_frame();
  playerSourceChange(); // ay playcore or uart playcore
  checkSDonStart();
  muteAYBeep();
  if(lfsConfig.playerSource==PLAYER_MODE_UART) unMuteAmp();
#ifdef DEBUG_RAM
  printf("Setup end\n");
  checkHeap();
#endif  
}

void loop(){
  getDateTime();
  generalTick();
  player();
  if(zpOwns&&!zpMine()) zpRelease();   // upstream's screen again: its frame first
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
    case SCR_ALERT:
      alert_screen();
      break;
  }
  // Yield to other tasks more frequently for better encoder response
  taskYIELD();
  vTaskDelay(pdMS_TO_TICKS(1));
  scrTimeout();
  esp_task_wdt_reset();
}
