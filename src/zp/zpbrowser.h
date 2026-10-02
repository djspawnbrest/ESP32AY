// zpbrowser.h - the file browser, for use rather than likeness (the user,
// 28 Sep 2026): twice the size of Z-Player's font - a line a file, name,
// extension and size; the cursor line inverted; the path and the place in
// the list on the blue bar above; the file under the cursor's own title
// (MOD, S3M, XM, PT3, PT2 headers) on the line below.  Upstream's
// browser_screen() still owns the list, the cursor and the keys.
//
//   y   0- 39  the Z*PLAYER letters
//   y  40- 55  the path | n/total              2x, on blue
//   y  56-311  16 lines                        2x, 30 columns
//   y  312-319 the sort, the cursor file's title

#define ZPB_TOP   40
#define ZPB_LINES 16


static bool zpBrowserMine(){ return zpSkin()&&PlayerCTRL.screen_mode==SCR_BROWSER; }

// a file's own name for itself, from its header: MOD, S3M, XM, PT3, PT2
static void zpbReadTitle(FsFile &f,const char *name,char *out){
  out[0]=0;
  uint8_t h[128];
  int type=browser_check_ext(name),off=-1,len=0;
  switch(type){
    case TYPE_MOD: off=0; len=20; break;
    case TYPE_S3M: off=0; len=28; break;
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    case TYPE_XM:  off=17; len=20; break;
  #endif
    case TYPE_PT3: off=0x1E; len=32; break;
    case TYPE_PT2: off=101; len=30; break;
    default: return;
  }
  if(!f.seekSet(off)||f.read(h,len)!=len) return;
  int n=0;
  for(int i=0;i<len&&n<23;i++){
    uint8_t c=h[i];
    if(!c) break;
    out[n++]=(c>=0x20&&c<0x7F)?c:' ';
  }
  while(n&&out[n-1]==' ') n--;
  out[n]=0;
  int s=0; while(out[s]==' ') s++;
  if(s) memmove(out,out+s,n-s+1);
}

// one line at 2x: name 21 columns, extension 3, size 5; the cursor inverted
static void zpbLine(int i,const char *name,bool dir,bool ayl,uint32_t size,bool cursor,bool playing){
  char nm[64],ext[8]="";
  strncpy(nm,name,sizeof(nm)-1); nm[sizeof(nm)-1]=0;
  if(!dir){
    char *dot=strrchr(nm,'.');
    if(dot&&dot!=nm&&strlen(dot)<=5){ strcpy(ext,dot+1); *dot=0; }
    for(char *p=ext;*p;p++) *p=tolower(*p);
  }
  int y=ZPB_TOP+16+i*16;
  uint8_t pName=cursor?ZX_BWHITE:ZX_BLACK,pSize=cursor?ZX_BCYAN:ZX_BLACK;
  uint8_t iName=cursor?ZX_BLACK:(playing?ZX_BGREEN:(dir?ZX_BYELLOW:ayl?ZX_BCYAN:ZX_BWHITE));
  zpTextS(0,y,nm,iName,pName,2,21);
  zpTextS(21*8,y," ",iName,pName,2,1);
  zpTextS(22*8,y,dir?"dir":ext,iName,pName,2,3);
  char sz[8];
  if(dir||ayl) strcpy(sz,"");
  else if(size<10000) snprintf(sz,sizeof(sz),"%5u",(unsigned)size);
  else if(size<10240000) snprintf(sz,sizeof(sz),"%4uK",(unsigned)(size/1024));
  else snprintf(sz,sizeof(sz),"%4uM",(unsigned)(size/1048576));
  zpTextS(25*8,y,sz,cursor?ZX_BLACK:ZX_BCYAN,pSize,2,5);
}

// upstream's browser_screen() asks this first: true = drawn
bool zpBrowserDraw(int mode){
  if(!zpBrowserMine()) return false;
  if(!zpInit()) return false;
  bool full=!zpOwns;
  zpClaim();
  if(!full&&!PlayerCTRL.scr_mode_update[SCR_BROWSER]) return true;

  zpClear(ZX_BLACK);
  zpBitmap((ZP_W-ZP_LOGO_W*2)/2,6,zpLogo,ZP_LOGO_W,ZP_LOGO_H,ZX_BWHITE,2);
  const char *hdr=mode==BROWSE_DIR?(strcmp(sdConfig.active_dir,"/")?sdConfig.active_dir:"SD:/"):sdConfig.ayl_file;
  char pos[16];
  snprintf(pos,sizeof(pos),"%d/%d",sort_list_len?sdConfig.dir_cur+1:0,sort_list_len);
  int pl=strlen(pos),room=30-pl-1;
  const char *h=hdr; if((int)strlen(h)>room) h+=strlen(h)-room;   // the path's tail, if long
  zpFill(0,ZPB_TOP,ZP_W,16,ZX_BLUE);
  zpTextS(0,ZPB_TOP,h,ZX_BWHITE,ZX_BLUE,2,room);
  zpTextS((30-pl)*8,ZPB_TOP,pos,ZX_BYELLOW,ZX_BLUE,2);

  int id=sdConfig.dir_cur-ZPB_LINES/2;
  if(id>sort_list_len-ZPB_LINES) id=sort_list_len-ZPB_LINES;
  if(id<0) id=0;
  char title[24]="";
  if(mode==BROWSE_DIR){
    FsFile dir,f;
    xSemaphoreTake(sdCardSemaphore,portMAX_DELAY);
    bool ok=dir.open(sdConfig.active_dir,O_RDONLY);
    for(int i=0;i<ZPB_LINES&&ok&&id<sort_list_len;i++,id++){
      char name[MAX_PATH]="";
      if(!f.open(&dir,sort_list[id].file_id,O_RDONLY)) continue;
      f.getName(name,sizeof(name));
      bool isdir=sort_list[id].hash[0]==1;
      bool ayl=!isdir&&browser_check_ext(name)==TYPE_AYL;
      if(sdConfig.dir_cur==id&&!isdir&&!ayl) zpbReadTitle(f,name,title);
      f.close();
      bool playing=!isdir&&!strcmp(sdConfig.active_dir,sdConfig.play_dir)&&sdConfig.play_cur==id&&!sdConfig.isPlayAYL;
      zpbLine(i,name,isdir,ayl,sort_list[id].file_size,sdConfig.dir_cur==id,playing);
    }
    if(ok) dir.close();
    xSemaphoreGive(sdCardSemaphore);
  }else{
    browser_ayl_draw_begin(id);
    for(int i=0;i<ZPB_LINES&&id<sort_list_len;i++,id++){
      playlist_iterate(lfn,sizeof(lfn));
      playlist_file_name(lfn,sizeof(lfn));
      bool playing=!strcmp(sdConfig.ayl_file,sdConfig.play_ayl_file)&&sdConfig.play_cur==id&&sdConfig.isPlayAYL;
      zpbLine(i,lfn,false,false,0,sdConfig.dir_cur==id,playing);
    }
    browser_ayl_draw_end();
  }
  zpText(0,39,mode==BROWSE_DIR?sort_names[sdConfig.browser_sort]:"AYL",ZX_BYELLOW,ZX_BLACK,6);
  zpText(7,39,title,ZX_BMAGENTA,ZX_BLACK,53);
  PlayerCTRL.scr_mode_update[SCR_BROWSER]=false;
  zpFlush();
  return true;
}
