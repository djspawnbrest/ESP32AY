enum {
  FILE_ERR_NONE=0,
  FILE_ERR_NO_CARD=-1,
  FILE_ERR_TOO_LARGE=-2,
  FILE_ERR_NOT_ENOUGH_MEMORY=-3,
  FILE_ERR_UNK_FORMAT=-4,
  FILE_ERR_OTHER=-5,
  FILE_ERR_NO_FILE=-6
};

#define BROWSER_LINES       18
// #define BROWSER_WAIT_SHOW   1000/10 //delay before showing wait screen

enum {
  BROWSE_DIR,
  BROWSE_AYL
};

enum {
  MUS=0,
  DIR,
  AYL
};

#define SORT_HASH_LEN       4
#define SORT_FILES_MAX      1280  //there are 1070 entries in the Tr_Songs/Authors currently

struct sortStruct {
  char hash[SORT_HASH_LEN];
  uint16_t file_id;
};

sortStruct sort_list[SORT_FILES_MAX];
sortStruct sort_list_play[SORT_FILES_MAX];
int16_t sort_list_len,sort_list_play_len;
int16_t cursor_offset;
uint8_t browser_rebuild=1;

void scrollString(char *txt, uint8_t textSize, uint16_t textColor, int width, int height, int xPos, int yPos, uint8_t scrollNumber=0){
  uint16_t strSize=tft_strlen(txt,textSize);
  if(millis()-mlsS[scrollNumber]>sUp[scrollNumber]){
    img.setColorDepth(8);
    img.createSprite(width,height);
    img.fillScreen(0);
    img.fillRoundRect(0,0,width,height,3,TFT_RED);
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
}

int browser_check_ext(const char* name){
  for(int i=strlen(name)-1;i>=0;i--){
    if(name[i]=='.'){
      for(int j=1;j<TYPES_ALL;j++){
        if(strcasecmp(&name[i+1],file_ext_list[j])==0) return j;
      }
      break;
    }
  }
  return TYPE_UNK;
}

void browser_reset_directory(){
  strncpy(Config.active_dir,"/",sizeof(Config.active_dir)-1);
  strncpy(Config.prev_dir,"/",sizeof(Config.prev_dir)-1);
  strncpy(Config.ayl_file,"",sizeof(Config.ayl_file)-1);
}

void browser_enter_directory(){
  sd_dir.open(Config.active_dir, O_RDONLY);
  sd_file.open(&sd_dir,sort_list[Config.dir_cur].file_id,O_RDONLY);
  sd_file.getName(lfn, sizeof(lfn));
  sd_file.close();
  sd_dir.close();
  strncat(Config.active_dir,lfn,sizeof(Config.active_dir)-1);
  strncat(Config.active_dir,"/",sizeof(Config.active_dir)-1);
  Config.active_dir[sizeof(Config.active_dir)-1]=0;
  Config.dir_cur_prev=Config.dir_cur;
  Config.dir_cur=0;
}

void browser_leave_directory(){
  memset(Config.prev_dir,0,sizeof(Config.prev_dir));
  for(int i=strlen(Config.active_dir)-2;i>=0;i--){
    if(Config.active_dir[i]=='/'){
      strncpy(Config.prev_dir,&Config.active_dir[i+1],sizeof(Config.prev_dir)-1);
      // Config.prev_dir[strlen(Config.prev_dir)-1]=0; //remove trailing slash
      Config.active_dir[i+1]=0;
      break;
    }
  }
  Config.dir_cur=Config.dir_cur_prev;
  Config.dir_cur_prev=0;
}

bool browser_full_path(int cur, char* path, int path_size){
  char temp[MAX_PATH]={0};
  if(cur<0||cur>=sort_list_len) return false;
  if(sd_dir.open(Config.active_dir,O_RDONLY)){
    if(sd_file.open(&sd_dir,sort_list[cur].file_id,O_RDONLY)){
      sd_file.getName(temp,sizeof(temp));
      sd_file.close();
    }
    temp[sizeof(temp)-1]=0;
    sd_dir.close();
    strncpy(path,Config.active_dir,path_size-1);
    strncat(path,temp,path_size-1);
    return true;
  }
  path[0]=0;
  return false;
}

int browser_build_list(bool fromPlayer=false){
  memset(sort_list,0,sizeof(sort_list));
  if(!sd_fat.begin(SD_CONFIG)){
    // message_box_P(PSTR(MSG_ERR_NO_CARD),MB_OK);
    PlayerCTRL.isSDeject=true;
    PlayerCTRL.screen_mode=SCR_SDEJECT;
    return FILE_ERR_NO_CARD;
  }
  if(!sd_dir.open(fromPlayer?Config.play_dir:Config.active_dir,O_RDONLY)){
    // message_box_P(PSTR(MSG_ERR_CANT_OPEN_DIR),MB_OK);
    return FILE_ERR_OTHER;
  }
  int file_id=-1;
  sort_list_len=0;
  cursor_offset=0;
  int ptime=millis();
  while(sd_file.openNext(&sd_dir,O_RDONLY)){
    if(!sd_file.isHidden()){
      memset(lfn,0,SORT_HASH_LEN);
      sd_file.getName(lfn,sizeof(lfn)); //full long name is needed to be able to check the extension
      uint8_t file_type=browser_check_ext(lfn);
      if(sd_file.isSubDir()||(sd_file.isFile()&&file_type!=TYPE_UNK)){
          if(sort_list_len>=SORT_FILES_MAX){
            sd_file.close();
            sd_dir.close();
            sort_list_len=0;
            char str[128];
            snprintf(str,sizeof(str),"Too many files\nin a folder\n(%u max)",SORT_FILES_MAX);
            // message_box(str, MB_OK);
            return FILE_ERR_OTHER;
          }
          if(sd_file.isSubDir()){
            if(strcasecmp(Config.prev_dir,lfn)==0) file_id=sd_file.dirIndex();
          }
          sort_list[sort_list_len].file_id=sd_file.dirIndex();
          for(int i=0;i<SORT_HASH_LEN;i++){
            if(lfn[i]>='a'&&lfn[i]<='z') lfn[i]-=32;  //to upper case just in case
            if(lfn[i]=='_') lfn[i]=0x20; //put underscore names to the top
            if(file_type==TYPE_AYL) lfn[i]=0x20; // put ayl on top
          }
          if(sd_file.isSubDir()){
            sort_list[sort_list_len].hash[0]=1;
            memcpy(&sort_list[sort_list_len].hash[1],lfn,SORT_HASH_LEN-1);
          }else{
            memcpy(sort_list[sort_list_len].hash,lfn,SORT_HASH_LEN);
          }
          if(sd_file.isSubDir()||file_type==TYPE_AYL) cursor_offset++;
          sort_list_len++;
      }
    }
    sd_file.close();
  }
  sd_dir.close();
  if(sort_list_len==0){
    // message_box_P(PSTR(MSG_ERR_NO_FILES), MB_OK);
    return FILE_ERR_OTHER;
  }
  //now sort the list for a convenient alphanumeric ordered display
  //insertion sort
  int i=1;
  while(i<sort_list_len){
    int j=i;
    while(j>0){
      int carry=1;
      for(int k=SORT_HASH_LEN-1;k>=0;k--){
        carry=((sort_list[j].hash[k]-sort_list[j-1].hash[k]-carry)<0)?1:0;
      }
      if (!carry) break;
      sortStruct temp;
      memcpy(&temp,&sort_list[j],sizeof(sortStruct));
      memcpy(&sort_list[j],&sort_list[j-1],sizeof(sortStruct));
      memcpy(&sort_list[j-1],&temp,sizeof(sortStruct));
      j--;
    }
    i++;
  }
  //or bubblesort
  /*
    for(int i=0;i<sort_list_len-1;i++){
      for(int j=0;j<sort_list_len-i-1;j++){
        //if arr j > arr j+1 swap
        int carry=1;
        for(int k=SORT_HASH_LEN-1;k>=0;k++){
          carry=((sort_list[j].hash[k]-sort_list[j+1].hash[k]-carry)<0)?1:0;
        }
        if(!carry){
          sortStruct temp;
          memcpy(&temp,&sort_list[j],sizeof(sortStruct));
          memcpy(&sort_list[j],&sort_list[j+1],sizeof(sortStruct));
          memcpy(&sort_list[j+1],&temp,sizeof(sortStruct));
        }
      }
    }
  */
  for(int i=0;i<sort_list_len;i++){
    if(sort_list[i].file_id==file_id){
      Config.dir_cur=i;
      break;
    }
  }
  if(Config.dir_cur<0||Config.dir_cur>=sort_list_len) Config.dir_cur=0;
  return FILE_ERR_NONE;
}

bool browser_move_cur(int dir, bool loop){
  bool done=false;
  // frame_cnt = 0;
  Config.dir_cur+=dir;
  if(loop){
    if(Config.dir_cur<0) Config.dir_cur=sort_list_len-1;
    if(Config.dir_cur>=sort_list_len) Config.dir_cur=0;
  }else{
    if(Config.dir_cur<0){
      Config.dir_cur=0;
      done=true;
    }
    if(Config.dir_cur>=sort_list_len){
      Config.dir_cur=sort_list_len-1;
      done=true;
    }
  }
  memset(sPos,0,sizeof(sPos));
  memset(scrollDir,true,sizeof(scrollDir));
  memset(scrollbuf,0,sizeof(scrollbuf));
  memset(mlsS,0,sizeof(mlsS));
  return done;
}

void browser_shuffle_cur(){
  int prev_cur=Config.dir_cur;
  while(1){
    Config.dir_cur=rand()%sort_list_len;
    if(Config.dir_cur!=prev_cur) break;
    yield();
  }
}

void browser_dir_draw_begin(int id){
  sd_dir.open(Config.active_dir,O_RDONLY);
}

void browser_dir_draw_item(int sx, int sy, int id){
  char tmp[MAX_PATH];
  memcpy(tmp,Config.active_dir,sizeof(Config.active_dir));
  sd_file.open(&sd_dir,sort_list[id].file_id,O_RDONLY);
  sd_file.getName(lfn,sizeof(lfn));
  sd_file.close();
  lfn[sizeof(lfn)-1]=0;
  uint8_t type=MUS;
  if(sort_list[id].hash[0]==1) type=DIR;
  if(browser_check_ext(lfn)==TYPE_AYL) type=AYL;
  char buf[MAX_PATH];
  switch(type){
    case MUS:
      if(!strcmp(Config.active_dir,Config.play_dir)&&Config.play_cur==id&&!Config.isPlayAYL){
        spr_print(img,sx,sy,lfn,2,TFT_BLUE);
      }else{
        spr_print(img,sx,sy,lfn,2,WILD_CYAN_D2);
      }
      break;
    case DIR:
      sprintf(buf,"[%s]",lfn);
      strcat(tmp,lfn);
      strcat(tmp,"/");
      if(!strcmp(Config.play_dir,tmp)&&!Config.isPlayAYL){
        spr_print(img,sx,sy,buf,2,TFT_VIOLET);
      }else{
        spr_print(img,sx,sy,buf,2,TFT_YELLOW);
      }
      break;
    case AYL:
      sprintf(buf,"<%s>",lfn);
      strcat(tmp,lfn);
      if(!strcmp(Config.play_ayl_file,tmp)&&Config.isPlayAYL){
        spr_print(img,sx,sy,buf,2,TFT_VIOLET);
      }else{
        spr_print(img,sx,sy,buf,2,WILD_GREEN);
      }
      break;
  }
}

void browser_dir_draw_end(){
  sd_dir.close();
}

void browser_ayl_draw_begin(int id){
  playlist_open(Config.ayl_file,id);
}

void browser_ayl_draw_item(int sx, int sy, int id){
  playlist_iterate(lfn,sizeof(lfn));
  playlist_file_name(lfn,sizeof(lfn));
  if(!strcmp(Config.ayl_file,Config.play_ayl_file)&&Config.play_cur==id&&Config.isPlayAYL){
    spr_print(img,sx,sy,lfn,2,TFT_BLUE);
  }else{
    spr_print(img,sx,sy,lfn,2,WILD_CYAN_D2);
  }
}

void browser_ayl_draw_end(){
  playlist_close();
}

int browser_screen(int mode){
  PGM_P header_dir=PSTR("Files");
  PGM_P header_ayl=PSTR("Playlist");
  if(mode==BROWSE_DIR){
    if(browser_rebuild){
      browser_rebuild=0;
      int ret=browser_build_list();
      if(ret!=FILE_ERR_NONE){
        if(ret!=FILE_ERR_NO_CARD){
          browser_leave_directory();
        }else{
          browser_reset_directory();
        }
        browser_rebuild=1;
        return ret;
      }
    }
  }else{
    if(browser_rebuild){
      browser_rebuild=0;
      if(!playlist_open(Config.ayl_file,0)) return FILE_ERR_NO_CARD;
      sort_list_len=0;
      while(playlist_iterate(lfn,sizeof(lfn))){
        sort_list_len++;
      }
      playlist_close();
    }
  }
  if(PlayerCTRL.scr_mode_update[SCR_BROWSER]){ 
    int id=Config.dir_cur-BROWSER_LINES/2;
    if(id>=sort_list_len-BROWSER_LINES) id=sort_list_len-BROWSER_LINES;
    if(id<0) id=0;
    img.setColorDepth(8);
    img.createSprite(224,304);
    img.fillScreen(0);
    img.setTextWrap(false);
    img.setTextColor(TFT_WHITE);
    img.setTextSize(2);
    img.setFreeFont(&WildFont);
    //draw header
    PGM_P header_str=(mode==BROWSE_DIR)?header_dir:header_ayl;
    spr_println(img,0,1,header_str,2,ALIGN_CENTER,WILD_CYAN);
    int sx=0;
    int sy=8*2*2;
    if(mode==BROWSE_DIR){
      browser_dir_draw_begin(id);
    }else{
      browser_ayl_draw_begin(id);
    }
    for(int i=0;i<BROWSER_LINES;i++){
      if(id>=0&&id<sort_list_len){
        if(Config.dir_cur==id){
          // draw cursor
          img.fillRoundRect(sx,sy-(8*2),img.width(),8*2,3,TFT_RED);
        }
        if(mode==BROWSE_DIR){
          browser_dir_draw_item(sx,sy,id);
        }else{
          browser_ayl_draw_item(sx,sy,id);
        }
        if(Config.dir_cur==id){
          if(tft_strlen(lfn,2)>img.width()){
            memcpy(scrollbuf,lfn,sizeof(lfn));
            sUp[0]=S_UPD_DIR;
            mlsS[0]=millis();
            scroll=true;
            scrollSY=sy;
          }else{
            memset(sPos,0,sizeof(sPos));
            memset(scrollDir,true,sizeof(scrollDir));
            memset(scrollbuf,0,sizeof(scrollbuf));
            memset(mlsS,0,sizeof(mlsS));
            scroll=false;
            scrollSY=0;
          }
        }
        sy+=8*2;
      }
      id++;
    }
    if(mode==BROWSE_DIR){
      browser_dir_draw_end();
    }else{
      browser_ayl_draw_end();
    }
    PlayerCTRL.scr_mode_update[SCR_BROWSER]=false;
    img.pushSprite(8,8);
    img.deleteSprite();
  }
  //scroll
  if(scroll){
    if(Config.isPlayAYL){
      if(!strcmp(Config.ayl_file,Config.play_ayl_file)&&Config.play_cur==Config.dir_cur){
        scrollString(scrollbuf,2,TFT_BLUE,224,16,8,scrollSY-8,0);
      }else{
        scrollString(scrollbuf,2,WILD_CYAN_D2,224,16,8,scrollSY-8,0);
      }
    }else{
      if(!strcmp(Config.active_dir,Config.play_dir)&&Config.play_cur==Config.dir_cur){
        scrollString(scrollbuf,2,TFT_BLUE,224,16,8,scrollSY-8,0);
      }else{
        scrollString(scrollbuf,2,WILD_CYAN_D2,224,16,8,scrollSY-8,0);
      }
    }
  }
  //keypad survey
  if(enc.left()){
    sound_play(SFX_MOVE);
    PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
    browser_move_cur(-1,true);
  }
  if(enc.right()){
    sound_play(SFX_MOVE);
    PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
    browser_move_cur(1,true);
  }
  if(enc.hasClicks(1)){
    sound_play(SFX_SELECT);
    if(mode==BROWSE_DIR){
      if(sort_list[Config.dir_cur].hash[0]==1){ //directory
        browser_enter_directory();
        PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
        browser_rebuild=1;
      }else{ //file
        browser_full_path(Config.dir_cur,lfn,sizeof(lfn));
        switch(browser_check_ext(lfn)){
          case TYPE_UNK:
            return FILE_ERR_UNK_FORMAT;
            break;
          case TYPE_AYL: //enter playlist mode
            strncpy(Config.ayl_file,lfn,sizeof(Config.ayl_file)-1);
            Config.dir_cur_prev=Config.dir_cur;
            Config.dir_cur=0;
            PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
            browser_rebuild=1;
            Config.isBrowserPlaylist=BROWSE_AYL;
            break;
          case TYPE_PT1:
          case TYPE_PT2:
          case TYPE_PT3:
          case TYPE_STC:
          case TYPE_STP:
          case TYPE_ASC:
          case TYPE_PSC:
          case TYPE_SQT:
          case TYPE_AY:
          case TYPE_PSG:
          case TYPE_RSF:
          case TYPE_YRG:
            memcpy(Config.play_dir,Config.active_dir,sizeof(Config.active_dir));
            memcpy(sort_list_play,sort_list,sizeof(sort_list));
            Config.play_count_files=sort_list_len;
            Config.play_cur_start=cursor_offset;
            Config.play_cur=Config.dir_cur;
            // leave AYL play
            Config.play_ayl_file[0]=0;
            Config.isPlayAYL=false;
            // rebuild browser list command
            browser_rebuild=1;
            PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
            // change track command
            PlayerCTRL.isBrowserCommand=true;
            PlayerCTRL.autoPlay=false;
            PlayerCTRL.isFinish=true;
            break;
        }
      }
    }else{
      playlist_get_entry_full_path(Config.dir_cur,lfn,sizeof(lfn));
      memcpy(Config.play_ayl_file,Config.ayl_file,sizeof(Config.ayl_file));
      Config.play_cur=Config.dir_cur;
      // enter AYL play
      Config.play_count_files=sort_list_len;
      Config.play_cur_start=0;
      Config.isPlayAYL=true;
      // rebuild browser list command
      browser_rebuild=1;
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      // change track command
      PlayerCTRL.isBrowserCommand=true;
      PlayerCTRL.autoPlay=false;
      PlayerCTRL.isFinish=true;
      // DEBUG
    }
    config_save();
    return Config.dir_cur;
  }
  // if(up.click()){
  //   stop_down++;
  //   // if(stop_down>=STOP_HOLD_TIME){
  //     sound_play(SFX_SELECT);
  //     return -1;
  //   // }
  // }else{
  //   stop_down=0;
  // }
  if(dn.click()){
    sound_play(SFX_CANCEL);
    if(mode==BROWSE_DIR){
      browser_leave_directory();
      // browser_build_list();
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      browser_rebuild=1;
    }else{ // leave playlist
      Config.dir_cur=Config.dir_cur_prev;
      Config.dir_cur_prev=0;
      Config.ayl_file[0]=0;
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      browser_rebuild=1;
      Config.isBrowserPlaylist=BROWSE_DIR;
    }
    config_save();
    return Config.dir_cur;
  }
  return 0;
}