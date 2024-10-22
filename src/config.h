#include <LittleFS.h>

void config_default(){
  memset(&Config,0,sizeof(Config));
  Config.playerSource=PLAYER_MODE_SD;
  Config.ay_layout=LAY_ABC;
  Config.ay_clock=CLK_PENTAGON;
  Config.volume=32;
  Config.scr_bright=100;
  Config.scr_timeout=0;
  Config.play_mode=PLAY_MODE_ALL;
  Config.sound_vol=VOL_MID;
  browser_reset_directory();
}

void config_load(){
  config_default();
  LittleFS.begin();
  fs::File f=LittleFS.open(CFG_FILENAME,"r");
  if(f){
    if(f.size()==sizeof(Config)){
      f.readBytes((char*)&Config,sizeof(Config));
    }
    f.close();
  } else {
    if(!LittleFS.exists(CFG_FILENAME)){
      fs::File f=LittleFS.open(CFG_FILENAME, "w");
      if(f){
        f.write((const uint8_t*)&Config, sizeof(Config));
        f.close();
      }
    }
  }
  LittleFS.end();
}

void config_save(){
  LittleFS.begin();
  fs::File f=LittleFS.open(CFG_FILENAME, "w");
  if (f){
    f.write((const uint8_t*)&Config, sizeof(Config));
    f.close();
  }
  LittleFS.end();
}

void configResetPlayingPath(){
  strncpy(Config.play_dir,"/",sizeof(Config.play_dir)-1);
  strncpy(Config.active_dir,"/",sizeof(Config.active_dir)-1);
  strncpy(Config.prev_dir,"/",sizeof(Config.prev_dir)-1);
  // memset(sort_list,0,sizeof(sort_list));
  // memset(sort_list_play,0,sizeof(sort_list_play));
  Config.isPlayAYL=false;
  Config.isBrowserPlaylist=false;
  Config.play_count_files=0;
  // sort_list_len=0;
  Config.play_ayl_file[0]=0;
  Config.ayl_file[0]=0;
  Config.play_cur=0;
  Config.dir_cur=0;
  config_save();
}

void config_about_screen(){
  char buf[32];
  if(PlayerCTRL.scr_mode_update[SCR_ABOUT]){
    PlayerCTRL.scr_mode_update[SCR_ABOUT]=false;
    img.setColorDepth(8);
    img.createSprite(224,304);
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(2);
    img.setFreeFont(&WildFont);
    spr_println(img,0,1,PSTR("About"),2,ALIGN_CENTER,WILD_CYAN);
    //print message
    sprintf(buf,"ESP32 AY Player v.%s",VERSION);
    spr_println(img,0,8,buf,2,ALIGN_CENTER,TFT_RED);
    spr_println(img,0,9,PSTR("by Spawn 10'24"),2,ALIGN_CENTER,TFT_YELLOW);
    spr_println(img,0,10,PSTR("powered with"),2,ALIGN_CENTER,TFT_CYAN);
    spr_println(img,0,11,PSTR("  and"),2,ALIGN_CENTER,TFT_CYAN);
    spr_println(img,0,11,PSTR("libayfly     z80emu"),2,ALIGN_CENTER,WILD_GREEN);
    img.pushSprite(8,8);
    img.deleteSprite();
  }
  //survey keypad
  if(enc.click()){
    sound_play(SFX_SELECT);
    PlayerCTRL.screen_mode=SCR_CONFIG;
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
  }
  if(dn.click()){
    sound_play(SFX_SELECT);
    PlayerCTRL.screen_mode=SCR_CONFIG;
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
  }
}

void config_reset_default_screen(){
  const char* const btns[]={"YES","NO"};
  const char* const msg="Are you shure?";
  const char* const msg2="(Player will restart)";
  if(PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]){
    PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=false;
    img.setColorDepth(8);
    img.createSprite(224,304);
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(2);
    img.setFreeFont(&WildFont);
    spr_println(img,0,1,PSTR("Config reset"),2,ALIGN_CENTER,WILD_CYAN);
    //print message
    spr_println(img,0,8,msg,2,ALIGN_CENTER,TFT_RED);
    spr_println(img,0,9,msg2,2,ALIGN_CENTER,TFT_YELLOW);
    //print buttons
    spr_draw_buttons(img,17,2,PlayerCTRL.msg_cur,btns[YES],btns[NO],2);
    img.pushSprite(8,8);
    img.deleteSprite();
  }
  //survey keypad
  if(enc.left()){
    if(PlayerCTRL.msg_cur!=YES) sound_play(SFX_MOVE);
    PlayerCTRL.msg_cur--;
    if(PlayerCTRL.msg_cur<0) PlayerCTRL.msg_cur=0;
    PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
  }
  if(enc.right()){
    if(PlayerCTRL.msg_cur!=NO) sound_play(SFX_MOVE);
    PlayerCTRL.msg_cur++;
    if(PlayerCTRL.msg_cur>1) PlayerCTRL.msg_cur=1;
    PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
  }
  if(enc.click()){
    switch(PlayerCTRL.msg_cur){
    case YES:
      sound_play(SFX_SELECT);
      config_default();
      config_save();
      ESP.restart();
      break;
    case NO:
      sound_play(SFX_CANCEL);
      PlayerCTRL.msg_cur=YES;
      PlayerCTRL.screen_mode=SCR_CONFIG;
      PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
      break;
    }
  }
  if(dn.click()){
    sound_play(SFX_CANCEL);
    PlayerCTRL.msg_cur=YES;
    PlayerCTRL.screen_mode=SCR_CONFIG;
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
  }
}

void config_screen(){
  const char* const player_sources[]={"SD","UART"};
  const char* const play_modes[]={"Once","All","Shuffle"};
  const char* const sound_vol_names[]={"Off","Min","Mid","High"};
  char buf[32];
  if(PlayerCTRL.scr_mode_update[SCR_CONFIG]){
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=false;
    int ccur=Config.cfg_cur;
    img.setColorDepth(8);
    img.createSprite(224,304);
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(1);
    img.setFreeFont(&WildFont);
    spr_println(img,0,1,PSTR("Settings"),2,ALIGN_CENTER,WILD_CYAN);
    spr_printmenu_item(img,2,2,PSTR("Player source"),WILD_CYAN_D2,ccur==0?TFT_RED:TFT_BLACK,player_sources[Config.playerSource],TFT_YELLOW);
    sprintf(buf,"%s",ay_layout_names[Config.ay_layout]);
    spr_printmenu_item(img,3,2,PSTR("Stereo"),WILD_CYAN_D2,ccur==1?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    switch(Config.ay_clock){
      case CLK_SPECTRUM: strcpy(buf,"ZX 1.77MHz");break;
      case CLK_PENTAGON: strcpy(buf,"PEN 1.75MHz");break;
      case CLK_MSX: strcpy(buf,"MSX 1.78MHz");break;
      case CLK_CPC: strcpy(buf,"CPC 1.0MHz");break;
      case CLK_ATARIST: strcpy(buf,"ST 2.0MHz");break;
    }
    spr_printmenu_item(img,4,2,PSTR("Clock"),WILD_CYAN_D2,ccur==2?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    spr_printmenu_item(img,5,2,PSTR("Play mode"),WILD_CYAN_D2,ccur==3?TFT_RED:TFT_BLACK,play_modes[Config.play_mode],TFT_YELLOW);
    sprintf(buf,"%2u%%",Config.scr_bright);
    spr_printmenu_item(img,6,2,PSTR("Scr.brightness"),WILD_CYAN_D2,ccur==4?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    if(!Config.scr_timeout){
      strcpy(buf,"Off");
    }else{
      sprintf(buf,"%2us",Config.scr_timeout);
    }
    spr_printmenu_item(img,7,2,PSTR("Scr.timeout"),WILD_CYAN_D2,ccur==5?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    spr_printmenu_item(img,8,2,PSTR("Key Sounds"),WILD_CYAN_D2,ccur==6?TFT_RED:TFT_BLACK,sound_vol_names[Config.sound_vol],TFT_YELLOW);
    spr_printmenu_item(img,9,2,PSTR("Reset to default"),WILD_CYAN_D2,ccur==7?TFT_RED:TFT_BLACK);
    spr_printmenu_item(img,10,2,PSTR("About"),WILD_CYAN_D2,ccur==8?TFT_RED:TFT_BLACK);

    img.pushSprite(8,8);
    img.deleteSprite();
  }
  // survey keypad
  if(enc.left()){
    sound_play(SFX_MOVE);
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
    Config.cfg_cur--;
    if(Config.cfg_cur<0)Config.cfg_cur=8;
  }
  if(enc.right()){
    sound_play(SFX_MOVE);
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
    Config.cfg_cur++;
    if(Config.cfg_cur>8) Config.cfg_cur=0;
  }
  if(dn.click()){
    sound_play(SFX_CANCEL);
    PlayerCTRL.screen_mode=PlayerCTRL.prev_screen_mode;
    PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
    return;
  }
  if(enc.click()){
    sound_play(SFX_SELECT);
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
    switch (Config.cfg_cur){
      case 0:
        Config.playerSource++;
        if(Config.playerSource>=PLAYER_MODE_ALL) Config.playerSource=PLAYER_MODE_SD;
        playerSourceChange();
        break;
      case 1:
        Config.ay_layout++;
        if (Config.ay_layout>=LAY_ALL) Config.ay_layout=0;
        break;
      case 2:
        switch(Config.ay_clock){
          case CLK_SPECTRUM: Config.ay_clock=CLK_PENTAGON;break;
          case CLK_PENTAGON: Config.ay_clock=CLK_MSX;break;
          case CLK_MSX: Config.ay_clock=CLK_CPC;break;
          case CLK_CPC: Config.ay_clock=CLK_ATARIST;break;
          case CLK_ATARIST: Config.ay_clock=CLK_SPECTRUM;break;
        }
        ay_set_clock(Config.ay_clock);
        break;
      case 3:
        Config.play_mode++;
        if(Config.play_mode>=PLAY_MODES_ALL) Config.play_mode=PLAY_MODE_ONE;
        break;
      case 4:
        Config.scr_bright-=10;
        if (Config.scr_bright<=0) Config.scr_bright=100;
        display_brightness(Config.scr_bright);
        break;
      case 5:
        switch(Config.scr_timeout){
          case 0: Config.scr_timeout=1;break;
          case 1: Config.scr_timeout=3;break;
          case 3: Config.scr_timeout=5;break;
          case 5: Config.scr_timeout=10;break;
          case 10: Config.scr_timeout=20;break;
          case 20: Config.scr_timeout=30;break;
          case 30: Config.scr_timeout=60;break;
          case 60: Config.scr_timeout=90;break;
          case 90: Config.scr_timeout=0;break;
        }
        break;
      case 6:
        Config.sound_vol++;
        if(Config.sound_vol>=VOL_MODES_ALL) Config.sound_vol=0;
        break;
      case 7:
        PlayerCTRL.msg_cur=NO;
        PlayerCTRL.screen_mode=SCR_RESET_CONFIG;
        PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
        break;
      case 8:
        PlayerCTRL.screen_mode=SCR_ABOUT;
        PlayerCTRL.scr_mode_update[SCR_ABOUT]=true;
        break;
    }
    config_save();
  }
}