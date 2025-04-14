#include <LittleFS.h>

bool cfgSet=false;

uint32_t getLittleFSFreeSpaceKB(){
  if(!LittleFS.begin()) return 0;
  uint32_t totalBytes=LittleFS.totalBytes();
  uint32_t usedBytes=LittleFS.usedBytes();
  uint32_t freeBytes=totalBytes-usedBytes;
  uint32_t freeKB=freeBytes/1024;
  LittleFS.end();
  return freeKB;
}

uint32_t getLittleFSTotalSpaceKB(){
  if(!LittleFS.begin()) return 0;
  uint32_t totalBytes=LittleFS.totalBytes();
  uint32_t totalKB=totalBytes/1024;
  LittleFS.end();
  return totalKB;
}

void config_default(){
  memset(&Config,0,sizeof(Config));
  Config.playerSource=PLAYER_MODE_SD;
  Config.zx_int=PENT_INT;
  Config.ay_layout=LAY_ABC;
  Config.ay_clock=CLK_PENTAGON;
  Config.volume=32;
  Config.scr_bright=100;
  Config.scr_timeout=0;
  Config.play_mode=PLAY_MODE_ALL;
  Config.modStereoSeparation=MOD_HALFSTEREO;
  Config.batCalib=0.0;
  Config.encType=EB_STEP2;
  browser_reset_directory();
}

void config_load(){
  config_default();
  LittleFS.begin(true);
  fs::File f=LittleFS.open(CFG_FILENAME,"r");
  if(f){
    if(f.size()==sizeof(Config)){
      f.readBytes((char*)&Config,sizeof(Config));
    }
    f.close();
  }else{
    if(!LittleFS.exists(CFG_FILENAME)){
      fs::File f=LittleFS.open(CFG_FILENAME,"w");
      if(f){
        f.write((const uint8_t*)&Config,sizeof(Config));
        f.close();
      }
    }
  }
}

void config_save(){
  if(xSemaphoreTake(outSemaphore,portMAX_DELAY)==pdTRUE){
    fs::File f=LittleFS.open(CFG_FILENAME,"w");
    if(f){
      f.write((const uint8_t*)&Config,sizeof(Config));
      f.close();
    }
    xSemaphoreGive(outSemaphore);
  }
}

void configResetPlayingPath(){
  strncpy(Config.play_dir,"/",sizeof(Config.play_dir)-1);
  strncpy(Config.active_dir,"/",sizeof(Config.active_dir)-1);
  strncpy(Config.prev_dir,"/",sizeof(Config.prev_dir)-1);
  Config.isPlayAYL=false;
  Config.isBrowserPlaylist=false;
  Config.play_count_files=0;
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
    sprintf(buf,"ZxPod Player v.%s",VERSION);
    spr_println(img,0,7,buf,2,ALIGN_CENTER,TFT_RED);
    spr_println(img,0,8,PSTR("by"),2,ALIGN_CENTER,TFT_CYAN);
    spr_println(img,0,9,PSTR("       ,"),2,ALIGN_LEFT,WILD_GREEN);
    spr_println(img,0,9,PSTR("  Spawn"),2,ALIGN_LEFT,TFT_YELLOW);
    spr_println(img,0,9,PSTR("Andy Karpov  "),2,ALIGN_RIGHT,TFT_YELLOW);
    spr_println(img,0,10,PSTR("04'25"),2,ALIGN_CENTER,WILD_RED);
    spr_println(img,0,11,PSTR("powered with"),2,ALIGN_CENTER,TFT_CYAN);
    spr_println(img,0,12,PSTR("libayfly, z80emu,"),2,ALIGN_CENTER,WILD_GREEN);
    spr_println(img,0,13,PSTR("ESP8266Audio"),2,ALIGN_CENTER,WILD_GREEN);
    img.pushSprite(8,8);
    img.deleteSprite();
  }
  //survey keypad
  if(enc.click()&&lcdBlackout==false){
    PlayerCTRL.screen_mode=SCR_CONFIG;
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
  }
  if(dn.click()&&lcdBlackout==false){
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
  if(enc.left()&&lcdBlackout==false){
    PlayerCTRL.msg_cur--;
    if(PlayerCTRL.msg_cur<0) PlayerCTRL.msg_cur=0;
    PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
  }
  if(enc.right()&&lcdBlackout==false){
    PlayerCTRL.msg_cur++;
    if(PlayerCTRL.msg_cur>1) PlayerCTRL.msg_cur=1;
    PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
  }
  if(enc.click()&&lcdBlackout==false){
    switch(PlayerCTRL.msg_cur){
    case YES:
      config_default();
      config_save();
      ESP.restart();
      break;
    case NO:
      PlayerCTRL.msg_cur=YES;
      PlayerCTRL.screen_mode=SCR_CONFIG;
      PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
      break;
    }
  }
  if(dn.click()&&lcdBlackout==false){
    PlayerCTRL.msg_cur=YES;
    PlayerCTRL.screen_mode=SCR_CONFIG;
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
  }
}

void config_screen(){
  const char* const player_sources[]={"SD","UART"};
  const char* const play_modes[]={"Once","All","Shuffle"};
  const char* const zx_int[]={"PENT 48.8","ZX 50.0"};
  char buf[32];
  if(PlayerCTRL.scr_mode_update[SCR_CONFIG]){
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=false;
    int8_t ccur=Config.cfg_cur;
    img.setColorDepth(8);
    img.createSprite(224,288); //224,304 without voltage debug
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(1);
    img.setFreeFont(&WildFont);
    spr_println(img,0,1,PSTR("Settings"),2,ALIGN_CENTER,WILD_CYAN);
    spr_printmenu_item(img,2,2,PSTR("Player source"),WILD_CYAN_D2,ccur==0?TFT_RED:TFT_BLACK,player_sources[Config.playerSource],TFT_YELLOW);
    spr_printmenu_item(img,3,2,PSTR("ZX INT"),WILD_CYAN_D2,ccur==1?TFT_RED:TFT_BLACK,zx_int[Config.zx_int],TFT_YELLOW);
    sprintf(buf,"%s%s%s",(cfgSet&&ccur==2)?"<":"",ay_layout_names[Config.ay_layout],(cfgSet&&ccur==2)?">":"");
    spr_printmenu_item(img,4,2,PSTR("Stereo"),(cfgSet&&ccur==2)?WILD_RED:WILD_CYAN_D2,ccur==2?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==2)?WILD_RED:TFT_YELLOW);
    switch(Config.ay_clock){
      case CLK_SPECTRUM: sprintf(buf,"%sZX 1.77MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_PENTAGON: sprintf(buf,"%sPEN 1.75MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_MSX: sprintf(buf,"%sMSX 1.78MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_CPC: sprintf(buf,"%sCPC 1.0MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_ATARIST: sprintf(buf,"%sST 2.0MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
    }
    spr_printmenu_item(img,5,2,PSTR("AY Clock"),(cfgSet&&ccur==3)?WILD_RED:WILD_CYAN_D2,ccur==3?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==3)?WILD_RED:TFT_YELLOW);
    sprintf(buf,"%s%s%s",(cfgSet&&ccur==4)?"<":"",play_modes[Config.play_mode],(cfgSet&&ccur==4)?">":"");
    spr_printmenu_item(img,6,2,PSTR("Play mode"),(cfgSet&&ccur==4)?WILD_RED:WILD_CYAN_D2,ccur==4?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==4)?WILD_RED:TFT_YELLOW);
    sprintf(buf,"%s%2u%%%s",(cfgSet&&ccur==5)?"<":"",Config.scr_bright,(cfgSet&&ccur==5)?">":"");
    spr_printmenu_item(img,7,2,PSTR("Scr.brightness"),(cfgSet&&ccur==5)?WILD_RED:WILD_CYAN_D2,ccur==5?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==5)?WILD_RED:TFT_YELLOW);
    if(!Config.scr_timeout){
      sprintf(buf,"%sOff%s",(cfgSet&&ccur==6)?"<":"",(cfgSet&&ccur==6)?">":"");
    }else{
      sprintf(buf,"%s%2us%s",(cfgSet&&ccur==6)?"<":"",Config.scr_timeout,(cfgSet&&ccur==6)?">":"");
    }
    spr_printmenu_item(img,8,2,PSTR("Scr.timeout"),(cfgSet&&ccur==6)?WILD_RED:WILD_CYAN_D2,ccur==6?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==6)?WILD_RED:TFT_YELLOW);
    switch(Config.modStereoSeparation){
      case MOD_FULLSTEREO: sprintf(buf,"%sFull Stereo%s",(cfgSet&&ccur==7)?"<":"",(cfgSet&&ccur==7)?">":"");break;
      case MOD_HALFSTEREO: sprintf(buf,"%sHalf Stereo%s",(cfgSet&&ccur==7)?"<":"",(cfgSet&&ccur==7)?">":"");break;
      case MOD_MONO: sprintf(buf,"%sMono%s",(cfgSet&&ccur==7)?"<":"",(cfgSet&&ccur==7)?">":"");break;
    }
    spr_printmenu_item(img,9,2,PSTR("DAC Pan."),(cfgSet&&ccur==7)?WILD_RED:WILD_CYAN_D2,ccur==7?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==7)?WILD_RED:TFT_YELLOW);
    sprintf(buf,"%s%.1fV%s",(cfgSet&&ccur==8)?(Config.batCalib>0.0)?"<+":"<":(Config.batCalib>0.0)?"+":"",Config.batCalib,(cfgSet&&ccur==8)?">":"");
    spr_printmenu_item(img,10,2,PSTR("Batt calib"),(cfgSet&&ccur==8)?WILD_RED:WILD_CYAN_D2,ccur==8?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==8)?WILD_RED:TFT_YELLOW);
    spr_printmenu_item(img,11,2,PSTR("Reset to default"),WILD_CYAN_D2,ccur==9?TFT_RED:TFT_BLACK);
    spr_printmenu_item(img,12,2,PSTR("About"),WILD_CYAN_D2,ccur==10?TFT_RED:TFT_BLACK);
    // Push sprite
    img.pushSprite(8,8);
    img.deleteSprite();
  }
  // voltage debug
  if(millis()-mlsV>vUp){
    static int chgP=0;
    uint64_t InVolt=0;
    //Reading from a port with averaging
    for(int i=0;i<READ_CNT;i++){
      InVolt+=analogReadMilliVolts(VOLTPIN);
    }
    InVolt=InVolt/READ_CNT;
    // printf("InVolt: %llu\n",InVolt);
    volt=((InVolt/1000.0)*VoltMult)+Config.batCalib;//+0.1;
    img.setColorDepth(8);
    img.createSprite(224,16);
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(1);
    img.setFreeFont(&WildFont);
    sprintf(buf,"%llu mV",InVolt);
    spr_printmenu_item(img,1,1,PSTR("mV on pin:"),WILD_RED,TFT_BLACK,buf,TFT_GREEN);
    sprintf(buf,"%.2f V",volt);
    spr_printmenu_item(img,2,1,PSTR("Battery voltage:"),WILD_RED,TFT_BLACK,buf,TFT_GREEN);
    img.pushSprite(8,296);
    img.deleteSprite();
    mlsV=millis();
  }
  // survey keypad
  if(enc.left()&&lcdBlackout==false){
    if(!cfgSet){
      PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
      Config.cfg_cur--;
      if(Config.cfg_cur<0)Config.cfg_cur=10;
    }else{
      switch(Config.cfg_cur){
        case 2:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.ay_layout--;
          if(Config.ay_layout<0) Config.ay_layout=LAY_ALL-1;
          break;
        case 3:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(Config.ay_clock){
            case CLK_SPECTRUM: Config.ay_clock=CLK_ATARIST;break;
            case CLK_PENTAGON: Config.ay_clock=CLK_SPECTRUM;break;
            case CLK_MSX: Config.ay_clock=CLK_PENTAGON;break;
            case CLK_CPC: Config.ay_clock=CLK_MSX;break;
            case CLK_ATARIST: Config.ay_clock=CLK_CPC;break;
          }
          ay_set_clock(Config.ay_clock);
          break;
        case 4:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.play_mode--;
          if(Config.play_mode<PLAY_MODE_ONE) Config.play_mode=PLAY_MODES_ALL-1;
          break;
        case 5:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.scr_bright-=10;
          if (Config.scr_bright<10) Config.scr_bright=100;
          display_brightness(Config.scr_bright);
          break;
        case 6:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(Config.scr_timeout){
            case 0: Config.scr_timeout=90;break;
            case 1: Config.scr_timeout=0;break;
            case 3: Config.scr_timeout=1;break;
            case 5: Config.scr_timeout=3;break;
            case 10: Config.scr_timeout=5;break;
            case 20: Config.scr_timeout=10;break;
            case 30: Config.scr_timeout=20;break;
            case 60: Config.scr_timeout=30;break;
            case 90: Config.scr_timeout=60;break;
          }
          break;
        case 7:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(Config.modStereoSeparation){
            case MOD_FULLSTEREO: Config.modStereoSeparation=MOD_MONO;break;
            case MOD_HALFSTEREO: Config.modStereoSeparation=MOD_FULLSTEREO;break;
            case MOD_MONO: Config.modStereoSeparation=MOD_HALFSTEREO;break;
          }
          if(PlayerCTRL.music_type==TYPE_MOD) setModSeparation();
          if(PlayerCTRL.music_type==TYPE_S3M) setS3mSeparation();
          break;
        case 8:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.batCalib-=0.1;
          if(Config.batCalib<-1.0) Config.batCalib=1.0;
          break;
      }
    }
  }
  if(enc.right()&&lcdBlackout==false){
    if(!cfgSet){
      PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
      Config.cfg_cur++;
      if(Config.cfg_cur>10) Config.cfg_cur=0;
    }else{
      switch(Config.cfg_cur){
        case 2:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.ay_layout++;
          if(Config.ay_layout>=LAY_ALL) Config.ay_layout=0;
          break;
        case 3:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(Config.ay_clock){
            case CLK_SPECTRUM: Config.ay_clock=CLK_PENTAGON;break;
            case CLK_PENTAGON: Config.ay_clock=CLK_MSX;break;
            case CLK_MSX: Config.ay_clock=CLK_CPC;break;
            case CLK_CPC: Config.ay_clock=CLK_ATARIST;break;
            case CLK_ATARIST: Config.ay_clock=CLK_SPECTRUM;break;
          }
          ay_set_clock(Config.ay_clock);
          break;
        case 4:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.play_mode++;
          if(Config.play_mode>=PLAY_MODES_ALL) Config.play_mode=PLAY_MODE_ONE;
          break;
        case 5:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.scr_bright+=10;
          if (Config.scr_bright>100) Config.scr_bright=10;
          display_brightness(Config.scr_bright);
          break;
        case 6:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
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
        case 7:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(Config.modStereoSeparation){
            case MOD_FULLSTEREO: Config.modStereoSeparation=MOD_HALFSTEREO;break;
            case MOD_HALFSTEREO: Config.modStereoSeparation=MOD_MONO;break;
            case MOD_MONO: Config.modStereoSeparation=MOD_FULLSTEREO;break;
          }
          if(PlayerCTRL.music_type==TYPE_MOD) setModSeparation();
          if(PlayerCTRL.music_type==TYPE_S3M) setS3mSeparation();
          break;
        case 8:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          Config.batCalib+=0.1;
          if(Config.batCalib>1.0) Config.batCalib=-1.0;
          break;
      }
    }
  }
  if(dn.click()&&lcdBlackout==false){
    cfgSet=false;
    PlayerCTRL.screen_mode=PlayerCTRL.prev_screen_mode;
    PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
    return;
  }
  if(enc.click()&&lcdBlackout==false){
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
    switch (Config.cfg_cur){
      case 0:
        Config.playerSource++;
        if(Config.playerSource>=PLAYER_MODE_ALL) Config.playerSource=PLAYER_MODE_SD;
        playerSourceChange();
        break;
      case 1:
        Config.zx_int=!Config.zx_int;
        frame_max=frameMax(PLAY_NORMAL);
        break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        cfgSet=!cfgSet;
        break;
      case 9:
        PlayerCTRL.msg_cur=NO;
        PlayerCTRL.screen_mode=SCR_RESET_CONFIG;
        PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
        break;
      case 10:
        PlayerCTRL.screen_mode=SCR_ABOUT;
        PlayerCTRL.scr_mode_update[SCR_ABOUT]=true;
        break;
    }
    config_save();
  }
}

/*
0 - EB_STEP4_LOW - active low (pull-up to VCC). Full cycle (4 phases) per click.
1 - EB_STEP4_HIGH - active high (pull-up to GND). Full cycle (4 phases) per click
2 - EB_STEP2 - half cycle (2 phases) per click (Set by default)
3 - EB_STEP1 - quarter cycle (1 phase) per click, and non-latching encoders
*/

void startUpConfig(){
  static bool scrUpdate=true;
  static bool itemSet=false;
  static int8_t ptr=0;
  static char buf[32];
  const char* const enc_types[]={"STEP 4L","STEP 4H","STEP 2","STEP 1"};
  buttonsSetup();
  TFTInit();
  show_frame();
  img.setColorDepth(8);
  img.createSprite(224,304);
  img.fillScreen(0);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(1);
  img.setFreeFont(&WildFont);
  spr_println(img,0,9,PSTR("Release button"),2,ALIGN_CENTER,WILD_RED);
  spr_println(img,0,10,PSTR("to continue!"),2,ALIGN_CENTER,WILD_RED);
  img.pushSprite(8,8);
  img.deleteSprite();
  while(digitalRead(DN_BTN)==LOW) yield();
  while(true){
    generalTick();
    if(scrUpdate){
      scrUpdate=false;
      int8_t ccur=Config.cfg_cur;
      img.setColorDepth(8);
      img.createSprite(224,304);
      img.fillScreen(0);
      img.setTextColor(TFT_WHITE);
      img.setTextSize(1);
      img.setFreeFont(&WildFont);
      spr_println(img,0,1,PSTR("Root Settings"),2,ALIGN_CENTER,WILD_CYAN);
      sprintf(buf,"%s%s%s",(ptr==0&&itemSet)?"<":"",enc_types[Config.encType],(ptr==0&&itemSet)?">":"");
      spr_printmenu_item(img,2,2,PSTR("Encoder type"),WILD_CYAN_D2,ptr==0?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
      sprintf(buf,"%s%s",(ptr==1&&itemSet)?"<Reset config":"Reset config",(ptr==1&&itemSet)?">":"");
      spr_printmenu_item(img,3,2,buf,(ptr==1&&itemSet)?TFT_YELLOW:WILD_CYAN_D2,ptr==1?TFT_RED:TFT_BLACK);
      // Legend
      if(itemSet){
        spr_println(img,0,19,PSTR("<"),2,ALIGN_LEFT,WILD_CYAN);
        spr_println(img,0,19,PSTR("Exit from item"),2,ALIGN_CENTER,WILD_CYAN);
        spr_println(img,0,19,PSTR(">"),2,ALIGN_RIGHT,WILD_CYAN);
      }else{
        spr_println(img,0,18,PSTR("Hold to exit/"),2,ALIGN_CENTER,WILD_CYAN);
        spr_println(img,0,19,PSTR("Down"),2,ALIGN_LEFT,WILD_CYAN);
        spr_println(img,0,19,PSTR("Item change"),2,ALIGN_CENTER,WILD_CYAN);
        spr_println(img,0,19,PSTR("Up"),2,ALIGN_RIGHT,WILD_CYAN);
      }
      img.pushSprite(8,8);
      img.deleteSprite();
    }
    if(enc.click()){
      itemSet=!itemSet;
      scrUpdate=true;
    }
    if(up.click()||up.holding()){
      if(itemSet){
        switch(ptr){
          case 0:
            switch(Config.encType){
              case EB_STEP4_LOW: Config.encType=EB_STEP4_HIGH;break;
              case EB_STEP4_HIGH: Config.encType=EB_STEP2;break;
              case EB_STEP2: Config.encType=EB_STEP1;break;
              case EB_STEP1: Config.encType=EB_STEP4_LOW;break;
            }
            break;
          case 1:
            config_default();
            config_save();
            ESP.restart();
            break;
        }
      }else{
        ptr++;
        if(ptr>1) ptr=0;
      }
      scrUpdate=true;
    }
    if(dn.click()||dn.holding()){
      if(itemSet){
        switch(ptr){
          case 0:
            switch(Config.encType){
              case EB_STEP1: Config.encType=EB_STEP2;break;
              case EB_STEP2: Config.encType=EB_STEP4_HIGH;break;
              case EB_STEP4_HIGH: Config.encType=EB_STEP4_LOW;break;
              case EB_STEP4_LOW: Config.encType=EB_STEP1;break;
            }
            break;
          case 1:
            config_default();
            config_save();
            ESP.restart();
            break;
        }
      }else{
        ptr--;
        if(ptr<0) ptr=1;
      }
      scrUpdate=true;
    }
    if(enc.holding()&&!itemSet){config_save(); ESP.restart(); break;} // Exit
    yield();
  }
}

void checkStartUpConfig(){
  pinMode(DN_BTN,INPUT);
  pinMode(UP_BTN, INPUT);
  if(digitalRead(DN_BTN)==LOW){
    startUpConfig();
  }
  if(digitalRead(UP_BTN)==LOW){
    config_default();
    config_save();
  }
}
