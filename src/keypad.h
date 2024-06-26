#include <EncButton.h>

#define UP_BTN 39
#define DN_BTN 36
#define OK_BTN 33
#define LT_ENC 34
#define RT_ENC 35

Button up(UP_BTN,INPUT_PULLUP,LOW);
Button dn(DN_BTN,INPUT_PULLUP,LOW);
EncButton enc(LT_ENC,RT_ENC,OK_BTN,INPUT,INPUT);

void buttonsSetup(){
  enc.setEncType(EB_STEP2);
  enc.setClickTimeout(200);
  enc.setHoldTimeout(500);
  enc.setFastTimeout(100);
  dn.setClickTimeout(200);
  dn.setHoldTimeout(500);
  up.setClickTimeout(200);
  up.setHoldTimeout(300);
}

void playerModeChange(){

}

bool isRight=false,isLeft=false;

void buttonsTick(){
  // if(enc.right()){
  //   isRight=true;
  // }
  // if (enc.left()){
  //   isLeft=true;
  // }
  // if(enc.timeout(400)){
  //   if(isRight){
  //     isRight=false;
  //   }
  //   if(isLeft){
  //     isLeft=false;
  //   }
  // }
  // if (enc.rightH()){

  // }
  // if (enc.release()){

  // }
  // if (enc.click()){

  // }
  // if (enc.hold()){

  // }
}

void generalTick(){
  enc.tick();
  up.tick();
  dn.tick();

  if(enc.hold()){
    if(PlayerCTRL.screen_mode!=SCR_CONFIG&&PlayerCTRL.screen_mode!=SCR_RESET_CONFIG&&PlayerCTRL.screen_mode!=SCR_ABOUT){
      sound_play(SFX_SELECT);
      PlayerCTRL.screen_mode++;
      if(PlayerCTRL.screen_mode==SCR_CONFIG) PlayerCTRL.screen_mode=SCR_PLAYER;
      PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
    }
  }
  if(enc.hasClicks(2)){
    if(PlayerCTRL.screen_mode!=SCR_CONFIG&&PlayerCTRL.screen_mode!=SCR_RESET_CONFIG&&PlayerCTRL.screen_mode!=SCR_ABOUT){
      sound_play(SFX_SELECT);
      PlayerCTRL.prev_screen_mode=PlayerCTRL.screen_mode;
      PlayerCTRL.screen_mode=SCR_CONFIG;
      PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
    }
  }
  // if (enc.leftH()){

  // }
  // if (up.click()||up.holding()){
  //   if (!enc.holding()){
      
  //   }else{
      
  //   }
  // }
  // if (dn.click()||dn.holding()){
  //   if (!enc.holding()){
      
  //   }else{
      
  //   }
  // }
  // if (dn.click(2)) {

  // }
  // if (dn.timeout(2000)||up.timeout(2000)){

  // }
  // buttonsTick();
}
