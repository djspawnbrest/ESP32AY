// zplayout.h - the player screens' layout, for use rather than likeness
// (the user, 28 Sep 2026): Z-Player's letters, its information block full
// width at twice the size, the note bars, and the playlist - the previous,
// the current and the next file, three times the size.  zpplayer.h (a GS
// module), zpay.h (the AY formats) and zpgen.h (the rest) fill it.
//
//   y   0- 39  the Z*PLAYER letters, a line under them
//   y  40- 55  <EXT> Information:  date time   2x, the volume shows here
//   y  56- 71  Name:                           2x, scrolls when longer
//   y  72- 79  the progress bar
//   y  80-159  five lines, two pairs each      2x, 30 columns
//   y 160-207  the note bars
//   y 208-215  the octaves
//   y 220-295  previous / current / next      3x, 20 columns, scroll when longer
//   y 302-317  n/total, mode, volume          2x

static uint32_t zpTick;                 // screen frames drawn

// A line longer than its field scrolls as upstream's scrollString() does
// (browser.h): to its end a pixel of the font at a time, a second's rest
// (S_UPD_DIR), back to its start, a rest - here a step a screen frame
// (25 a second), in the framebuffer.  zpScrollText() draws only when the
// line has moved, or the screen was cleared; zpScrollReset() starts it
// again from the left.
struct ZpScroll{ int pos; bool back; uint32_t rest,clears; char text[MAX_PATH]; };

static void zpScrollReset(ZpScroll &sc){ sc.pos=0; sc.back=false; sc.rest=millis(); sc.text[0]=0; }

// `s` at x,y, `scale` times the 4x8 cells, in a field of `width` cells
static void zpScrollText(ZpScroll &sc,int x,int y,const char *s,uint8_t ink,uint8_t paper,int scale,int width,bool redraw){
  if(strcmp(sc.text,s)){                // another line: from its start
    zpScrollReset(sc);
    strncpy(sc.text,s,sizeof(sc.text)-1); sc.text[sizeof(sc.text)-1]=0;
    redraw=true;
  }
  if(sc.clears!=zpClears){ sc.clears=zpClears; redraw=true; }
  uint8_t g[MAX_PATH]; int n=0;         // the glyphs, as zpTextS maps them
  for(const char *p=sc.text;*p&&n<(int)sizeof(g);n++){
    uint8_t c=(uint8_t)*p++;
    if(c==0xC2&&(uint8_t)*p==0xB7){ c=0x7F; p++; }
    else if(c>=0x80) c='?';
    g[n]=c;
  }
  int cw=4*scale,field=width*cw,len=n*cw;
  // it fits - or the field is not whole bytes on the screen: as it is
  if(len<=field||(x|field)&1||y<0||y+8*scale>ZP_H||x+field>ZP_W){
    if(redraw) zpTextS(x,y,sc.text,ink,paper,scale,width);
    return;
  }
  int last=sc.pos;
  if(millis()-sc.rest>=S_UPD_DIR){
    sc.rest=0;
    if(!sc.back){ sc.pos+=scale; if(sc.pos>=len-field){ sc.pos=len-field; sc.back=true; sc.rest=millis(); } }
    else{ sc.pos-=scale; if(sc.pos<=0){ sc.pos=0; sc.back=false; sc.rest=millis(); } }
  }
  if(sc.pos==last&&!redraw) return;
  // a row of the font at a time, straight into the framebuffer's bytes,
  // then copied for the scale's other rows: a pixel at a time (zpFill)
  // it took long enough to slow the UI loop, and the encoder's clicks
  // with it (`x` and the field are even, two pixels a byte)
  for(int r=0;r<8;r++){
    int y0=y+r*scale;
    uint8_t *row=zpFb+(y0*ZP_W+x)/2;
    for(int px=0;px<field;px+=2){
      int sx=sc.pos+px;
      uint8_t a=(zpFont[g[sx/cw]&0x7F][r]&(8>>((sx%cw)/scale)))?ink:paper;
      sx++;
      uint8_t b=(zpFont[g[sx/cw]&0x7F][r]&(8>>((sx%cw)/scale)))?ink:paper;
      row[px/2]=a<<4|b;
    }
    for(int k=1;k<scale;k++) memcpy(row+k*ZP_W/2,row,field/2);
  }
  for(int st=y>>3;st<=(y+8*scale-1)>>3;st++) zpDirty|=1ULL<<st;
}

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

// the header bar: a volume change shows there for a second and a half.
// At its right the date and the time, when Date & time says "Show clock"
// (upstream's showClock(), the colon flashing as there); the time alone
// when the header's own text leaves no room for the date.
static void zplHeader(const char *normal){
  static int lastVol=-1; static uint32_t shown=0;
  static char last[40]="",lastClk[16]="";
  static uint32_t lastClears;
  int v=sdConfig.volume;
  if(lastVol<0) lastVol=v;
  if(v!=lastVol){ lastVol=v; shown=millis(); zpDirty|=0x3FULL<<6; }   // upstream's own popup landed at y 62-77
  char want[40];
  if(shown&&millis()-shown<1500) snprintf(want,sizeof(want),"Volume: %d/63",v);
  else{ shown=0; snprintf(want,sizeof(want),"%s",normal); }
  char clk[16]="";
  int len=strlen(want);
  if(lfsConfig.showClock&&foundRtc&&len<=23){
    const char *colon=(millis()/500)&1?" ":":";
    if(len<=16) snprintf(clk,sizeof(clk),"%02d %.3s %02d%s%02d",now.day(),monthsOfTheYear[now.month()%13],now.hour(),colon,now.minute());
    else snprintf(clk,sizeof(clk),"%02d%s%02d",now.hour(),colon,now.minute());
  }
  if(PlayerCTRL.scr_mode_update[SCR_PLAYER]||lastClears!=zpClears){ last[0]=0; lastClears=zpClears; }
  if(strcmp(want,last)||strcmp(clk,lastClk)){
    strcpy(last,want); strcpy(lastClk,clk);
    zpFill(0,ZPL_HDR,ZP_W,16,ZX_BLUE);
    zpTextS(4,ZPL_HDR,want,ZX_BWHITE,ZX_BLUE,2,29);
    if(clk[0]) zpTextS(ZP_W-4-strlen(clk)*8,ZPL_HDR,clk,ZX_BYELLOW,ZX_BLUE,2);
  }
}

static ZpScroll zplNameScroll;

static void zplName(const char *name){
  zpTextS(0,ZPL_NAME,"Name:",ZX_BGREEN,ZX_BLACK,2);
  zpScrollText(zplNameScroll,6*8,ZPL_NAME,name&&name[0]?name:"(no name)",ZX_BWHITE,ZX_BLACK,2,24,PlayerCTRL.scr_mode_update[SCR_PLAYER]);
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

// the 36 note bars of octaves 3-5 (bar/peak, 0-40), falling: a bar 2 a
// frame, a peak 1 every `peakEvery` frames.  The card's bars are struck
// again every frame while a note sounds, so its peaks may hang (4); where a
// note's bar is not, a peak that hung on would be left alone in the air (1).
static void zplBars36(uint8_t *bar,uint8_t *peak,int peakEvery){
  int h=48;
  for(int n=0;n<36;n++){
    int x=n*ZP_W/36+1,w=ZP_W/36-2;
    zpFill(x,ZPL_BARS,w,h,ZX_BLACK);
    int b=bar[n]*h/40,p=peak[n]*h/40;
    if(b) zpFill(x,ZPL_BARS+h-b,w,b,ZX_BYELLOW);
    if(p) zpFill(x,ZPL_BARS+(p+2>h?0:h-p-2),w,1,ZX_BYELLOW);   // the peak's mark, in the bars' own rows
    if(bar[n]>2) bar[n]-=2; else bar[n]=0;
    if(peak[n]&&zpTick%peakEvery==0) peak[n]--;
  }
}

//------------------------------------------------------------------------
// The playlist: previous, current, next - read when the track changes
//------------------------------------------------------------------------
static char zplList[3][MAX_PATH];
static ZpScroll zplListScroll[3];
static int zplKeyCur=-1,zplKeyMode=-1,zplKeyCount=-1;
static uint32_t zplKeyClears;
static bool zplKeyAyl;

static void zplBase(char *out,const char *path){
  const char *b=strrchr(path,'/'); b=b?b+1:path;
  strncpy(out,b,MAX_PATH-1); out[MAX_PATH-1]=0;
}

// the playing list's entries idx (0..2 of want[]), as file names
static void zplRead(const int *want,char (*out)[MAX_PATH]){
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
        if(f.open(&d,sort_list_play[idx].file_id,O_RDONLY)){ f.getName(out[k],MAX_PATH); f.close(); }
      }
      d.close();
    }
  }
  xSemaphoreGive(sdCardSemaphore);
}

static void zplPlaylist(){
  int cur=sdConfig.play_cur,first=sdConfig.play_cur_start,count=sdConfig.play_count_files;
  bool changed=cur!=zplKeyCur||lfsConfig.play_mode!=zplKeyMode||count!=zplKeyCount||sdConfig.isPlayAYL!=zplKeyAyl||zpClears!=zplKeyClears;
  bool full=changed||PlayerCTRL.scr_mode_update[SCR_PLAYER];
  if(full){
    zplKeyCur=cur; zplKeyMode=lfsConfig.play_mode; zplKeyCount=count; zplKeyAyl=sdConfig.isPlayAYL; zplKeyClears=zpClears;
    int prev=cur-1<first?count-1:cur-1,next=cur+1>=count?first:cur+1;
    if(lfsConfig.play_mode==PLAY_MODE_ONE) next=cur;
    int want[3]={prev,cur,next};
    zplRead(want,zplList);
    if(lfsConfig.play_mode==PLAY_MODE_SHUFFLE) strcpy(zplList[2],"(shuffle)");
    if(count-first<=1){ zplList[0][0]=0; if(lfsConfig.play_mode!=PLAY_MODE_ONE) zplList[2][0]=0; }
    // the current on the blue bar, its neighbours dimmer
    zpFill(0,ZPL_LIST-2,ZP_W,78,ZX_BLACK);
    zpFill(0,ZPL_LIST+25,ZP_W,26,ZX_BLUE);
    for(int k=0;k<3;k++) zpScrollReset(zplListScroll[k]);
  }
  // the three lines, each scrolling when longer than the screen
  zpScrollText(zplListScroll[0],0,ZPL_LIST,zplList[0],ZX_WHITE,ZX_BLACK,3,20,full);
  zpScrollText(zplListScroll[1],0,ZPL_LIST+26,zplList[1],ZX_BWHITE,ZX_BLUE,3,20,full);
  zpScrollText(zplListScroll[2],0,ZPL_LIST+52,zplList[2],ZX_WHITE,ZX_BLACK,3,20,full);
  // the foot: the place in the list, the mode, the volume
  static const char *modes[3]={"ONE","ALL","RND"};
  const char *m=!PlayerCTRL.isPlay?((millis()/500)&1?"PAUSE":""):modes[lfsConfig.play_mode%3];
  char foot[32];
  snprintf(foot,sizeof(foot),"%d/%d",count>first?cur-first+1:0,count>first?count-first:0);
  zpTextS(0,ZPL_FOOT,foot,ZX_BYELLOW,ZX_BLACK,2,11);
  zpTextS(12*8,ZPL_FOOT,m,ZX_BGREEN,ZX_BLACK,2,6);
  zpTextSf(19*8,ZPL_FOOT,ZX_BCYAN,ZX_BLACK,2,11,"Vol %d",sdConfig.volume);
}
