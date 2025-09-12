#include <EncButton.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define DN_BTN 40
#define UP_BTN 41
#define OK_BTN 42
#define RT_ENC 1
#define LT_ENC 2
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define UP_BTN 39
#define DN_BTN 36
#define OK_BTN 33
#define LT_ENC 34
#define RT_ENC 35
#endif

Button up(UP_BTN,INPUT_PULLUP,LOW);
Button dn(DN_BTN,INPUT_PULLUP,LOW);
EncButton enc(LT_ENC,RT_ENC,OK_BTN,INPUT_PULLUP,INPUT_PULLUP);

void buttonsSetup(){
  enc.setEncType(lfsConfig.encType);
  enc.setEncReverse(lfsConfig.encReverse);
  enc.setClickTimeout(200);
  enc.setHoldTimeout(500);
  enc.setFastTimeout(100);
  dn.setClickTimeout(200);
  dn.setHoldTimeout(500);
  up.setClickTimeout(200);
  up.setHoldTimeout(300);
}

bool isRight=false,isLeft=false;

void generalTick(){
  enc.tick();
  up.tick();
  dn.tick();
  if(enc.hold()){
    if(PlayerCTRL.screen_mode!=SCR_CONFIG&&PlayerCTRL.screen_mode!=SCR_RESET_CONFIG&&PlayerCTRL.screen_mode!=SCR_ABOUT){
      PlayerCTRL.screen_mode++;
      if(PlayerCTRL.screen_mode==SCR_CONFIG) PlayerCTRL.screen_mode=SCR_PLAYER;
      PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
      if(PlayerCTRL.screen_mode==SCR_BROWSER) scrNotPlayer=true;
    }
  }
  if(enc.hasClicks(2)){
    if(PlayerCTRL.screen_mode!=SCR_CONFIG&&PlayerCTRL.screen_mode!=SCR_RESET_CONFIG&&PlayerCTRL.screen_mode!=SCR_ABOUT){
      PlayerCTRL.prev_screen_mode=PlayerCTRL.screen_mode;
      PlayerCTRL.screen_mode=SCR_CONFIG;
      PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
      scrNotPlayer=true;
    }
  }
  if(PlayerCTRL.screen_mode==SCR_ALERT){scrNotPlayer=true;}
}
