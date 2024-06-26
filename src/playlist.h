bool playlist_iterate(char* buf,int buf_size,bool fromPlayer=false){
  bool skip=false;
  while(1){
    if(fromPlayer?!sd_play_file.fgets(buf,buf_size):!sd_file.fgets(buf,buf_size)) return false;
    if(buf[0]=='<'){
      skip = true;
      continue;
    }
    if(buf[0]=='>'){
      skip=false;
      continue;
    }
    if(skip) continue;
    buf[buf_size-1]=0; //in case of entries exceeding read buffer
    buf[strcspn(buf,"\r\n")]=0;  //remove trailing newline
    int type=browser_check_ext(buf);
    if(type==TYPE_UNK||type==TYPE_AYL) continue;
    break;
  }
  for(int i=0;i<buf_size;i++){
    if(buf[i]=='\\') buf[i]='/';
  }
  return true;
}

void playlist_close(bool fromPlayer=false){
  fromPlayer?sd_play_file.close():sd_file.close();
}

bool playlist_open(const char* filename, int cur, bool fromPlayer=false){
  char temp[16];
  if(!sd_fat.begin(SD_CONFIG)){
    // message_box_P(PSTR(MSG_ERR_NO_CARD), MB_OK);
    return false;
  }
  if(fromPlayer?!sd_play_file.open(filename,O_RDONLY):!sd_file.open(filename,O_RDONLY)) return false;
  if(fromPlayer?!sd_play_file.fgets(temp,sizeof(temp)):!sd_file.fgets(temp,sizeof(temp))) return false;
  if(memcmp(temp,"ZX Spectrum",11)!=0) return false;
  for(int i=0;i<cur;i++){
    if(!playlist_iterate(lfn,sizeof(lfn),fromPlayer)){
      playlist_close(fromPlayer);
      return false;
    }
  }
  return true;
}

void playlist_absolute_path(char* path, int path_size){
  int up_count=1;
  int con_offset=0;
  int i=0;
  while(i<path_size-3){
    if(/*memcmp(&path[i],"../",3)==0||*/memcmp(&path[i],":",1)==0){
      up_count++;
      i+=1;
      con_offset=i;
      continue;
    }
    i++;
  }
  char temp[MAX_PATH];
  memset(temp,0,sizeof(temp));
  // strncpy(temp,Config.active_dir,sizeof(temp)-1);
  while(up_count>0){
    for(i=sizeof(temp);i>=0;i++){
      if(temp[i]=='/'){
        temp[i]=0;
        break;
      }
    }
    up_count--;
  }
  // strncat(temp,"/",sizeof(temp)-1);
  strncat(temp,&path[con_offset],sizeof(temp)-1);
  strncpy(path,temp,path_size-1);
}

void playlist_file_name(char* path, int path_size){
  for(int i=path_size-1;i>=0;i--){
    if(path[i]=='/'){
      strcpy(path,&path[i+1]);
      break;
    }
  }
}
/*
void playlist_test()  {
  Serial.println("playlist test");
  strcpy(Config.active_dir,"/Tr_Songs/Albums/ATR Player/");
  if(playlist_open("/Tr_Songs/Albums/ATR Player/ATRV01.ayl")){
    char temp[256];
    while(playlist_iterate(temp,sizeof(temp))){
      Serial.println(temp);//original entry
      playlist_absolute_path(temp,sizeof(temp));
      Serial.println(temp);//absolute path
      playlist_file_name(temp,sizeof(temp));
      Serial.println(temp);//file name only
    }
    playlist_close();
  }else{
    Serial.println("Can't open file");
  }
}
*/
bool playlist_get_entry_full_path(int cur, char* path, int path_len, bool fromPlayer=false){
  if(!playlist_open(fromPlayer?Config.play_ayl_file:Config.ayl_file,cur)) return false;
  bool done=true;
  if(!playlist_iterate(path,path_len)){
    done=false;
  }
  playlist_close(fromPlayer);
  if(done){
    playlist_absolute_path(path,path_len);
  }
  return done;
}
