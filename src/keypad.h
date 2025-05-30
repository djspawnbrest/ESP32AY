#include <EncButton.h>
#include <PCF8574.h>

#define LT_ENC P0
#define RT_ENC P1

#define OK_BTN P2
#define DN_BTN P3
#define UP_BTN P4

VirtButton up;
VirtButton dn;
VirtEncButton enc;

PCF8574::DigitalInput di;
PCF8574 ext(0x20);

void buttonsSetup(){
  ext.pinMode(LT_ENC, INPUT_PULLUP);
  ext.pinMode(RT_ENC, INPUT_PULLUP);
  ext.pinMode(OK_BTN, INPUT_PULLUP);
  ext.pinMode(DN_BTN, INPUT_PULLUP);
  ext.pinMode(UP_BTN, INPUT_PULLUP);
  ext.begin();
  pinMode(LT_ENC, INPUT_PULLUP);
  pinMode(RT_ENC, INPUT_PULLUP);
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
  di=ext.digitalReadAll();
  enc.tick(!di.p0,!di.p1,!di.p2);
  dn.tick(!di.p3);
  up.tick(!di.p4);
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
}
