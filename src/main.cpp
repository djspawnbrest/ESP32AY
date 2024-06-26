#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

#include "defines.h"

#include "res/select.h"
#include "res/move.h"
#include "res/cancel.h"

#include "amp.h"
#include "keypad.h"
#include "tftui.h"
#include "config.h"
#include "playlist.h"
#include "browser.h"

// void dbg(){
//   Serial.print("Ayl file: ");
//   Serial.println(Config.ayl_file);
//   Serial.print("Play ayl file: ");
//   Serial.println(Config.play_ayl_file);
//   Serial.print("Play dir: ");
//   Serial.println(Config.play_dir);
//   Serial.print("Active dir: ");
//   Serial.println(Config.active_dir);
//   Serial.print("Prev dir: ");
//   Serial.println(Config.prev_dir);
//   Serial.print("Cur dir: ");
//   Serial.println(Config.dir_cur);
//   Serial.print("Prev dir cur: ");
//   Serial.println(Config.dir_cur_prev);
//   Serial.print("Play cur: ");
//   Serial.println(Config.play_cur);
//   Serial.print("Count files: ");
//   Serial.println(Config.play_count_files);
//   Serial.print("isBrowserPlaylist: ");
//   Serial.println(Config.isBrowserPlaylist?"YES":"NO");
//   Serial.print("isPlayAYL: ");
//   Serial.println(Config.isPlayAYL?"YES":"NO");
//   Serial.print("Cursor offset: ");
//   Serial.println(cursor_offset);
// }

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

#include "player.h"

void setup(){
  display_brightness(0);
  Serial.begin(115200);
  config_load();
  AYPlayCoreInit();
  initVoltage();
  TFTInit();
  buttonsSetup();
  DACInit();
  // AYTimerInit();
  AYInit();
  muteAYBeep();
  ampInit();
  introTFT();
  delay(2000);
  show_frame();
  checkSDonStart();
}

void loop(){
  ay_set_clock(Config.ay_clock);
  generalTick();
  player();
  switch(PlayerCTRL.screen_mode){
    case SCR_PLAYER:
      player_screen();
      break;
    case SCR_BROWSER:
      browser_screen(Config.isBrowserPlaylist);
      break;
    case SCR_CONFIG:
      config_screen();
      break;
    case SCR_RESET_CONFIG:
      config_reset_default_screen();
      break;
    case SCR_ABOUT:
      config_about_screen();
      break;
    case SCR_SDEJECT:
      sdEject();
      checkSDonStart();
      break;
  }
  scrTimeout();
}