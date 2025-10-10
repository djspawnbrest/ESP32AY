enum{
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

enum{
  BROWSE_DIR=0,
  BROWSE_AYL
};

enum{
  MUS=0,
  DIR,
  AYL
};

#define SORT_HASH_LEN       4
#define SORT_FILES_MAX      1280 //there are 1070 entries in the Tr_Songs/Authors currently

struct sortStruct{
  char hash[SORT_HASH_LEN];
  uint16_t file_id;
};

sortStruct sort_list[SORT_FILES_MAX];
sortStruct sort_list_play[SORT_FILES_MAX];
int16_t sort_list_len,sort_list_play_len;
int16_t cursor_offset;
uint8_t browser_rebuild=1;

//not need semaphore
void scrollString(char *txt,uint8_t textSize,uint16_t textColor,int width,int height,int xPos,int yPos,uint8_t scrollNumber=0){
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

//not need semaphore
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

//not need semaphore
void browser_reset_directory(){
  strncpy(sdConfig.active_dir,"/",sizeof(sdConfig.active_dir)-1);
  strncpy(sdConfig.prev_dir,"/",sizeof(sdConfig.prev_dir)-1);
  strncpy(sdConfig.ayl_file,"",sizeof(sdConfig.ayl_file)-1);
}

//semaphored
void browser_enter_directory(){
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    sd_dir.open(sdConfig.active_dir,O_RDONLY);
    sd_file.open(&sd_dir,sort_list[sdConfig.dir_cur].file_id,O_RDONLY);
    sd_file.getName(lfn,sizeof(lfn));
    sd_file.close();
    sd_dir.close();
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  strncat(sdConfig.active_dir,lfn,sizeof(sdConfig.active_dir)-1);
  strncat(sdConfig.active_dir,"/",sizeof(sdConfig.active_dir)-1);
  sdConfig.active_dir[sizeof(sdConfig.active_dir)-1]=0;
  sdConfig.dir_cur_prev=sdConfig.dir_cur;
  sdConfig.dir_cur=0;
}

//not need semaphore
void browser_leave_directory(){
  memset(sdConfig.prev_dir,0,sizeof(sdConfig.prev_dir));
  for(int i=strlen(sdConfig.active_dir)-2;i>=0;i--){
    if(sdConfig.active_dir[i]=='/'){
      strncpy(sdConfig.prev_dir,&sdConfig.active_dir[i+1],sizeof(sdConfig.prev_dir)-1);
      sdConfig.active_dir[i+1]=0;
      break;
    }
  }
  sdConfig.dir_cur=sdConfig.dir_cur_prev;
  sdConfig.dir_cur_prev=0;
}

//semaphored
bool browser_full_path(int cur,char* path,int path_size){
  char temp[MAX_PATH]={0};
  if(cur<0||cur>=sort_list_len) return false;
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    if(sd_dir.open(sdConfig.active_dir,O_RDONLY)){
      if(sd_file.open(&sd_dir,sort_list[cur].file_id,O_RDONLY)){
        sd_file.getName(temp,sizeof(temp));
        sd_file.close();
      }
      temp[sizeof(temp)-1]=0;
      sd_dir.close();
      strncpy(path,sdConfig.active_dir,path_size-1);
      strncat(path,temp,path_size-1);
      xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
      return true;
    }
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  path[0]=0;
  return false;
}

//semaphored
bool browser_get_played_file_name(int cur,char* path,int path_size){
  char temp[MAX_PATH]={0};
  if(cur<0||cur>=sort_list_len) return false;
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    if(sd_play_dir.open(sdConfig.play_dir,O_RDONLY)){
      if(sd_file.open(&sd_play_dir,sort_list[cur].file_id,O_RDONLY)){
        sd_file.getName(temp,sizeof(temp));
        sd_file.close();
      }
      temp[sizeof(temp)-1]=0;
      sd_play_dir.close();
      // strncpy(path,sdConfig.play_dir,path_size-1);
      strncat(path,temp,path_size-1);
      xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
      return true;
    }
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  path[0]=0;
  return false;
}

void insertion_sort_list(){
  int i=1;
  while(i<sort_list_len){
    int j=i;
    while(j>0){
      int carry=1;
      for(int k=SORT_HASH_LEN-1;k>=0;k--){
        carry=((sort_list[j].hash[k]-sort_list[j-1].hash[k]-carry)<0)?1:0;
      }
      if(!carry) break;
      sortStruct temp;
      memcpy(&temp,&sort_list[j],sizeof(sortStruct));
      memcpy(&sort_list[j],&sort_list[j-1],sizeof(sortStruct));
      memcpy(&sort_list[j-1],&temp,sizeof(sortStruct));
      j--;
    }
    i++;
  }
  // move variable Config.dir_cur
  int file_id=-1;
  for(int i=0;i<sort_list_len;i++){
    if(sort_list[i].file_id==file_id){
      sdConfig.dir_cur=i;
      break;
    }
  }
  if(sdConfig.dir_cur<0||sdConfig.dir_cur>=sort_list_len) sdConfig.dir_cur=0;
}

int search_files_in_dir(FsFile* dir,char* found_dir,bool* found_file,const char* current_path){
  while(sd_file.openNext(dir,O_RDONLY)){
    if(!sd_file.isHidden()&&sd_file.isFile()){
      memset(lfn,0,SORT_HASH_LEN);
      sd_file.getName(lfn,sizeof(lfn));
      uint8_t file_type=browser_check_ext(lfn);
      if(file_type!=TYPE_UNK){
        if(sort_list_len>=SORT_FILES_MAX){
          sd_file.close();
          sort_list_len=0;
          printf("Too many files\nin a folder\n(%u max)\n",SORT_FILES_MAX);
          return FILE_ERR_OTHER;
        }
        sort_list[sort_list_len].file_id=sd_file.dirIndex();
        for(int i=0;i<SORT_HASH_LEN;i++){
          if(lfn[i]>='a'&&lfn[i]<='z') lfn[i]-=32; // to upper case just in case
          if(lfn[i]=='_') lfn[i]=0x20; // put underscore names to the top
          if(file_type==TYPE_AYL) lfn[i]=0x20; // put ayl on top
        }
        memcpy(sort_list[sort_list_len].hash,lfn,SORT_HASH_LEN);
        if(file_type==TYPE_AYL) cursor_offset++;
        sort_list_len++;
        if(!*found_file){
          snprintf(found_dir,512,"%s",current_path);
          *found_file=true;
        }
      }
    }else if(!sd_file.isHidden()&&sd_file.isSubDir()){
      sort_list_len++;
      cursor_offset++;
    }
    sd_file.close();
  }
  if(sort_list_len==0&&!*found_file){
    return FILE_ERR_NO_FILE;
  }
  insertion_sort_list();
  return FILE_ERR_NONE;
}

int recursive_search(FsFile* dir,char* found_dir,bool* found_file,const char* current_path){
  // Searching files into current directory first
  int result=search_files_in_dir(dir,found_dir,found_file,current_path);
  if(*found_file){
    return FILE_ERR_NONE;
  }
  // If not found supported files - entering into subfolders
  dir->rewindDirectory();
  while(sd_file.openNext(dir,O_RDONLY)){
    char fileMsg[100];
    sd_file.getName(fileMsg,sizeof(fileMsg));
    if(sd_file.isSubDir()){
      cursor_offset=0;
      sort_list_len=0;
      FsFile sub_dir;
      if(sub_dir.open(dir,fileMsg,O_RDONLY)){
        char sub_dir_path[512];
        snprintf(sub_dir_path,sizeof(sub_dir_path),"%s/%s",current_path,fileMsg);
        result=recursive_search(&sub_dir,found_dir,found_file, sub_dir_path);
        sub_dir.close();
        if(*found_file){
          return FILE_ERR_NONE;
        }
      }
    }
    sd_file.close();
  }
  return FILE_ERR_NO_FILE;
}

int browser_search_files_in_sd_dir(bool fromPlayer=false){
  memset(sort_list,0,sizeof(sort_list));
  cursor_offset=0;
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    if(!sd_fat.begin(SD_CONFIG)){
      PlayerCTRL.isSDeject=true;
      PlayerCTRL.screen_mode=SCR_SDEJECT;
      return FILE_ERR_NO_CARD;
    }
    if(!sd_dir.open(fromPlayer?sdConfig.play_dir:sdConfig.active_dir,O_RDONLY)){
      return FILE_ERR_OTHER;
    }
    xSemaphoreGive(sdCardSemaphore); // Release the semaphore
  }
  char found_dir[512]={0};
  bool found_file=false;
  const char* initial_path=fromPlayer?sdConfig.play_dir:sdConfig.active_dir;
  snprintf(found_dir,sizeof(found_dir),"%s",initial_path);
  int result=recursive_search(&sd_dir,found_dir,&found_file,initial_path);
  sd_dir.close();
  if(result==FILE_ERR_NONE){
    // Setting up paths for Config.play_dir and Config.active_dir
    snprintf(sdConfig.play_dir,sizeof(sdConfig.play_dir),"%s",found_dir);
    snprintf(sdConfig.active_dir,sizeof(sdConfig.active_dir),"%s",found_dir);
    // Remove the extra slash at the beginning of the path, if there is one
    if(sdConfig.play_dir[0]=='/'){
      memmove(sdConfig.play_dir,sdConfig.play_dir+1,strlen(sdConfig.play_dir));
    }
    if(sdConfig.active_dir[0]=='/'){
      memmove(sdConfig.active_dir,sdConfig.active_dir+1,strlen(sdConfig.active_dir));
    }
    // Add a slash at the end of the path if there is none.
    if(sdConfig.play_dir[strlen(sdConfig.play_dir)-1]!='/'){
      strncat(sdConfig.play_dir,"/",sizeof(sdConfig.play_dir)-strlen(sdConfig.play_dir)-1);
    }
    if(sdConfig.active_dir[strlen(sdConfig.active_dir)-1]!='/'){
      strncat(sdConfig.active_dir,"/",sizeof(sdConfig.active_dir)-strlen(sdConfig.active_dir)-1);
    }
    return FILE_ERR_NONE;
  }
  PlayerCTRL.isSDeject=true;
  PlayerCTRL.screen_mode=SCR_NOFILES;
  return FILE_ERR_NO_FILE;
}

//semaphored
int browser_build_list(bool fromPlayer=false){
  memset(sort_list,0,sizeof(sort_list));
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    if(!sd_fat.begin(SD_CONFIG)){
      PlayerCTRL.isSDeject=true;
      PlayerCTRL.screen_mode=SCR_SDEJECT;
      return FILE_ERR_NO_CARD;
    }
    if(!sd_dir.open(fromPlayer?sdConfig.play_dir:sdConfig.active_dir,O_RDONLY)){
      return FILE_ERR_OTHER;
    }
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  int file_id=-1;
  sort_list_len=0;
  cursor_offset=0;
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
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
              return FILE_ERR_OTHER;
            }
            if(sd_file.isSubDir()){
              if(strcasecmp(sdConfig.prev_dir,lfn)==0) file_id=sd_file.dirIndex();
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
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  if(sort_list_len==0){
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
      sdConfig.dir_cur=i;
      break;
    }
  }
  if(sdConfig.dir_cur<0||sdConfig.dir_cur>=sort_list_len) sdConfig.dir_cur=0;
  return FILE_ERR_NONE;
}

//not need semaphore
bool browser_move_cur(int dir,bool loop){
  bool done=false;
  sdConfig.dir_cur+=dir;
  if(loop){
    if(sdConfig.dir_cur<0) sdConfig.dir_cur=sort_list_len-1;
    if(sdConfig.dir_cur>=sort_list_len) sdConfig.dir_cur=0;
  }else{
    if(sdConfig.dir_cur<0){
      sdConfig.dir_cur=0;
      done=true;
    }
    if(sdConfig.dir_cur>=sort_list_len){
      sdConfig.dir_cur=sort_list_len-1;
      done=true;
    }
  }
  memset(sPos,0,sizeof(sPos));
  memset(scrollDir,true,sizeof(scrollDir));
  memset(scrollbuf,0,sizeof(scrollbuf));
  memset(mlsS,0,sizeof(mlsS));
  return done;
}

//not need semaphore
void browser_shuffle_cur(){
  int prev_cur=sdConfig.dir_cur;
  while(1){
    sdConfig.dir_cur=rand()%sort_list_len;
    if(sdConfig.dir_cur!=prev_cur) break;
    yield();
  }
}

//semaphored
void browser_dir_draw_begin(int id){
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    sd_dir.open(sdConfig.active_dir,O_RDONLY);
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
}

//semaphored
void browser_dir_draw_item(int sx,int sy,int id){
  char tmp[MAX_PATH];
  memcpy(tmp,sdConfig.active_dir,sizeof(sdConfig.active_dir));
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    sd_file.open(&sd_dir,sort_list[id].file_id,O_RDONLY);
    sd_file.getName(lfn,sizeof(lfn));
    sd_file.close();
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
  lfn[sizeof(lfn)-1]=0;
  uint8_t type=MUS;
  if(sort_list[id].hash[0]==1) type=DIR;
  if(browser_check_ext(lfn)==TYPE_AYL) type=AYL;
  char buf[MAX_PATH];
  switch(type){
    case MUS:
      if(!strcmp(sdConfig.active_dir,sdConfig.play_dir)&&sdConfig.play_cur==id&&!sdConfig.isPlayAYL){
        spr_print(img,sx,sy,lfn,2,TFT_BLUE);
      }else{
        spr_print(img,sx,sy,lfn,2,WILD_CYAN_D2);
      }
      break;
    case DIR:
      sprintf(buf,"[%s]",lfn);
      strcat(tmp,lfn);
      strcat(tmp,"/");
      // Processing and highlighting full playing path folders\file or playlist
      if(strstr(sdConfig.play_dir,tmp)!=NULL&&!sdConfig.isPlayAYL){
        spr_print(img,sx,sy,buf,2,TFT_VIOLET);
      }else if(strstr(sdConfig.play_ayl_file,tmp)!=NULL&&sdConfig.isPlayAYL){
        spr_print(img,sx,sy,buf,2,TFT_VIOLET);
      }else{
        spr_print(img,sx,sy,buf,2,TFT_YELLOW);
      }
      break;
    case AYL:
      sprintf(buf,"<%s>",lfn);
      strcat(tmp,lfn);
      if(!strcmp(sdConfig.play_ayl_file,tmp)&&sdConfig.isPlayAYL){
        spr_print(img,sx,sy,buf,2,TFT_VIOLET);
      }else{
        spr_print(img,sx,sy,buf,2,WILD_GREEN);
      }
      break;
  }
}

//semaphored
void browser_dir_draw_end(){
  if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
    sd_dir.close();
    xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  }
}

//in playlist.h
void browser_ayl_draw_begin(int id){
  playlist_open(sdConfig.ayl_file,id);
}

//in playlist.h
void browser_ayl_draw_item(int sx,int sy,int id){
  playlist_iterate(lfn,sizeof(lfn));
  playlist_file_name(lfn,sizeof(lfn));
  if(!strcmp(sdConfig.ayl_file,sdConfig.play_ayl_file)&&sdConfig.play_cur==id&&sdConfig.isPlayAYL){
    spr_print(img,sx,sy,lfn,2,TFT_BLUE);
  }else{
    spr_print(img,sx,sy,lfn,2,WILD_CYAN_D2);
  }
}

//in playlist.h
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
      if(!playlist_open(sdConfig.ayl_file,0)) return FILE_ERR_NO_CARD;
      sort_list_len=0;
      while(playlist_iterate(lfn,sizeof(lfn))){
        sort_list_len++;
      }
      playlist_close();
    }
  }
  if(PlayerCTRL.scr_mode_update[SCR_BROWSER]){ 
    int id=sdConfig.dir_cur-BROWSER_LINES/2;
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
        if(sdConfig.dir_cur==id){
          // draw cursor
          img.fillRoundRect(sx,sy-(8*2),img.width(),8*2,3,TFT_RED);
        }
        if(mode==BROWSE_DIR){
          browser_dir_draw_item(sx,sy,id);
        }else{
          browser_ayl_draw_item(sx,sy,id);
        }
        if(sdConfig.dir_cur==id){
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
    if(sdConfig.isPlayAYL){
      if(!strcmp(sdConfig.ayl_file,sdConfig.play_ayl_file)&&sdConfig.play_cur==sdConfig.dir_cur){
        scrollString(scrollbuf,2,TFT_BLUE,224,16,8,scrollSY-8,0);
      }else{
        scrollString(scrollbuf,2,WILD_CYAN_D2,224,16,8,scrollSY-8,0);
      }
    }else{
      if(!strcmp(sdConfig.active_dir,sdConfig.play_dir)&&sdConfig.play_cur==sdConfig.dir_cur){
        scrollString(scrollbuf,2,TFT_BLUE,224,16,8,scrollSY-8,0);
      }else{
        scrollString(scrollbuf,2,WILD_CYAN_D2,224,16,8,scrollSY-8,0);
      }
    }
  }
  //keypad survey
  if(enc.left()&&lcdBlackout==false&&scrNotPlayer==false){
    PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
    browser_move_cur(-1,true);
  }
  if(enc.right()&&lcdBlackout==false&&scrNotPlayer==false){
    PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
    browser_move_cur(1,true);
  }
  if(enc.hasClicks(1)&&!up.holding()&&lcdBlackout==false&&scrNotPlayer==false){
    if(mode==BROWSE_DIR){
      if(sort_list[sdConfig.dir_cur].hash[0]==1){ //directory
        browser_enter_directory();
        PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
        browser_rebuild=1;
      }else{ //file
        browser_full_path(sdConfig.dir_cur,lfn,sizeof(lfn));
        switch(browser_check_ext(lfn)){
          case TYPE_UNK:
            return FILE_ERR_UNK_FORMAT;
            break;
          case TYPE_AYL: //enter playlist mode
            strncpy(sdConfig.ayl_file,lfn,sizeof(sdConfig.ayl_file)-1);
            sdConfig.dir_cur_prev=sdConfig.dir_cur;
            sdConfig.dir_cur=0;
            PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
            browser_rebuild=1;
            sdConfig.isBrowserPlaylist=BROWSE_AYL;
            delay(30);
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
          case TYPE_MOD:
          case TYPE_S3M:
        #if defined(CONFIG_IDF_TARGET_ESP32S3)
          case TYPE_XM:
        #endif
            muteAmp();
            muteAYBeep();
            memcpy(sdConfig.play_dir,sdConfig.active_dir,sizeof(sdConfig.active_dir));
            memcpy(sort_list_play,sort_list,sizeof(sort_list));
            sdConfig.play_count_files=sort_list_len;
            sdConfig.play_cur_start=cursor_offset;
            sdConfig.play_cur=sdConfig.dir_cur;
            // leave AYL play
            sdConfig.play_ayl_file[0]=0;
            sdConfig.isPlayAYL=false;
            // rebuild browser list command
            browser_rebuild=1;
            PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
            // change track command
            PlayerCTRL.isBrowserCommand=true;
            PlayerCTRL.autoPlay=false;
            PlayerCTRL.isFinish=true;
            delay(30);
            break;
        }
      }
    }else{
      PlayerCTRL.isPlay=false;
      playlist_get_entry_full_path(sdConfig.dir_cur,lfn,sizeof(lfn));
      memcpy(sdConfig.play_ayl_file,sdConfig.ayl_file,sizeof(sdConfig.ayl_file));
      sdConfig.play_cur=sdConfig.dir_cur;
      // enter AYL play
      sdConfig.play_count_files=sort_list_len;
      sdConfig.play_cur_start=0;
      sdConfig.isPlayAYL=true;
      // rebuild browser list command
      browser_rebuild=1;
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      // change track command
      PlayerCTRL.isBrowserCommand=true;
      PlayerCTRL.autoPlay=false;
      PlayerCTRL.isFinish=true;
      delay(30);
    }
    sd_config_save();
    return sdConfig.dir_cur;
  }
  if(up.holding()&&enc.hasClicks(1)&&mode==BROWSE_DIR&&sort_list[sdConfig.dir_cur].hash[0]!=1&&lcdBlackout==false&&scrNotPlayer==false){ // Remove file if not folder and not in playlist
    char pf[MAX_PATH];
    char rf[MAX_PATH];
    sdConfig.play_ayl_file[0]=0; // clean string
    browser_full_path(sdConfig.dir_cur,rf,sizeof(rf)); // Get full removed file path
    browser_get_played_file_name(sdConfig.play_cur,sdConfig.play_ayl_file,sizeof(sdConfig.play_ayl_file));
    strncpy(pf,sdConfig.play_dir,sizeof(pf)-1);
    strncat(pf,sdConfig.play_ayl_file,sizeof(pf)-1);
    if(!strcmp(rf,pf)){ // if played file
      if(sdConfig.play_count_files>1){
        PlayerCTRL.isPlay=false;
        if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
          sd_fat.remove(rf);
          xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
        }
        // update folder
        browser_build_list();
        memcpy(sdConfig.play_dir,sdConfig.active_dir,sizeof(sdConfig.active_dir));
        memcpy(sort_list_play,sort_list,sizeof(sort_list));
        sdConfig.play_count_files=sort_list_len;
        if(sdConfig.play_count_files==0&&strcmp(sdConfig.play_dir,"/")){ // if not root directory
          browser_leave_directory();
          browser_build_list();
          memcpy(sdConfig.play_dir,sdConfig.active_dir,sizeof(sdConfig.active_dir));
          memcpy(sort_list_play,sort_list,sizeof(sort_list));
          sdConfig.play_count_files=sort_list_len;
          if(sdConfig.play_count_files==0) goto pf0;
        }
        sdConfig.play_cur_start=cursor_offset;
        sdConfig.play_prev_cur=sdConfig.play_cur;
        sdConfig.play_cur=sdConfig.dir_cur;
        sdConfig.isPlayAYL=false;
        // change track command
        PlayerCTRL.isBrowserCommand=true;
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isFinish=true;
        // rebuild browser list command
        browser_rebuild=1;
        PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
        delay(30);
      }else{ // no more file in this dir - leave, search new
        PlayerCTRL.isPlay=false;
        if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
          sd_fat.remove(rf);
          xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
        }
      pf0:
        // reseting
        configResetPlayingPath();
        sort_list_len=0;
        sdConfig.play_count_files=sort_list_len;
        sdConfig.play_cur_start=0;
        sort_list_len=0;
        // now prepare default playing file
        browser_search_files_in_sd_dir(true);
        memcpy(sort_list_play,sort_list,sizeof(sort_list));
        sdConfig.play_count_files=sort_list_len;
        sdConfig.play_cur_start=cursor_offset;
        sdConfig.play_cur=sdConfig.dir_cur=cursor_offset;
        sdConfig.isPlayAYL=false;
        // change track command
        PlayerCTRL.isBrowserCommand=true;
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isFinish=true;
        // rebuild browser list command
        browser_rebuild=1;
        PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      }
    }else if(!strcmp(sdConfig.play_dir,sdConfig.active_dir)&&strcmp(rf,pf)){ // if in playing dir
      if(sdConfig.play_count_files>1){
        if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
          sd_fat.remove(rf);
          xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
        }
        // update folder
        browser_build_list();
        memcpy(sdConfig.play_dir,sdConfig.active_dir,sizeof(sdConfig.active_dir));
        memcpy(sort_list_play,sort_list,sizeof(sort_list));
        sdConfig.play_count_files=sort_list_len;
        sdConfig.play_cur_start=cursor_offset;
        if(sdConfig.play_cur>sdConfig.dir_cur+sdConfig.play_cur_start) sdConfig.play_cur--; // if removed file before playing file fix
        sdConfig.isPlayAYL=false;
        // rebuild browser list command
        browser_rebuild=1;
        PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      }else{ // no more file in this dir - leave, search new
        PlayerCTRL.isPlay=false;
        if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
          sd_fat.remove(rf);
          xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
        }
        // reseting
        configResetPlayingPath();
        sort_list_len=0;
        sdConfig.play_count_files=sort_list_len;
        sdConfig.play_cur_start=0;
        sort_list_len=0;
        // now prepare default playing file
        browser_search_files_in_sd_dir(true);
        memcpy(sort_list_play,sort_list,sizeof(sort_list));
        sdConfig.play_count_files=sort_list_len;
        sdConfig.play_cur_start=cursor_offset;
        sdConfig.play_cur=sdConfig.dir_cur=cursor_offset;
        sdConfig.isPlayAYL=false;
        // change track command
        PlayerCTRL.isBrowserCommand=true;
        PlayerCTRL.autoPlay=false;
        PlayerCTRL.isFinish=true;
        // rebuild browser list command
        browser_rebuild=1;
        PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      }
    }else{ // not played dir
      if(xSemaphoreTake(sdCardSemaphore,portMAX_DELAY)==pdTRUE){
        sd_fat.remove(rf);
        xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
      }
      browser_rebuild=1;
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
    }
    sd_config_save();
    return sdConfig.dir_cur;
  }
  if(dn.click()&&lcdBlackout==false&&scrNotPlayer==false){
    if(mode==BROWSE_DIR){
      browser_leave_directory();
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      browser_rebuild=1;
    }else{ // leave playlist
      sdConfig.dir_cur=sdConfig.dir_cur_prev;
      sdConfig.dir_cur_prev=0;
      sdConfig.ayl_file[0]=0;
      PlayerCTRL.scr_mode_update[SCR_BROWSER]=true;
      browser_rebuild=1;
      sdConfig.isBrowserPlaylist=BROWSE_DIR;
    }
    sd_config_save();
    return sdConfig.dir_cur;
  }
  keysTimeOut();
  return 0;
}

void initSemaphore(){
  sdCardSemaphore=xSemaphoreCreateMutex();
  if(sdCardSemaphore==NULL){
    printf("Error creating sd semaphore\n");
  }else{
    // Give the semaphore so it's available for the first take
    xSemaphoreGive(sdCardSemaphore);
  }
  outSemaphore=xSemaphoreCreateBinary();
  if(outSemaphore==NULL){
    printf("Failed to create outSemaphore\n");
  }else{
    // Give the semaphore so it's available for the first take
    xSemaphoreGive(outSemaphore);
  }
}
