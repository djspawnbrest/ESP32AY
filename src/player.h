static bool dynRebuild=true;
static bool colCh2=false;
int ay_cur_song=0;
uint16_t prev_file_id=0;

void player_shuffle_cur(){
  int prev_cur=Config.play_cur;
  if(Config.play_count_files<=0) return;
  while(1){
    // randomSeed(analogRead(VOLTPIN));
    Config.play_cur=random(Config.play_cur_start,Config.play_count_files);
    if(Config.play_cur!=prev_cur) break;
    yield();
  }
}

void muteAYBeep(){
  sound_clear_buf();
  for (uint8_t chip=0; chip<2;chip++){
    ay_write(chip,0x07,0xff);
    ay_write(chip,0x08,0x00);
    ay_write(chip,0x09,0x00);
    ay_write(chip,0x0a,0x00);
  }
}
//subsong is only used for AY format
int music_open(const char* filename,int ay_sub_song){
  int err=FILE_ERR_OTHER;
  if(!sd_fat.begin(SD_CONFIG)) return FILE_ERR_NO_CARD;
  if(sd_play_file.open(filename, O_RDONLY)){
    sd_play_file.getName(lfn,sizeof(lfn));
    memcpy(playedFileName,lfn,sizeof(lfn));
    PlayerCTRL.music_type=browser_check_ext(lfn);
    switch(PlayerCTRL.music_type){
      case TYPE_UNK: break;
      case TYPE_AYL: break;
      case TYPE_PT1:
      case TYPE_PT2:
      case TYPE_PT3:
      case TYPE_STC:
      case TYPE_STP:
      case TYPE_ASC:
      case TYPE_PSC:
      case TYPE_SQT:
        memset(&AYInfo,0,sizeof(AYInfo));
        music_data_size=sd_play_file.fileSize();
        if(music_data_size<=sizeof(music_data)){
          sd_play_file.read((char*)music_data,music_data_size);
          err=FILE_ERR_NONE;
        }else{
          err=FILE_ERR_TOO_LARGE;
        }
        sd_play_file.close();
        break;
      case TYPE_AY:
        memset(&AYInfo,0,sizeof(AYInfo));
        err=AY_Load(AYInfo,ay_sub_song); //uses currently opened file stream
        sd_play_file.close();
        break;
      case TYPE_PSG:
        memset(&music_data,0,sizeof(music_data));
        music_data_size=sd_play_file.fileSize();
        memset(&AYInfo,0,sizeof(AYInfo));
        AYInfo.Length=0;
        AYInfo.Loop=0;
        sd_play_file.read(fileInfoBuf,16);
        memcpy(&AYInfo.Length,&fileInfoBuf[8],sizeof(unsigned long));
        //check if present frames count info else calculate and write to file
        if(AYInfo.Length==0){
          sd_play_file.close();
          sd_play_file.open(filename, O_RDWR);
          bool fe=false;
          while(sd_play_file.available()){
            byte b=sd_play_file.read();
            switch(b){
              case 0xFF:
                AYInfo.Length++;
                break;
              case 0xFE:
                fe=true;
                break;
              case 0xFD:
                break;
              default:
                if(fe){AYInfo.Length+=b*4-1;fe=false;break;}
                break;
            }
          }
          sd_play_file.rewind();
          sd_play_file.read(fileInfoBuf,8);
          sd_play_file.write(&AYInfo.Length,sizeof(AYInfo.Length));
          sd_play_file.close();
          sd_play_file.open(filename, O_RDONLY);
        }
        sd_play_file.rewind();
        //skip 16 info bytes
        sd_play_file.seek(16);
        memcpy(&AYInfo.Name,utf8rus(playedFileName),utf8_strlen(playedFileName)-4);
        fillBuffer();
        err=FILE_ERR_NONE;
        break;
      case TYPE_RSF:
        memset(&music_data,0,sizeof(music_data));
        music_data_size=sd_play_file.fileSize();
        memset(&AYInfo,0,sizeof(AYInfo));
        uint16_t freq,offset,loop;
        if(sd_play_file.read(fileInfoBuf,4)<=0)break; // short file
        //if(fileInfoBuf[0]!='R'||fileInfoBuf[1]!='S'||fileInfoBuf[2]!='F')return; //not RSF
        switch(fileInfoBuf[3]){ // reading RSF HEADER v3 only supported!
          case 3: // RSF ver.3
            if(sd_play_file.read(fileInfoBuf,14)==0)break; // short file
            memcpy(&freq,&fileInfoBuf[0],sizeof(uint16_t));
            memcpy(&offset,&fileInfoBuf[2],sizeof(uint16_t));
            memcpy(&AYInfo.Length,&fileInfoBuf[4],sizeof(uint32_t));
            memcpy(&AYInfo.Loop,&fileInfoBuf[8],sizeof(uint32_t));
            AYInfo.Loop=0;
            sd_play_file.rewind();
            sd_play_file.seek(20);
            if(sd_play_file.read(fileInfoBuf,offset-20)==0)break; // short file
            uint16_t offsetString=0; //20 -start position for strings - name, author, comment
            uint8_t offsetBuf[4];
            uint16_t bufPos = 0;
            offsetBuf[bufPos]=0;
            while(offsetString<=offset-21){
              if(fileInfoBuf[offsetString]==0x00){
                bufPos++;
                offsetBuf[bufPos]=offsetString+1;
              }
              offsetString++;
            }
            memcpy(&AYInfo.Name,&fileInfoBuf[offsetBuf[0]],offsetBuf[1]);
            memcpy(&AYInfo.Author,&fileInfoBuf[offsetBuf[1]],offsetBuf[2]-offsetBuf[1]);
            break;
        }
        sd_play_file.rewind();
        sd_play_file.seek(offset);
        fillBuffer();
        err=FILE_ERR_NONE;
        break;
      case TYPE_YRG:
        yrgFrame=0;
        memset(&music_data,0,sizeof(music_data));
        music_data_size=sd_play_file.fileSize();
        memset(&AYInfo,0,sizeof(AYInfo));
        AYInfo.Loop=0;
        AYInfo.Length=(unsigned long)(music_data_size/16);
        memcpy(&AYInfo.Name,utf8rus(playedFileName),utf8_strlen(playedFileName)-4);
        fillBuffer();
        err=FILE_ERR_NONE;
        break;
    }
  }
  return err;
}

bool player_full_path(int cur, char* path, int path_size){
  char temp[MAX_PATH]={0};
  if(cur<0||cur>=Config.play_count_files) return false;
  if(sd_play_dir.open(Config.play_dir,O_RDONLY)){
    if(sd_play_file.open(&sd_play_dir,sort_list_play[cur].file_id,O_RDONLY)){
      sd_play_file.getName(temp,sizeof(temp));
      sd_play_file.close();
    }
    temp[sizeof(temp)-1]=0;
    sd_play_dir.close();
    strncpy(path,Config.play_dir,path_size-1);
    strncat(path,temp,path_size-1);
    return true;
  }
  path[0]=0;
  return false;
}

int checkSDonStart(){
  frame_cnt=0;
  int err=FILE_ERR_OTHER;
  while(!sd_fat.begin(SD_CONFIG)){ // while
    sdEject();
    // return FILE_ERR_NO_CARD;
  }
  PlayerCTRL.screen_mode=SCR_PLAYER;
  PlayerCTRL.isSDeject=false;
  if(Config.isPlayAYL){ // check if last played playlist exists
    if(sd_play_dir.open(Config.play_ayl_file,O_RDONLY)){
      sd_play_dir.close();
      if(!playlist_open(Config.ayl_file,0,true)) return FILE_ERR_NO_CARD;
      sort_list_len=0;
      while(playlist_iterate(lfn,sizeof(lfn),true)){
        sort_list_len++;
      }
      playlist_close();
      Config.play_count_files=sort_list_len;
      Config.play_cur_start=0;
      sort_list_len=0;
      memcpy(sort_list_play,sort_list,sizeof(sort_list));
      playlist_get_entry_full_path(Config.play_cur,lfn,sizeof(lfn),true);
      if(sd_play_file.open(lfn,O_RDONLY)){
        sd_play_file.close();
        //now prepare playing file
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isBrowserCommand=true;
        return FILE_ERR_NONE;
      }else{
        // reseting to root dir
        configResetPlayingPath();
        //now prepare default playing file
        browser_build_list(true);
        memcpy(sort_list_play,sort_list,sizeof(sort_list));
        Config.play_count_files=sort_list_len;
        Config.play_cur_start=cursor_offset;
        Config.play_cur=Config.dir_cur;
        player_full_path(Config.play_cur,lfn,sizeof(lfn));
        if(sd_play_file.open(lfn,O_RDONLY)){
          sd_play_file.close();
          PlayerCTRL.autoPlay=false;
          PlayerCTRL.isBrowserCommand=true;
          return FILE_ERR_NONE;
        }
      }
    }else{
      // reset to root dir
      configResetPlayingPath();
      //now prepare default playing file
      browser_build_list(true);
      memcpy(sort_list_play,sort_list,sizeof(sort_list));
      Config.play_count_files=sort_list_len;
      Config.play_cur_start=cursor_offset;
      Config.play_cur=Config.dir_cur;
      player_full_path(Config.play_cur,lfn,sizeof(lfn));
      if(sd_play_file.open(lfn,O_RDONLY)){
        sd_play_file.close();
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isBrowserCommand=true;
        return FILE_ERR_NONE;
      }
    }
  }else{  // check if last played dir exists
    char buf[MAX_PATH];
    strncpy(buf,Config.play_dir,sizeof(Config.play_dir)-1);
    if(sd_play_dir.open(Config.play_dir,O_RDONLY)){
      sd_play_dir.close();
      //now prepare playing file
      browser_build_list(true);
      memcpy(sort_list_play,sort_list,sizeof(sort_list));
      Config.play_count_files=sort_list_len;
      Config.play_cur_start=cursor_offset;
      player_full_path(Config.play_cur,lfn,sizeof(lfn));
      if(sd_play_file.open(lfn,O_RDONLY)){
        sd_play_file.close();
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isBrowserCommand=true;
        return FILE_ERR_NONE;
      }else{
        // reseting
        configResetPlayingPath();
        //now prepare default playing file
        browser_build_list(true);
        memcpy(sort_list_play,sort_list,sizeof(sort_list));
        Config.play_count_files=sort_list_len;
        Config.play_cur_start=cursor_offset;
        Config.play_cur=Config.dir_cur;
        player_full_path(Config.play_cur,lfn,sizeof(lfn));
        if(sd_play_file.open(lfn,O_RDONLY)){
          sd_play_file.close();
          PlayerCTRL.autoPlay=false;
          PlayerCTRL.isBrowserCommand=true;
          return FILE_ERR_NONE;
        }
        return FILE_ERR_NONE;
      }
    }else{
      configResetPlayingPath();
      //now prepare default playing file
      browser_build_list(true);
      memcpy(sort_list_play,sort_list,sizeof(sort_list));
      Config.play_count_files=sort_list_len;
      Config.play_cur_start=cursor_offset;
      Config.play_cur=Config.dir_cur;
      player_full_path(Config.play_cur,lfn,sizeof(lfn));
      if(sd_play_file.open(lfn,O_RDONLY)){
        sd_play_file.close();
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isBrowserCommand=true;
        return FILE_ERR_NONE;
      }
      return FILE_ERR_NONE;
    }
  }
  return err;
}

void music_init(){
  AYInfo.module=music_data;
  AYInfo.module_len=music_data_size;
  AYInfo.file_data=music_data;
  AYInfo.file_len=music_data_size;

  switch(PlayerCTRL.music_type){
    case TYPE_PT1: PT1_Init(AYInfo); PT1_GetInfo(AYInfo); break;
    case TYPE_PT2: PT2_Init(AYInfo); PT2_GetInfo(AYInfo); break;
    case TYPE_PT3: PT3_Init(AYInfo); PT3_GetInfo(AYInfo); break;
    case TYPE_STC: STC_Init(AYInfo); STC_GetInfo(AYInfo); break;
    case TYPE_STP: STP_Init(AYInfo); STP_GetInfo(AYInfo); break;
    case TYPE_ASC: ASC_Init(AYInfo); ASC_GetInfo(AYInfo); break;
    case TYPE_PSC: PSC_Init(AYInfo); PSC_GetInfo(AYInfo); break;
    case TYPE_SQT: SQT_Init(AYInfo); SQT_GetInfo(AYInfo); break;
    case TYPE_AY: AY_Init(AYInfo); AY_GetInfo(AYInfo); break;
    case TYPE_PSG: break;
    case TYPE_RSF: break;
    case TYPE_YRG: break;
  }
}

void music_play(){
  if(Config.play_mode==PLAY_MODE_ONE){
    if(PlayerCTRL.music_type!=TYPE_PSG&&PlayerCTRL.music_type!=TYPE_RSF&&PlayerCTRL.music_type!=TYPE_YRG&&PlayerCTRL.music_type!=TYPE_AY){
      if(PlayerCTRL.trackFrame>=AYInfo.Length){
        if(AYInfo.Loop>0){
          PlayerCTRL.trackFrame=AYInfo.Loop;
        }else{
          PlayerCTRL.trackFrame-=AYInfo.Length; // =0
        }
      }
    }else{
      if(PlayerCTRL.trackFrame>=AYInfo.Length){
        PlayerCTRL.isFinish=true;
        return;
      }
    }
  }else{
    if(PlayerCTRL.trackFrame>=AYInfo.Length){
      PlayerCTRL.isFinish=true;
      return;
    }
  }
  switch(PlayerCTRL.music_type){
    case TYPE_PT1: PT1_Play(AYInfo); break;
    case TYPE_PT2: PT2_Play(AYInfo); break;
    case TYPE_PT3: PT3_Play(AYInfo); break;
    case TYPE_STC: STC_Play(AYInfo); break;
    case TYPE_STP: STP_Play(AYInfo); break;
    case TYPE_ASC: ASC_Play(AYInfo); break;
    case TYPE_PSC: PSC_Play(AYInfo); break;
    case TYPE_SQT: SQT_Play(AYInfo); break;
    case TYPE_AY: break; //AY files play differently, they're running Z80 emulation tied to the sound ISR
    case TYPE_PSG: PSG_Play(); break;
    case TYPE_RSF: RSF_Play(); break;
    case TYPE_YRG: YRG_Play(); break;
  }
}

void music_stop(){
  ay_reset();
  switch(PlayerCTRL.music_type){
    case TYPE_PT1: PT1_Cleanup(AYInfo); break;
    case TYPE_PT2: PT2_Cleanup(AYInfo); break;
    case TYPE_PT3: PT3_Cleanup(AYInfo); break;
    case TYPE_STC: STC_Cleanup(AYInfo); break;
    case TYPE_STP: STP_Cleanup(AYInfo); break;
    case TYPE_ASC: ASC_Cleanup(AYInfo); break;
    case TYPE_PSC: PSC_Cleanup(AYInfo); break;
    case TYPE_SQT: SQT_Cleanup(AYInfo); break;
    case TYPE_AY: AY_Cleanup(AYInfo); break;
    case TYPE_PSG: PSG_Cleanup(); break;
    case TYPE_RSF: RSF_Cleanup(); break;
    case TYPE_YRG: YRG_Cleanup(); break;
  }
  PlayerCTRL.trackFrame=0;
}

void check_cursor_range(){
  if(Config.play_cur>Config.play_count_files-1) Config.play_cur=Config.play_cur_start;
  if(Config.play_cur<Config.play_cur_start) Config.play_cur=Config.play_count_files-1;
  config_save();
}

void changeTrackIcon(bool next=true){
  if(PlayerCTRL.screen_mode==SCR_PLAYER){
    img.setColorDepth(8);
    img.createSprite(13,13);
    img.fillScreen(0);
    if(next) img.drawBitmap(0,0,playerIcons13x13[NEXT],13,13,TFT_BLACK,TFT_YELLOW);
    else img.drawBitmap(0,0,playerIcons13x13[PREV],13,13,TFT_BLACK,TFT_YELLOW);
    img.pushSprite(114,173);
    img.deleteSprite();
  }
}

void playFinish(){
  //reset frames
  frame_cnt=0;
  PlayerCTRL.trackFrame=0;
  // reset all scroll
  memset(sPos,0,sizeof(sPos));
  memset(scrollDir,true,sizeof(scrollDir));
  memset(scrollbuf,0,sizeof(scrollbuf));
  memset(mlsS,0,sizeof(mlsS));
  // prepare finish
  PlayerCTRL.isPlay=false;
  music_stop();
  muteAYBeep();
  switch(Config.play_mode){
    case PLAY_MODE_ONE:
      if(PlayerCTRL.autoPlay){
        if(PlayerCTRL.music_type==TYPE_AY&&AY_GetNumSongs>0){
          ay_cur_song++;
          if(ay_cur_song>AY_GetNumSongs()){
            ay_cur_song=0;
            changeTrackIcon(true);
          }
        }else{
          changeTrackIcon(true);
        }
      }
      check_cursor_range();
      break;
    case PLAY_MODE_ALL:
      if(PlayerCTRL.autoPlay){
        if(PlayerCTRL.music_type==TYPE_AY&&AY_GetNumSongs>0){
          ay_cur_song++;
          if(ay_cur_song>AY_GetNumSongs()){
            ay_cur_song=0;
            changeTrackIcon(true);
            Config.play_cur++;
          }
        }else{
          changeTrackIcon(true);
          Config.play_cur++;
        }
      }
      check_cursor_range();
      break;
    case PLAY_MODE_SHUFFLE:
      if(PlayerCTRL.autoPlay){
        if(PlayerCTRL.music_type==TYPE_AY&&AY_GetNumSongs>0){
          ay_cur_song++;
          if(ay_cur_song>AY_GetNumSongs()){
            ay_cur_song=0;
            changeTrackIcon(true);
            player_shuffle_cur();
          }
        }else{
          changeTrackIcon(true);
          player_shuffle_cur();
        }
      }
      check_cursor_range();
      break;
  }
  config_save();
  PlayerCTRL.autoPlay=true;
  PlayerCTRL.isBrowserCommand=false;
}

void player(){
  if(!sd_fat.card()->sectorCount()){
    PlayerCTRL.isFinish=true;
    PlayerCTRL.scr_mode_update[SCR_PLAYER]=true;
    PlayerCTRL.screen_mode=SCR_SDEJECT;
    PlayerCTRL.isSDeject=true;
    return;
  } 
  if(PlayerCTRL.isFinish){
    playFinish();
    muteAYBeep();
    if(Config.isPlayAYL){
      playlist_get_entry_full_path(Config.play_cur,lfn,sizeof(lfn),true);
      if(Config.play_prev_cur!=Config.play_cur) ay_cur_song=0;
      Config.play_prev_cur=Config.play_cur;
    }else{
      player_full_path(Config.play_cur,lfn,sizeof(lfn));
      if(prev_file_id!=sort_list_play[Config.play_cur].file_id) ay_cur_song=0;
      prev_file_id=sort_list_play[Config.play_cur].file_id;
    }
    memcpy(playFileName,lfn,sizeof(lfn));
    music_open(playFileName,ay_cur_song);
    music_init();
    muteAYBeep();
    PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
    browser_rebuild=1;
    PlayerCTRL.isFinish=false;
    PlayerCTRL.isPlay=true;
    return;
  }else{
    switch(PlayerCTRL.music_type){
    case TYPE_PSG:
    case TYPE_RSF:
    case TYPE_YRG:
      fillBuffer();
      break;
    default:
      break;
    }
  }
  if(!PlayerCTRL.isPlay){
    muteAYBeep();
  }
}

void scrollInfos(char *txt, uint8_t textSize, uint16_t textColor, int width, int height, int xPos, int yPos, uint8_t scrollNumber=1){
  bool scroll=false;
  int strSize=tft_strlen(txt,textSize);
  if(strSize>width) scroll=true;
  if(scroll){
    if(millis()-mlsS[scrollNumber]>sUp[scrollNumber]){
      img.setColorDepth(8);
      img.createSprite(width,height);
      img.fillScreen(0);
      img.setFreeFont(&WildFont);
      img.setTextSize(textSize);
      img.setTextWrap(false);
      img.setTextColor(textColor);
      img.setCursor((sPos[scrollNumber]-(sPos[scrollNumber]*2)),16);
      img.print(utf8rus(txt));
      sUp[scrollNumber]=sUpC[scrollNumber];
      if(strSize-sPos[scrollNumber]>=width&&scrollDir[scrollNumber]){
        sPos[scrollNumber]++;
        if(strSize-sPos[scrollNumber]<width){scrollDir[scrollNumber]=false;sUp[scrollNumber]=S_UPD_DIR;}
      }
      if(!scrollDir[scrollNumber]){
        sPos[scrollNumber]--;
        if(sPos[scrollNumber]<0){scrollDir[scrollNumber]=true;sUp[scrollNumber]=S_UPD_DIR;}
      }
      mlsS[scrollNumber]=millis();
      img.pushSprite(xPos,yPos);
      img.deleteSprite();
    }
  } else {
    img.createSprite(width,height);
    img.fillScreen(0);
    img.setFreeFont(&WildFont);
    img.setTextSize(textSize);
    img.setTextWrap(false);
    img.setTextColor(textColor);
    img.setCursor(0,textSize*8);
    img.print(utf8rus(txt));
    img.pushSprite(xPos,yPos);
    img.deleteSprite();
  }
}

void showFileInfo(){
  //current Track / Tracks count
  img.setColorDepth(8);
  img.createSprite(12*7,8*2);
  img.setFreeFont(&WildFont);
  img.setTextSize(2);
  img.fillScreen(0);
  uint8_t shift=0;
  img.setCursor(8,16);
  img.setTextColor(WILD_CYAN,TFT_BLACK,true);
  sprintf(tme,"%03d",Config.play_cur-Config.play_cur_start+1);
  img.print(tme);
  img.print("/");
  sprintf(tme,"%03d",Config.play_count_files-Config.play_cur_start);
  img.print(tme);
  img.pushSprite(148,62);
  img.deleteSprite();
  // file type
  if(PlayerCTRL.music_type==TYPE_AY){
    if(AY_GetNumSongs()==0){
      sprintf(tme,"%S",file_ext_list[PlayerCTRL.music_type]);
    }else{
      sprintf(tme, "%S %u/%u",file_ext_list[PlayerCTRL.music_type],ay_cur_song+1,AY_GetNumSongs()+1);
    }
  }else{
    sprintf(tme,"%S",file_ext_list[PlayerCTRL.music_type]);
  }
  img.setColorDepth(8);
  img.createSprite((20*10)-2,16);
  img.setTextSize(2);
  img.setCursor(0,16);
  img.setTextColor(WILD_CYAN);
  img.print("File type ");
  img.setTextColor(TFT_RED);
  img.print(": ");
  img.setTextColor(TFT_YELLOW);
  img.print(tme);
  img.pushSprite(9,220);
  img.deleteSprite();
  // is Turbo Sound
  if(AYInfo.is_ts){
    img.createSprite((3*10)-2,16);
    img.fillScreen(0);
    img.setFreeFont(&WildFont);
    img.setTextSize(2);
    img.setTextColor((colCh2)?TFT_RED:TFT_WHITE);
    img.setCursor(0,16);
    img.print("TS!");
    img.pushSprite(180,220);
    img.deleteSprite();
    colCh2=!colCh2;
  }
  // Name
  img.createSprite((5*10)-2,16);
  img.fillScreen(0);
  img.setTextSize(2);
  img.setCursor(0,16);
  img.setTextColor(WILD_CYAN);
  img.print("Name");
  img.setTextColor(TFT_RED);
  img.print(":");
  img.pushSprite(9,238);
  img.deleteSprite();
  scrollInfos(AYInfo.Name,2,TFT_YELLOW,170,16,60,238,1);
  // Author
  img.createSprite((7*10)-2,16);
  img.fillScreen(0);
  img.setTextSize(2);
  img.setCursor(0,16);
  img.setTextColor(WILD_CYAN);
  img.print("Author");
  img.setTextColor(TFT_RED);
  img.print(":");
  img.pushSprite(9,256);
  img.deleteSprite();
  scrollInfos(AYInfo.Author,2,TFT_YELLOW,150,16,80,256,2);
  // File name
  img.createSprite((5*10)-2,16);
  img.fillScreen(0);
  img.setTextSize(2);
  img.setCursor(0,16);
  img.setTextColor(WILD_CYAN);
  img.print("File");
  img.setTextColor(TFT_RED);
  img.print(":");
  img.pushSprite(9,274);
  img.deleteSprite();
  scrollInfos(playedFileName,2,TFT_YELLOW,170,16,60,274,3);
}

void playerFrameShow(){
  //show logo
  tft.pushImage(8,8,186,44,wildLogo186x44);
  uint8_t shift = 10;
  for (uint8_t i = 0; i < 36; i++) {
    tft.fillRect(shift, 70, 2,2, TFT_BLUE);
    shift += 4;
  }
  // blue delemiter under time
  img.setColorDepth(8);
  img.createSprite(224,2);
  img.fillScreen(0);
  shift=0;
  for (uint8_t i=0;i<56;i++) {
    img.fillRect(shift,0,2,2,TFT_BLUE);
    shift+=4;
  }
  img.pushSprite(9,194);
  img.deleteSprite();
  voltage();
}

void timeShow(){
  //file play position
  img.setColorDepth(8);
  img.createSprite(222,4);
  img.fillScreen(0);
  uint8_t step=0;
  for(uint8_t i=0;i<111;i++){
    if(i%2==0){
      img.fillRect(step,0,2,2,WILD_YELLOW);
      step+=2;
    }else{
      img.fillRect(step,2,2,2,WILD_YELLOW);
      step+=2;
    }
  }
  img.fillRect(0,0,map(PlayerCTRL.trackFrame,0,AYInfo.Length,1,222),4,TFT_YELLOW);
  img.pushSprite(9,161);
  img.deleteSprite();
  //time elapsed
  sprintf(tme, "%2.2u:%2.2u:%2.2u", PlayerCTRL.trackFrame/50/60, PlayerCTRL.trackFrame/50%60, PlayerCTRL.trackFrame*2%100);
  img.setColorDepth(8);
  img.createSprite((8*10)-2,8);
  img.setFreeFont(&WildFont);
  img.setTextSize(2);
  img.fillScreen(0);
  img.setCursor(0,16);
  img.setTextColor(TFT_WHITE,TFT_BLACK,true);
  img.print(tme);
  img.pushSprite(9,172);
  img.deleteSprite();
  img.setColorDepth(8);
  img.createSprite((8*10)-2,8);
  img.setFreeFont(&WildFont);
  img.setTextSize(2);
  img.fillScreen(0);
  img.setCursor(0,8);
  img.setTextColor(WILD_CYAN_D2,TFT_BLACK,true);
  img.print(tme);
  img.pushSprite(9,172+8);
  img.deleteSprite();
  //track time
  sprintf(tme, "%2.2u:%2.2u:%2.2u", AYInfo.Length/50/60, AYInfo.Length/50%60, AYInfo.Length*2%100);
  img.setColorDepth(8);
  img.createSprite((8*10)-2,8);
  img.setFreeFont(&WildFont);
  img.setTextSize(2);
  img.fillScreen(0);
  img.setCursor(0,16);
  img.setTextColor(TFT_WHITE,TFT_BLACK,true);
  img.print(tme);
  img.pushSprite(153,172);
  img.deleteSprite();
  img.setColorDepth(8);
  img.createSprite((8*10)-2,8);
  img.setFreeFont(&WildFont);
  img.setTextSize(2);
  img.fillScreen(0);
  img.setCursor(0,8);
  img.setTextColor(WILD_CYAN_D2,TFT_BLACK,true);
  img.print(tme);
  img.pushSprite(153,172+8);
  img.deleteSprite();
}

void ayClockShow(){
  // ay clock show
  switch(Config.ay_clock){
    case CLK_SPECTRUM: sprintf(tme,"ZX 1.77MHz");break;
    case CLK_PENTAGON: sprintf(tme,"PEN 1.75MHz");break;
    case CLK_MSX: sprintf(tme,"MSX 1.78MHz");break;
    case CLK_CPC: sprintf(tme,"CPC 1.0MHz");break;
    case CLK_ATARIST: sprintf(tme,"ST 2.0MHz");break;
  }
  img.setColorDepth(8);
  img.createSprite((22*10)-2,16);
  img.setTextSize(2);
  img.setCursor(0,16);
  img.setTextColor(WILD_CYAN);
  img.print("AY clock ");
  img.setTextColor(TFT_RED);
  img.print(": ");
  img.setTextColor(TFT_YELLOW);
  img.print(tme);
  img.pushSprite(9,202);
  img.deleteSprite();
}

void showPlayerIcons(){
  //play
  if(PlayerCTRL.isPlay&&!PlayerCTRL.isFastForward&&!PlayerCTRL.isSlowBackward){
    img.setColorDepth(8);
    img.createSprite(13,13);
    img.fillScreen(0);
    img.drawBitmap(0,0,playerIcons13x13[PLAY],13,13,TFT_BLACK,TFT_YELLOW);
    img.pushSprite(114,173);
    img.deleteSprite();
  }
  //pause
  if(!PlayerCTRL.isPlay){
    static bool pse=true;
    if(millis()-mls>500){
      img.setColorDepth(8);
      img.createSprite(13,13);
      img.fillScreen(0);
      if(pse) img.drawBitmap(0,0,playerIcons13x13[PAUSE],13,13,TFT_BLACK,TFT_YELLOW);
      img.pushSprite(114,173);
      img.deleteSprite();
      pse=!pse;
      mls=millis();
    }
  }
  //FWD
  if(PlayerCTRL.isFastForward&&PlayerCTRL.isPlay){
    static bool pse=true;
    if(millis()-mls>500){
      img.createSprite(13,13);
      img.fillScreen(0);
      if(pse) img.drawBitmap(0,0,playerIcons13x13[FWD],13,13,TFT_BLACK,TFT_YELLOW);
      img.pushSprite(114,173);
      img.deleteSprite();
      pse=!pse;
      mls=millis();
    }
  }
  //BWD
  if(PlayerCTRL.isSlowBackward&&PlayerCTRL.isPlay){
    static bool pse=true;
    if(millis()-mls>500){
      img.createSprite(13,13);
      img.setPivot(114+12,173+12);
      if(pse) img.drawBitmap(0,0,playerIcons13x13[FWD],13,13,TFT_BLACK,TFT_YELLOW);
      img.pushRotated(180);
      img.deleteSprite();
      pse=!pse;
      mls=millis();
    }
  }
}

void vbUpdate(){
  tft.fillRect(12,62,10*13,8*2,0);
  uint8_t shift = 10;
  for (uint8_t i = 0; i < 36; i++) {
    tft.fillRect(shift, 70, 2,2, TFT_BLUE);
    shift += 4;
  }
}

void player_screen(){
  if(PlayerCTRL.scr_mode_update[SCR_PLAYER]){
    clear_display_field();
    dynRebuild=true;
    playerFrameShow();
  }
  if(dynRebuild){
    // show play mode
    tft.drawBitmap(199,22,spModes[Config.play_mode],30,30,TFT_BLACK,WILD_CYAN);
    vbUpdate();
  }
  dynRebuild=false;
  PlayerCTRL.scr_mode_update[SCR_PLAYER]=false;
  voltage();
  ayClockShow();
  showFileInfo();
  timeShow();
  showPlayerIcons();
  fastEQ();

  //keypad survey
  if(enc.hasClicks(1)){sound_play(SFX_SELECT); PlayerCTRL.isPlay=!PlayerCTRL.isPlay;}
  if(!enc.holding()&&enc.right()){
    sound_play(SFX_SELECT);
    changeTrackIcon(true);
    if(Config.play_mode==PLAY_MODE_SHUFFLE) player_shuffle_cur();
    else Config.play_cur++;
    PlayerCTRL.isFinish=true;
    PlayerCTRL.autoPlay=false;
    PlayerCTRL.isBrowserCommand=true;
  }
  if(!enc.holding()&&enc.left()){
    sound_play(SFX_SELECT);
    changeTrackIcon(false);
    Config.play_cur--;
    PlayerCTRL.isFinish=true;
    PlayerCTRL.autoPlay=false;
    PlayerCTRL.isBrowserCommand=true;
  }
  if(enc.rightH()){
    if(PlayerCTRL.music_type!=TYPE_AY){
      frame_max=PLAY_FAST;
      PlayerCTRL.isSlowBackward=false;
      PlayerCTRL.isFastForward=true;
    }else{
      sound_play(SFX_SELECT);
      ay_cur_song++;
      if(ay_cur_song>AY_GetNumSongs()) ay_cur_song=0;
      changeTrackIcon(true);
      PlayerCTRL.isFinish=true;
      PlayerCTRL.autoPlay=false;
      PlayerCTRL.isBrowserCommand=true;
    }
  }
  if(enc.leftH()){
    if(PlayerCTRL.music_type!=TYPE_AY){
      frame_max=PLAY_SLOW;
      PlayerCTRL.isFastForward=false;
      PlayerCTRL.isSlowBackward=true;
    }else{
      sound_play(SFX_SELECT);
      ay_cur_song--;
      if(ay_cur_song<0) ay_cur_song=AY_GetNumSongs();
      changeTrackIcon(false);
      PlayerCTRL.isFinish=true;
      PlayerCTRL.autoPlay=false;
      PlayerCTRL.isBrowserCommand=true;
    }
  }
  if(enc.release()){frame_max=PLAY_NORMAL; PlayerCTRL.isFastForward=false; PlayerCTRL.isSlowBackward=false;}
  if (up.hasClicks(1) || up.holding()) {
    if (!enc.holding()) {
      if(Config.volume++ >=63) Config.volume=63;
      writeToAmp(AMP_REG2,(muteL<<7|muteR<<6|Config.volume));
      img.createSprite(10*13,8*2);
      img.fillScreen(0);
      img.setFreeFont(&WildFont);
      img.setTextColor(TFT_MAGENTA);
      img.setTextSize(2);
      img.setCursor(0,16);
      img.print("Volume:  ");
      img.setTextColor(WILD_YELLOW);
      if(Config.volume>=63) img.print("max");
      else if(Config.volume<=0) img.print("min");
      else img.print(Config.volume);
      img.pushSprite(12,62);
      img.deleteSprite();
    }
  }  
  if(up.hasClicks(2)){
    sound_play(SFX_SELECT);
    switch(Config.play_mode){
      case PLAY_MODE_ONE: Config.play_mode=PLAY_MODE_ALL;break;
      case PLAY_MODE_ALL: Config.play_mode=PLAY_MODE_SHUFFLE;break;
      case PLAY_MODE_SHUFFLE: Config.play_mode=PLAY_MODE_ONE;break;
    }
    dynRebuild=true;
  }
  if(dn.hasClicks(1)||dn.holding()) {
    if(!enc.holding()) {
      if(Config.volume-- <=0) Config.volume=0;
      writeToAmp(AMP_REG2,(muteL<<7|muteR<<6|Config.volume));
      img.createSprite(10*13,8*2);
      img.fillScreen(0);
      img.setFreeFont(&WildFont);
      img.setTextColor(TFT_MAGENTA);
      img.setTextSize(2);
      img.setCursor(0,16);
      img.print("Volume:  ");
      img.setTextColor(WILD_YELLOW);
      if(Config.volume>=63) img.print("max");
      else if(Config.volume<=0) img.print("min");
      else img.print(Config.volume);
      img.pushSprite(12,62);
      img.deleteSprite();
    }
  }
  if(dn.hasClicks(2)){
    sound_play(SFX_SELECT);
    switch(Config.ay_clock){
      case CLK_SPECTRUM: Config.ay_clock=CLK_PENTAGON;break;
      case CLK_PENTAGON: Config.ay_clock=CLK_MSX;break;
      case CLK_MSX: Config.ay_clock=CLK_CPC;break;
      case CLK_CPC: Config.ay_clock=CLK_ATARIST;break;
      case CLK_ATARIST: Config.ay_clock=CLK_SPECTRUM;break;
    }
    dynRebuild=true;
  }
  if(dn.timeout(2000)||up.timeout(2000)){
    dynRebuild=true;
    config_save();
  }
}

void wait_frame(){
  uint32_t prev = frame_cnt;
  while(frame_cnt==prev){yield(); vTaskDelay(1);}
}

void AYPlayTask(void * pvParameters){
  while(1){
    if(PlayerCTRL.isSDeject){
      muteAYBeep();
      vTaskDelay(2);
    }else{
      if(PlayerCTRL.isPlay&&!PlayerCTRL.isFinish){
        music_play();
        PlayerCTRL.trackFrame++;
      }
      if(PlayerCTRL.music_type==TYPE_AY){
        if(PlayerCTRL.isPlay&&!PlayerCTRL.isFinish){
          while(!Sound.buf_do_update){
            yield();
          }
          Sound.buf_do_update = false;
          if(PlayerCTRL.isPlay&&!PlayerCTRL.isFinish) {
            AY_PlayBuf();
            frqHz=Sound.buf_rd[Sound.buf_ptr_rd];
            uint8_t beep=0;
            uint8_t lvlRemap=map(frqHz,0,96,0,16);
            for(uint8_t i=0;i<sizeof(bufEQ);i++){
              if(FreqAY[i]>=frqHz&&FreqAY[i+1]<frqHz)beep=i;
            }
            bufEQ[beep]|=lvlRemap;
          }
        }
      }else{
        wait_frame();
      }
      vTaskDelay(2);
    }
  }
  vTaskDelete(NULL);
}

void AYPlayCoreInit(){
  xTaskCreatePinnedToCore(
    AYPlayTask, /* Function to implement the task */
    "AYPlay", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    1,  /* Priority of the task */
    &AYPlay,  /* Task handle. */
    0); /* Core where the task should run */
}
