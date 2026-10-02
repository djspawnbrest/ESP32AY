// zplayout.h - the player screens' layout, for use rather than likeness
// (the user, 28 Sep 2026): Z-Player's letters, its information block full
// width at twice the size, the note bars, and the playlist - the previous,
// the current and the next file, three times the size.  zpplayer.h (a GS
// module), zpay.h (the AY formats) and zpgen.h (the rest) fill it.
//
//   y   0- 39  the Z*PLAYER letters, a line under them
//   y  40- 55  <EXT> Information:              2x, the volume shows here
//   y  56- 71  Name:                           2x
//   y  72- 79  the progress bar
//   y  80-159  five lines, two pairs each      2x, 30 columns
//   y 160-207  the note bars
//   y 208-215  the octaves
//   y 220-295  previous / current / next      3x, 20 columns
//   y 302-317  n/total, mode, volume          2x

static uint32_t zpTick;                 // screen frames drawn

// a blue bar at 4x8 text rows (Set-Up, About)
static void zpHeader(int col,int row,int width,const char *s){
  zpFill(col*4,row*8,width*4,8,ZX_BLUE);
  zpText(col+1,row,s,ZX_BWHITE,ZX_BLUE);
}

#define ZPL_HDR   40
#define ZPL_NAME  56
#define ZPL_PROG  72
#define ZPL_INFO  80
#define ZPL_BARS  160
#define ZPL_OCT   208
#define ZPL_LIST  220
#define ZPL_FOOT  302

static void zplTop(const char *sub){
  zpBitmap((ZP_W-ZP_LOGO_W*2)/2,6,zpLogo,ZP_LOGO_W,ZP_LOGO_H,ZX_BWHITE,2);
  zpText((ZP_COLS-strlen(sub))/2,4,sub,ZX_BYELLOW,ZX_BLACK);
}

// the header bar: a volume change shows there for a second and a half
static void zplHeader(const char *normal){
  static int lastVol=-1; static uint32_t shown=0;
  static char last[40]="";
  int v=sdConfig.volume;
  if(lastVol<0) lastVol=v;
  if(v!=lastVol){ lastVol=v; shown=millis(); zpDirty|=0x3FULL<<6; }   // upstream's own popup landed at y 62-77
  char want[40];
  if(shown&&millis()-shown<1500) snprintf(want,sizeof(want),"Volume: %d/63",v);
  else{ shown=0; snprintf(want,sizeof(want),"%s",normal); }
  if(PlayerCTRL.scr_mode_update[SCR_PLAYER]) last[0]=0;
  if(strcmp(want,last)){
    strcpy(last,want);
    zpFill(0,ZPL_HDR,ZP_W,16,ZX_BLUE);
    zpTextS(4,ZPL_HDR,want,ZX_BWHITE,ZX_BLUE,2,29);
  }
}

static void zplName(const char *name){
  zpTextS(0,ZPL_NAME,"Name:",ZX_BGREEN,ZX_BLACK,2);
  zpTextS(6*8,ZPL_NAME,name&&name[0]?name:"(no name)",ZX_BWHITE,ZX_BLACK,2,24);
}

static void zplProgress(uint32_t frame,uint32_t length){
  int w=ZP_W-8;
  uint32_t len=length?length:1;
  int fill=frame>=len?w:(int)((uint64_t)frame*w/len);
  zpFrame(0,ZPL_PROG+1,ZP_W,6,ZX_BYELLOW);
  zpFill(4,ZPL_PROG+3,w,2,ZX_BLACK);
  zpFill(4,ZPL_PROG+3,fill,2,ZX_BYELLOW);
}

// a line of two pairs: labels green, values cyan; 9+7 | 7+6 columns of 8 px
static void zplLine(int i,const char *l1,const char *v1,const char *l2,const char *v2,uint8_t v2ink=ZX_BCYAN){
  int y=ZPL_INFO+i*16;
  zpTextS(0,y,l1,ZX_BGREEN,ZX_BLACK,2,9);
  zpTextS(9*8,y,v1,ZX_BCYAN,ZX_BLACK,2,8);
  zpTextS(17*8,y,l2,ZX_BGREEN,ZX_BLACK,2,7);
  zpTextS(24*8,y,v2,v2ink,ZX_BLACK,2,6);
}

static void zplTime(char *out,int n,uint32_t frames){
  uint32_t s=frames/50;
  snprintf(out,n,"%u:%02u",(unsigned)(s/60),(unsigned)(s%60));
}

// octave labels under the bars: n of them across the width, from `first`
static void zplOctaves(int first,int n){
  int w=ZP_W/n;
  for(int o=0;o<n;o++){
    zpFill(o*w+1,ZPL_OCT,w-2,8,ZX_BLUE);
    char s[12];
    if(n<=3) snprintf(s,sizeof(s),"octave-%d",first+o); else snprintf(s,sizeof(s),"%d",first+o);
    zpText((o*w+(w-strlen(s)*4)/2)/4,ZPL_OCT/8,s,ZX_BCYAN,ZX_BLUE);
  }
}

// the 36 note bars of octaves 3-5 (zpBar/zpPeak, 0-40), falling
static void zplBars36(uint8_t *bar,uint8_t *peak){
  int h=48;
  for(int n=0;n<36;n++){
    int x=n*ZP_W/36+1,w=ZP_W/36-2;
    zpFill(x,ZPL_BARS,w,h,ZX_BLACK);
    int b=bar[n]*h/40,p=peak[n]*h/40;
    if(b) zpFill(x,ZPL_BARS+h-b,w,b,ZX_BYELLOW);
    if(p) zpFill(x,ZPL_BARS+h-p-2,w,1,ZX_BYELLOW);
    if(bar[n]>2) bar[n]-=2; else bar[n]=0;
    if(peak[n]&&(zpTick&3)==0) peak[n]--;
  }
}

//------------------------------------------------------------------------
// The playlist: previous, current, next - read when the track changes
//------------------------------------------------------------------------
static char zplList[3][64];
static int zplKeyCur=-1,zplKeyMode=-1,zplKeyCount=-1;
static bool zplKeyAyl;

static void zplBase(char *out,const char *path){
  const char *b=strrchr(path,'/'); b=b?b+1:path;
  strncpy(out,b,63); out[63]=0;
}

// the playing list's entries idx (0..2 of want[]), as file names
static void zplRead(const int *want,char (*out)[64]){
  for(int k=0;k<3;k++) out[k][0]=0;
  xSemaphoreTake(sdCardSemaphore,portMAX_DELAY);
  if(sdConfig.isPlayAYL){                       // an AYL: its entries, as playlist_iterate() counts them
    FsFile f;
    if(f.open(sdConfig.play_ayl_file,O_RDONLY)){
      char line[MAX_PATH]; int idx=0; bool skip=false;
      while(f.fgets(line,sizeof(line))>0){
        if(line[0]=='<'){ skip=true; continue; }
        if(line[0]=='>'){ skip=false; continue; }
        if(skip) continue;
        line[strcspn(line,"\r\n")]=0;
        int t=browser_check_ext(line);
        if(t==TYPE_UNK||t==TYPE_AYL) continue;
        for(char *p=line;*p;p++) if(*p=='\\') *p='/';
        for(int k=0;k<3;k++) if(want[k]==idx) zplBase(out[k],line);
        if(++idx>want[0]&&idx>want[1]&&idx>want[2]) break;
      }
      f.close();
    }
  }else{                                        // a directory: sort_list_play's order
    FsFile d,f;
    if(d.open(sdConfig.play_dir,O_RDONLY)){
      for(int k=0;k<3;k++){
        int idx=want[k];
        if(idx<0||idx>=sdConfig.play_count_files) continue;
        if(f.open(&d,sort_list_play[idx].file_id,O_RDONLY)){ f.getName(out[k],64); f.close(); }
      }
      d.close();
    }
  }
  xSemaphoreGive(sdCardSemaphore);
}

static void zplPlaylist(){
  int cur=sdConfig.play_cur,first=sdConfig.play_cur_start,count=sdConfig.play_count_files;
  bool changed=cur!=zplKeyCur||lfsConfig.play_mode!=zplKeyMode||count!=zplKeyCount||sdConfig.isPlayAYL!=zplKeyAyl;
  if(changed||PlayerCTRL.scr_mode_update[SCR_PLAYER]){
    zplKeyCur=cur; zplKeyMode=lfsConfig.play_mode; zplKeyCount=count; zplKeyAyl=sdConfig.isPlayAYL;
    int prev=cur-1<first?count-1:cur-1,next=cur+1>=count?first:cur+1;
    if(lfsConfig.play_mode==PLAY_MODE_ONE) next=cur;
    int want[3]={prev,cur,next};
    zplRead(want,zplList);
    if(lfsConfig.play_mode==PLAY_MODE_SHUFFLE) strcpy(zplList[2],"(shuffle)");
    if(count-first<=1){ zplList[0][0]=0; if(lfsConfig.play_mode!=PLAY_MODE_ONE) zplList[2][0]=0; }
    // the three lines: the current on the blue bar, its neighbours dimmer
    zpFill(0,ZPL_LIST-2,ZP_W,78,ZX_BLACK);
    zpTextS(0,ZPL_LIST,zplList[0],ZX_WHITE,ZX_BLACK,3,20);
    zpFill(0,ZPL_LIST+25,ZP_W,26,ZX_BLUE);
    zpTextS(0,ZPL_LIST+26,zplList[1],ZX_BWHITE,ZX_BLUE,3,20);
    zpTextS(0,ZPL_LIST+52,zplList[2],ZX_WHITE,ZX_BLACK,3,20);
  }
  // the foot: the place in the list, the mode, the volume
  static const char *modes[3]={"ONE","ALL","RND"};
  const char *m=!PlayerCTRL.isPlay?((millis()/500)&1?"PAUSE":""):modes[lfsConfig.play_mode%3];
  char foot[32];
  snprintf(foot,sizeof(foot),"%d/%d",count>first?cur-first+1:0,count>first?count-first:0);
  zpTextS(0,ZPL_FOOT,foot,ZX_BYELLOW,ZX_BLACK,2,11);
  zpTextS(12*8,ZPL_FOOT,m,ZX_BGREEN,ZX_BLACK,2,6);
  zpTextSf(19*8,ZPL_FOOT,ZX_BCYAN,ZX_BLACK,2,11,"Vol %d",sdConfig.volume);
}
