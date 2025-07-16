#include <LittleFS.h>
#include "csmos.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "Adafruit_TinyUSB.h"
Adafruit_USBD_MSC usb_msc;
bool sdMounted=false;
#endif

bool cfgSet=false;
bool sdFlag=false;
bool cfgDateTimeSet=false;

void lfs_config_default(){
  memset(&lfsConfig,0,sizeof(lfsConfig));
  lfsConfig.playerSource=PLAYER_MODE_SD;
  lfsConfig.zx_int=PENT_INT;
  lfsConfig.ay_layout=LAY_ABC;
  lfsConfig.ay_clock=CLK_PENTAGON;
  lfsConfig.volume=32;
  lfsConfig.scr_bright=50;
  lfsConfig.scr_timeout=0;
  lfsConfig.play_mode=PLAY_MODE_ALL;
  lfsConfig.modStereoSeparation=MOD_HALFSTEREO;
  lfsConfig.batCalib=0.0;
  lfsConfig.encType=EB_STEP2;
  lfsConfig.encReverse=false;
  lfsConfig.showClock=false;
}

void sd_config_default(){
  memset(&sdConfig,0,sizeof(sdConfig));
  sdConfig.volume=32;
  browser_reset_directory();
}

void sd_config_load(){
  if(foundRom){
    eeInit(eepAddress);
    uint8_t sdConfigEepromFlag=255;
    eep.read(SD_FLAG_ADDRESS,&sdConfigEepromFlag,1);
    printf("SD Config flag: %d\n", sdConfigEepromFlag);
    if(sdConfigEepromFlag==255){ // Empty config, write default
      printf("Write default sdConfig...\n");
      eep.write(SD_START_ADDRESS,reinterpret_cast<uint8_t*>(&sdConfig),sizeof(sdConfig));
      eep.write(SD_FLAG_ADDRESS,64);
    }else if(sdConfigEepromFlag==64){ // Read config
      printf("Load sdConfig from eeprom...\n");
      eep.read(SD_START_ADDRESS,reinterpret_cast<uint8_t*>(&sdConfig),sizeof(sdConfig));
    }
  }else{
    if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
      if(sd_fat.begin(SD_CONFIG)){
        bool result=false;
        uint32_t fileSize=0;
        FsFile f;
        result=f.open(CFG_FILENAME,O_RDONLY);
        if(result){
          f.read(&sdConfig,sizeof(sdConfig));
          f.close();
        }else{
          result=f.open(CFG_FILENAME,O_RDWR|O_CREAT|O_TRUNC);
          if(result){
            f.write(&sdConfig,sizeof(sdConfig));
          }
          f.close();
        }
      }
      xSemaphoreGive(sdCardSemaphore);
    }
  }
  printf("SD Config size: %d bytes.\n",sizeof(sdConfig));
}

void lfs_config_load(){
  if(foundRom){
    eeInit(eepAddress);
    uint8_t lfsConfigEepromFlag=255;
    eep.read(LFS_FLAG_ADDRESS,&lfsConfigEepromFlag,1);
    printf("LFS Config flag: %d\n", lfsConfigEepromFlag);
    if(lfsConfigEepromFlag==255){ // Empty config, write default
      printf("Write default lfsConfig...\n");
      eep.write(LFS_START_ADDRESS,reinterpret_cast<uint8_t*>(&lfsConfig),sizeof(lfsConfig));
      eep.write(LFS_FLAG_ADDRESS,64);
    }else if(lfsConfigEepromFlag==64){ // Read config
      printf("Load lfsConfig from eeprom...\n");
      eep.read(LFS_START_ADDRESS,reinterpret_cast<uint8_t*>(&lfsConfig),sizeof(lfsConfig));
    }
  }else{
    LittleFS.begin(true);
    fs::File f=LittleFS.open(CFG_FILENAME,"r");
    if(f){
      if(f.size()==sizeof(lfsConfig)){
        f.readBytes((char*)&lfsConfig,sizeof(lfsConfig));
      }
      f.close();
    }else{
      if(!LittleFS.exists(CFG_FILENAME)){
        fs::File f=LittleFS.open(CFG_FILENAME,"w");
        if(f){
          f.write((const uint8_t*)&lfsConfig,sizeof(lfsConfig));
          f.close();
        }
      }
    }
  }
  printf("LFS Config size: %d bytes.\n",sizeof(lfsConfig));
}

void startup_config_load(){
  lfs_config_default();
  lfs_config_load();
  sd_config_default();
  sd_config_load();
}

void sd_config_save(){
  if(foundRom){
    printf("Save sdConfig to eeprom...\n");
    eeInit(eepAddress);
    eep.write(SD_START_ADDRESS,reinterpret_cast<uint8_t*>(&sdConfig),sizeof(sdConfig));
    eep.write(SD_FLAG_ADDRESS,64);
  }else{
    if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
      if(sd_fat.begin(SD_CONFIG)){
        bool result=false;
        FsFile f;
        result=f.open(CFG_FILENAME,O_RDWR);
        if(result){
          size_t writtenBytes=0;
          writtenBytes=f.write(&sdConfig,sizeof(sdConfig));
        }
        f.close();
      }
      xSemaphoreGive(sdCardSemaphore);
    }
  }
}

void lfs_config_save(){
  if(foundRom){
    printf("Save lfsConfig to eeprom...\n");
    eeInit(eepAddress);
    eep.write(LFS_START_ADDRESS,reinterpret_cast<uint8_t*>(&lfsConfig),sizeof(lfsConfig));
    eep.write(LFS_FLAG_ADDRESS,64);
  }else{
    if(xSemaphoreTake(outSemaphore,portMAX_DELAY)==pdTRUE){
      fs::File f=LittleFS.open(CFG_FILENAME,"w");
      if(f){
        f.write((const uint8_t*)&lfsConfig,sizeof(lfsConfig));
        f.close();
      }
      xSemaphoreGive(outSemaphore);
    }
  }
}

void configResetPlayingPath(){
  strncpy(sdConfig.play_dir,"/",sizeof(sdConfig.play_dir)-1);
  strncpy(sdConfig.active_dir,"/",sizeof(sdConfig.active_dir)-1);
  strncpy(sdConfig.prev_dir,"/",sizeof(sdConfig.prev_dir)-1);
  sdConfig.isPlayAYL=false;
  sdConfig.isBrowserPlaylist=false;
  sdConfig.play_count_files=0;
  sdConfig.play_ayl_file[0]=0;
  sdConfig.ayl_file[0]=0;
  sdConfig.play_cur=0;
  sdConfig.dir_cur=0;
  sd_config_save();
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
    sprintf(buf,"ZxPOD Player v.%s",VERSION);
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

void time_date_screen(){
  char buf[32];
  uint16_t year=now.year();
  int8_t month=now.month();
  int8_t day=now.day();
  int8_t hour=now.hour();
  int8_t minute=now.minute();
  int8_t second=now.second();
  if(PlayerCTRL.scr_mode_update[SCR_DATETIME]){
    PlayerCTRL.scr_mode_update[SCR_DATETIME]=false;
    int8_t ccur=lfsConfig.cfg_datetime_cur;
    img.setColorDepth(8);
    img.createSprite(224,304);
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(2);
    img.setFreeFont(&WildFont);
    spr_println(img,0,1,PSTR("Date&Time"),2,ALIGN_CENTER,WILD_CYAN);
    //print message
    spr_printmenu_item(img,2,2,PSTR("Show clock:"),WILD_CYAN_D2,ccur==0?TFT_RED:TFT_BLACK,lfsConfig.showClock?PSTR("Yes"):PSTR("No"),TFT_YELLOW);
    sprintf(buf,"%s%04d%s",(cfgDateTimeSet&&ccur==1)?"<":"",year,(cfgDateTimeSet&&ccur==1)?">":"");
    spr_printmenu_item(img,3,2,PSTR("Year:"),WILD_CYAN_D2,ccur==1?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    sprintf(buf,"%s%s%s",(cfgDateTimeSet&&ccur==2)?"<":"",monthsOfTheYear[month],(cfgDateTimeSet&&ccur==2)?">":"");
    spr_printmenu_item(img,4,2,PSTR("Month:"),WILD_CYAN_D2,ccur==2?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    sprintf(buf,"%s%02d%s",(cfgDateTimeSet&&ccur==3)?"<":"",day,(cfgDateTimeSet&&ccur==3)?">":"");
    spr_printmenu_item(img,5,2,PSTR("Date:"),WILD_CYAN_D2,ccur==3?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    sprintf(buf,"%s%02d%s",(cfgDateTimeSet&&ccur==4)?"<":"",hour,(cfgDateTimeSet&&ccur==4)?">":"");
    spr_printmenu_item(img,6,2,PSTR("Hour:"),WILD_CYAN_D2,ccur==4?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    sprintf(buf,"%s%02d%s",(cfgDateTimeSet&&ccur==5)?"<":"",minute,(cfgDateTimeSet&&ccur==5)?">":"");
    spr_printmenu_item(img,7,2,PSTR("Minute:"),WILD_CYAN_D2,ccur==5?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    sprintf(buf,"%s%02d%s",(cfgDateTimeSet&&ccur==6)?"<":"",second,(cfgDateTimeSet&&ccur==6)?">":"");
    spr_printmenu_item(img,8,2,PSTR("Second:"),WILD_CYAN_D2,ccur==6?TFT_RED:TFT_BLACK,buf,TFT_YELLOW);
    //formatted date and time
    sprintf(buf, "%02d %s %04d",now.day(),monthsOfTheYear[now.month()],now.year());
    spr_println(img,0,11,buf,2,ALIGN_CENTER,TFT_YELLOW);
    sprintf(buf,"%s",daysOfTheWeek[now.dayOfTheWeek()]);
    spr_println(img,0,12,buf,2,ALIGN_CENTER,TFT_YELLOW);
    sprintf(buf,"%02d:%02d:%02d",now.hour(),now.minute(),now.second());
    spr_println(img,0,13,buf,2,ALIGN_CENTER,TFT_YELLOW);
    img.pushSprite(8,8);
    img.deleteSprite();
  }
  //survey keypad
  if(enc.left()&&lcdBlackout==false){
    PlayerCTRL.scr_mode_update[SCR_DATETIME]=true;
    if(!cfgDateTimeSet){
      lfsConfig.cfg_datetime_cur--;
      if(lfsConfig.cfg_datetime_cur<0) lfsConfig.cfg_datetime_cur=6;
    }else{
      switch(lfsConfig.cfg_datetime_cur){
        case 1:
          year--;
          if(year<1970) year=1970;
          break;
        case 2:
          month--;
          if(month<1) month=12;
          break;
        case 3:
          day--;
          if(day<1) day=31;
          break;
        case 4:
          hour--;
          if(hour<0) hour=23;
          break;
        case 5:
          minute--;
          if(minute<0) minute=59;
          break;
        case 6:
          second--;
          if(second<0) second=59;
          break;
      }
      rtc.adjust(DateTime(year,month,day,hour,minute,second));
    }
  }
  if(enc.right()&&lcdBlackout==false){
    if(!cfgDateTimeSet){
      lfsConfig.cfg_datetime_cur++;
      if(lfsConfig.cfg_datetime_cur>6) lfsConfig.cfg_datetime_cur=0;
    }else{
      switch(lfsConfig.cfg_datetime_cur){
        case 1:
          year++;
          if(year>2099) year=2099;
          break;
        case 2:
          month++;
          if(month>12) month=1;
          break;
        case 3:
          day++;
          if(day>31) day=1;
          break;
        case 4:
          hour++;
          if(hour>23) hour=0;
          break;
        case 5:
          minute++;
          if(minute>59) minute=0;
          break;
        case 6:
          second++;
          if(second>59) second=0;
          break;
      }
      rtc.adjust(DateTime(year,month,day,hour,minute,second));
    }
    PlayerCTRL.scr_mode_update[SCR_DATETIME]=true;
  }
  if(enc.click()&&lcdBlackout==false){
    PlayerCTRL.scr_mode_update[SCR_DATETIME]=true;
    switch(lfsConfig.cfg_datetime_cur){
      case 0:
        lfsConfig.showClock=!lfsConfig.showClock;
        lfs_config_save();
        break;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
        cfgDateTimeSet=!cfgDateTimeSet;
        break;
    }
  }
  if(dn.click()&&lcdBlackout==false){
    PlayerCTRL.screen_mode=SCR_CONFIG;
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
  }
  //update screen if second change
  static uint8_t lastSecond=0;
  if(now.second()!=lastSecond){
    lastSecond=now.second();
    PlayerCTRL.scr_mode_update[SCR_DATETIME]=true;
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
      lfs_config_default();
      lfs_config_save();
      sd_config_default();
      sd_config_save();
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
  const char* const enc_reverse[]={"NORMAL","REVERSE"};
  char buf[32];
  if(PlayerCTRL.scr_mode_update[SCR_CONFIG]){
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=false;
    int8_t ccur=lfsConfig.cfg_cur;
    img.setColorDepth(8);
    img.createSprite(224,288); //224,304 without voltage debug
    img.fillScreen(0);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(1);
    img.setFreeFont(&WildFont);
    spr_println(img,0,1,PSTR("Settings"),2,ALIGN_CENTER,WILD_CYAN);
    spr_printmenu_item(img,2,2,PSTR("Player source"),WILD_CYAN_D2,ccur==0?TFT_RED:TFT_BLACK,player_sources[lfsConfig.playerSource],TFT_YELLOW);
    spr_printmenu_item(img,3,2,PSTR("ZX INT"),WILD_CYAN_D2,ccur==1?TFT_RED:TFT_BLACK,zx_int[lfsConfig.zx_int],TFT_YELLOW);
    sprintf(buf,"%s%s%s",(cfgSet&&ccur==2)?"<":"",ay_layout_names[lfsConfig.ay_layout],(cfgSet&&ccur==2)?">":"");
    spr_printmenu_item(img,4,2,PSTR("Stereo"),(cfgSet&&ccur==2)?WILD_RED:WILD_CYAN_D2,ccur==2?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==2)?WILD_RED:TFT_YELLOW);
    switch(lfsConfig.ay_clock){
      case CLK_SPECTRUM: sprintf(buf,"%sZX 1.77MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_PENTAGON: sprintf(buf,"%sPEN 1.75MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_MSX: sprintf(buf,"%sMSX 1.78MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_CPC: sprintf(buf,"%sCPC 1.0MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
      case CLK_ATARIST: sprintf(buf,"%sST 2.0MHz%s",(cfgSet&&ccur==3)?"<":"",(cfgSet&&ccur==3)?">":"");break;
    }
    spr_printmenu_item(img,5,2,PSTR("AY Clock"),(cfgSet&&ccur==3)?WILD_RED:WILD_CYAN_D2,ccur==3?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==3)?WILD_RED:TFT_YELLOW);
    sprintf(buf,"%s%s%s",(cfgSet&&ccur==4)?"<":"",play_modes[lfsConfig.play_mode],(cfgSet&&ccur==4)?">":"");
    spr_printmenu_item(img,6,2,PSTR("Play mode"),(cfgSet&&ccur==4)?WILD_RED:WILD_CYAN_D2,ccur==4?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==4)?WILD_RED:TFT_YELLOW);
    sprintf(buf,"%s%2u%%%s",(cfgSet&&ccur==5)?"<":"",lfsConfig.scr_bright,(cfgSet&&ccur==5)?">":"");
    spr_printmenu_item(img,7,2,PSTR("Scr.brightness"),(cfgSet&&ccur==5)?WILD_RED:WILD_CYAN_D2,ccur==5?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==5)?WILD_RED:TFT_YELLOW);
    if(!lfsConfig.scr_timeout){
      sprintf(buf,"%sOff%s",(cfgSet&&ccur==6)?"<":"",(cfgSet&&ccur==6)?">":"");
    }else{
      sprintf(buf,"%s%2us%s",(cfgSet&&ccur==6)?"<":"",lfsConfig.scr_timeout,(cfgSet&&ccur==6)?">":"");
    }
    spr_printmenu_item(img,8,2,PSTR("Scr.timeout"),(cfgSet&&ccur==6)?WILD_RED:WILD_CYAN_D2,ccur==6?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==6)?WILD_RED:TFT_YELLOW);
    switch(lfsConfig.modStereoSeparation){
      case MOD_FULLSTEREO: sprintf(buf,"%sFull Stereo%s",(cfgSet&&ccur==7)?"<":"",(cfgSet&&ccur==7)?">":"");break;
      case MOD_HALFSTEREO: sprintf(buf,"%sHalf Stereo%s",(cfgSet&&ccur==7)?"<":"",(cfgSet&&ccur==7)?">":"");break;
      case MOD_MONO: sprintf(buf,"%sMono%s",(cfgSet&&ccur==7)?"<":"",(cfgSet&&ccur==7)?">":"");break;
    }
    spr_printmenu_item(img,9,2,PSTR("DAC Pan."),(cfgSet&&ccur==7)?WILD_RED:WILD_CYAN_D2,ccur==7?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==7)?WILD_RED:TFT_YELLOW);
    spr_printmenu_item(img,10,2,PSTR("Enc direction"),WILD_CYAN_D2,ccur==8?TFT_RED:TFT_BLACK,enc_reverse[lfsConfig.encReverse],TFT_YELLOW);
    sprintf(buf,"%s%.1fV%s",(cfgSet&&ccur==9)?(lfsConfig.batCalib>0.0)?"<+":"<":(lfsConfig.batCalib>0.0)?"+":"",lfsConfig.batCalib,(cfgSet&&ccur==9)?">":"");
    spr_printmenu_item(img,11,2,PSTR("Batt calib"),(cfgSet&&ccur==9)?WILD_RED:WILD_CYAN_D2,ccur==9?(cfgSet)?TFT_GREEN:TFT_RED:TFT_BLACK,buf,(cfgSet&&ccur==9)?WILD_RED:TFT_YELLOW);
    if(foundRtc) spr_printmenu_item(img,12,2,PSTR("Date&Time"),WILD_CYAN_D2,ccur==10?TFT_RED:TFT_BLACK);
    spr_printmenu_item(img,(foundRtc)?13:12,2,PSTR("Reset to default"),WILD_CYAN_D2,ccur==11?TFT_RED:TFT_BLACK);
    spr_printmenu_item(img,(foundRtc)?14:13,2,PSTR("About"),WILD_CYAN_D2,ccur==12?TFT_RED:TFT_BLACK);
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
    volt=((InVolt/1000.0)*VoltMult)+lfsConfig.batCalib;//+0.1;
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
      lfsConfig.cfg_cur--;
      if(!foundRtc){
        if(lfsConfig.cfg_cur==10) lfsConfig.cfg_cur=9;
      }
      if(lfsConfig.cfg_cur<0)lfsConfig.cfg_cur=12;
    }else{
      switch(lfsConfig.cfg_cur){
        case 2:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.ay_layout--;
          if(lfsConfig.ay_layout<0) lfsConfig.ay_layout=LAY_ALL-1;
          break;
        case 3:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(lfsConfig.ay_clock){
            case CLK_SPECTRUM: lfsConfig.ay_clock=CLK_ATARIST;break;
            case CLK_PENTAGON: lfsConfig.ay_clock=CLK_SPECTRUM;break;
            case CLK_MSX: lfsConfig.ay_clock=CLK_PENTAGON;break;
            case CLK_CPC: lfsConfig.ay_clock=CLK_MSX;break;
            case CLK_ATARIST: lfsConfig.ay_clock=CLK_CPC;break;
          }
          ay_set_clock(lfsConfig.ay_clock);
          break;
        case 4:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.play_mode--;
          if(lfsConfig.play_mode<PLAY_MODE_ONE) lfsConfig.play_mode=PLAY_MODES_ALL-1;
          break;
        case 5:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.scr_bright-=10;
          if(lfsConfig.scr_bright<10) lfsConfig.scr_bright=100;
          display_brightness(lfsConfig.scr_bright);
          break;
        case 6:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(lfsConfig.scr_timeout){
            case 0: lfsConfig.scr_timeout=90;break;
            case 1: lfsConfig.scr_timeout=0;break;
            case 3: lfsConfig.scr_timeout=1;break;
            case 5: lfsConfig.scr_timeout=3;break;
            case 10: lfsConfig.scr_timeout=5;break;
            case 20: lfsConfig.scr_timeout=10;break;
            case 30: lfsConfig.scr_timeout=20;break;
            case 60: lfsConfig.scr_timeout=30;break;
            case 90: lfsConfig.scr_timeout=60;break;
          }
          break;
        case 7:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(lfsConfig.modStereoSeparation){
            case MOD_FULLSTEREO: lfsConfig.modStereoSeparation=MOD_MONO;break;
            case MOD_HALFSTEREO: lfsConfig.modStereoSeparation=MOD_FULLSTEREO;break;
            case MOD_MONO: lfsConfig.modStereoSeparation=MOD_HALFSTEREO;break;
          }
          if(PlayerCTRL.music_type==TYPE_MOD) setModSeparation();
          if(PlayerCTRL.music_type==TYPE_S3M) setS3mSeparation();
          break;
        case 9:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.batCalib-=0.1;
          if(lfsConfig.batCalib<-1.0) lfsConfig.batCalib=1.0;
          break;
      }
    }
  }
  if(enc.right()&&lcdBlackout==false){
    if(!cfgSet){
      PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
      lfsConfig.cfg_cur++;
      if(!foundRtc){
        if(lfsConfig.cfg_cur==10) lfsConfig.cfg_cur=11;
      }
      if(lfsConfig.cfg_cur>12) lfsConfig.cfg_cur=0;
    }else{
      switch(lfsConfig.cfg_cur){
        case 2:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.ay_layout++;
          if(lfsConfig.ay_layout>=LAY_ALL) lfsConfig.ay_layout=0;
          break;
        case 3:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(lfsConfig.ay_clock){
            case CLK_SPECTRUM: lfsConfig.ay_clock=CLK_PENTAGON;break;
            case CLK_PENTAGON: lfsConfig.ay_clock=CLK_MSX;break;
            case CLK_MSX: lfsConfig.ay_clock=CLK_CPC;break;
            case CLK_CPC: lfsConfig.ay_clock=CLK_ATARIST;break;
            case CLK_ATARIST: lfsConfig.ay_clock=CLK_SPECTRUM;break;
          }
          ay_set_clock(lfsConfig.ay_clock);
          break;
        case 4:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.play_mode++;
          if(lfsConfig.play_mode>=PLAY_MODES_ALL) lfsConfig.play_mode=PLAY_MODE_ONE;
          break;
        case 5:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.scr_bright+=10;
          if (lfsConfig.scr_bright>100) lfsConfig.scr_bright=10;
          display_brightness(lfsConfig.scr_bright);
          break;
        case 6:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(lfsConfig.scr_timeout){
            case 0: lfsConfig.scr_timeout=1;break;
            case 1: lfsConfig.scr_timeout=3;break;
            case 3: lfsConfig.scr_timeout=5;break;
            case 5: lfsConfig.scr_timeout=10;break;
            case 10: lfsConfig.scr_timeout=20;break;
            case 20: lfsConfig.scr_timeout=30;break;
            case 30: lfsConfig.scr_timeout=60;break;
            case 60: lfsConfig.scr_timeout=90;break;
            case 90: lfsConfig.scr_timeout=0;break;
          }
          break;
        case 7:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          switch(lfsConfig.modStereoSeparation){
            case MOD_FULLSTEREO: lfsConfig.modStereoSeparation=MOD_HALFSTEREO;break;
            case MOD_HALFSTEREO: lfsConfig.modStereoSeparation=MOD_MONO;break;
            case MOD_MONO: lfsConfig.modStereoSeparation=MOD_FULLSTEREO;break;
          }
          if(PlayerCTRL.music_type==TYPE_MOD) setModSeparation();
          if(PlayerCTRL.music_type==TYPE_S3M) setS3mSeparation();
          break;
        case 9:
          PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
          lfsConfig.batCalib+=0.1;
          if(lfsConfig.batCalib>1.0) lfsConfig.batCalib=-1.0;
          break;
      }
    }
  }
  if(dn.click()&&lcdBlackout==false){ //  Exit from menu
    cfgSet=false;
    PlayerCTRL.screen_mode=PlayerCTRL.prev_screen_mode;
    PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
    lfs_config_save();
    sdFlag=true;
    return;
  }
  if(enc.click()&&lcdBlackout==false){
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=true;
    switch(lfsConfig.cfg_cur){
      case 0:
      lfsConfig.playerSource++;
        if(lfsConfig.playerSource>=PLAYER_MODE_ALL) lfsConfig.playerSource=PLAYER_MODE_SD;
        playerSourceChange();
        break;
      case 1:
        lfsConfig.zx_int=!lfsConfig.zx_int;
        frame_max=frameMax(PLAY_NORMAL);
        break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 9:
        cfgSet=!cfgSet;
        break;
      case 8:
        lfsConfig.encReverse=!lfsConfig.encReverse;
        enc.setEncReverse(lfsConfig.encReverse);
        break;
      case 10:
        PlayerCTRL.screen_mode=SCR_DATETIME;
        PlayerCTRL.scr_mode_update[SCR_DATETIME]=true;
        break;
      case 11:
        PlayerCTRL.msg_cur=NO;
        PlayerCTRL.screen_mode=SCR_RESET_CONFIG;
        PlayerCTRL.scr_mode_update[SCR_RESET_CONFIG]=true;
        break;
      case 12:
        PlayerCTRL.screen_mode=SCR_ABOUT;
        PlayerCTRL.scr_mode_update[SCR_ABOUT]=true;
        break;
    }
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
      img.setColorDepth(8);
      img.createSprite(224,304);
      img.fillScreen(0);
      img.setTextColor(TFT_WHITE);
      img.setTextSize(1);
      img.setFreeFont(&WildFont);
      spr_println(img,0,1,PSTR("Root Settings"),2,ALIGN_CENTER,WILD_CYAN);
      sprintf(buf,"%s%s%s",(ptr==0&&itemSet)?"<":"",enc_types[lfsConfig.encType],(ptr==0&&itemSet)?">":"");
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
            switch(lfsConfig.encType){
              case EB_STEP4_LOW: lfsConfig.encType=EB_STEP4_HIGH;break;
              case EB_STEP4_HIGH: lfsConfig.encType=EB_STEP2;break;
              case EB_STEP2: lfsConfig.encType=EB_STEP1;break;
              case EB_STEP1: lfsConfig.encType=EB_STEP4_LOW;break;
            }
            break;
          case 1:
            lfs_config_default();
            lfs_config_save();
            sd_config_default();
            sd_config_save();
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
            switch(lfsConfig.encType){
              case EB_STEP1: lfsConfig.encType=EB_STEP2;break;
              case EB_STEP2: lfsConfig.encType=EB_STEP4_HIGH;break;
              case EB_STEP4_HIGH: lfsConfig.encType=EB_STEP4_LOW;break;
              case EB_STEP4_LOW: lfsConfig.encType=EB_STEP1;break;
            }
            break;
          case 1:
            lfs_config_default();
            lfs_config_save();
            sd_config_default();
            sd_config_save();
            ESP.restart();
            break;
        }
      }else{
        ptr--;
        if(ptr<0) ptr=1;
      }
      scrUpdate=true;
    }
    if(enc.holding()&&!itemSet){lfs_config_save(); ESP.restart(); break;} // Exit
    yield();
  }
}

//ESP32S3 usb <--> SD mass storage device
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb(uint32_t lba,void* buffer,uint32_t bufsize){
  bool rc;
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    rc=sd_fat.card()->readSectors(lba,(uint8_t*)buffer,bufsize/512);
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  return rc?bufsize:-1;
}
// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb(uint32_t lba,uint8_t* buffer,uint32_t bufsize){
  bool rc;
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    rc=sd_fat.card()->writeSectors(lba,buffer,bufsize/512);
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  return rc?bufsize:-1;
}
// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb(void){
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    sd_fat.card()->syncDevice();
    //sd_fat.cacheClear();
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
}

void mountSD(){
  if(sd_fat.begin(SD_CONFIG)&&!sdMounted){
    uint32_t block_count=sd_fat.card()->sectorCount();
    usb_msc.setCapacity(0,block_count,512);
    usb_msc.setUnitReady(0,true);
    sdMounted=true;
  }
}

void umountSD(){
  usb_msc.setUnitReady(0,false);
  sdMounted=false;
}

void massStorage(){
  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if(!TinyUSBDevice.isInitialized()){
    TinyUSBDevice.begin(1);
  }
  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("ZxPOD","SD <-->","1.0");
  usb_msc.setReadWriteCallback(0,msc_read_cb,msc_write_cb,msc_flush_cb);
  mountSD();
  usb_msc.begin();
  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if(TinyUSBDevice.mounted()){
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}
#endif

void checkStartUpConfig(){
  // TinyUSBDevice.begin(2);
  pinMode(DN_BTN,INPUT);
  pinMode(UP_BTN,INPUT);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // pinMode(OK_BTN,INPUT);
#endif
  if(digitalRead(DN_BTN)==LOW){
    startUpConfig();
  }
  if(digitalRead(UP_BTN)==LOW){
    printf("RESET ALL CMOS....\n");
    if(foundRom){
      eeInit(eepAddress);
      eeErase(CHUNK_SIZE,0,(totalKBytes*1024)-1);
    }
    lfs_config_default();
    lfs_config_save();
    sd_config_default();
    sd_config_save();
  }
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    // Mount SD card to USB
    massStorage();
  #endif
}
