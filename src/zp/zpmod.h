// zpmod.h - what the Z-Player screen shows of a module, read from its file:
// the title, the sample names (the scene's message board), the order list,
// and every pattern, into PSRAM at open so the pattern view never waits
// for the card.  31-sample modules (M.K. and friends) and the old
// 15-sample ones, 4 channels - what General Sound plays.

struct ZPMod{
  bool ok=false;
  char title[21];
  char smp[31][23];
  uint8_t smpVol[31];
  int nsmp;
  uint8_t songLen,restart;
  uint8_t order[128];
  char label[5];
  int nPat;
  uint8_t *pat=NULL;                    // nPat x 64 rows x 4 channels x 4 bytes, PSRAM
  uint32_t size;
}zpMod;

// Amiga periods, ProTracker's octaves 1-3; Z-Player calls them 3-5
static const uint16_t zpPeriods[36]={
  856,808,762,720,678,640,604,570,538,508,480,453,
  428,404,381,360,340,320,302,285,269,254,240,226,
  214,202,190,180,170,160,151,143,135,127,120,113,
};

// the note (0-35) nearest a period, -1 for none
static int zpNoteOf(uint16_t period){
  if(!period) return -1;
  int best=0,d=0xffff;
  for(int i=0;i<36;i++){ int e=abs((int)zpPeriods[i]-(int)period); if(e<d){ d=e; best=i; } }
  return best;
}

struct ZPCell{ int note; uint8_t smp,fx,param; };

static ZPCell zpCell(int pattern,int row,int ch){
  ZPCell c={-1,0,0,0};
  if(!zpMod.pat||pattern>=zpMod.nPat) return c;
  const uint8_t *p=zpMod.pat+((pattern*64+row)*4+ch)*4;
  c.smp=(p[0]&0xF0)|(p[2]>>4);
  c.note=zpNoteOf(((p[0]&0x0F)<<8)|p[1]);
  c.fx=p[2]&0x0F;
  c.param=p[3];
  return c;
}

static void zpModFree(){
  if(zpMod.pat) heap_caps_free(zpMod.pat);
  zpMod.pat=NULL;
  zpMod.ok=false;
}

static void zpName(char *dst,const uint8_t *src,int n){
  for(int i=0;i<n;i++) dst[i]=(src[i]>=0x20&&src[i]<0x7F)?src[i]:(src[i]?'?':' ');
  dst[n]=0;
  for(int i=n-1;i>=0&&dst[i]==' ';i--) dst[i]=0;
}

// The module's header and patterns, from the whole file in memory (the
// card's copy, GSPlay.h): nothing more is read from the SD card.
static bool zpModParse(const uint8_t *h,uint32_t size){
  zpModFree();
  zpMod.size=size;
  if(size<1084) return false;
  bool m31=!memcmp(h+1080,"M.K.",4)||!memcmp(h+1080,"M!K!",4)||!memcmp(h+1080,"FLT4",4)||!memcmp(h+1080,"4CHN",4);
  zpMod.nsmp=m31?31:15;
  zpName(zpMod.title,h,20);
  for(int i=0;i<31;i++){
    if(i<zpMod.nsmp){ zpName(zpMod.smp[i],h+20+i*30,22); zpMod.smpVol[i]=h[20+i*30+25]; }
    else{ zpMod.smp[i][0]=0; zpMod.smpVol[i]=0; }
  }
  int base=20+zpMod.nsmp*30;            // 950 or 470
  zpMod.songLen=h[base];
  zpMod.restart=h[base+1];
  memcpy(zpMod.order,h+base+2,128);
  if(m31){ memcpy(zpMod.label,h+1080,4); zpMod.label[4]=0; } else strcpy(zpMod.label,"15sm");
  zpMod.nPat=0;
  for(int i=0;i<128;i++) if(zpMod.order[i]+1>zpMod.nPat) zpMod.nPat=zpMod.order[i]+1;
  uint32_t off=base+2+128+(m31?4:0),bytes=(uint32_t)zpMod.nPat*1024;
  if(off+bytes>size) return false;
  zpMod.pat=(uint8_t*)heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!zpMod.pat) return false;
  memcpy(zpMod.pat,h+off,bytes);
  zpMod.ok=true;
  return true;
}

// the whole file, then zpModParse (the host's preview)
static bool zpModOpen(const char *path){
  FsFile f;
  xSemaphoreTake(sdCardSemaphore,portMAX_DELAY);
  bool ok=f.open(path,O_RDONLY);
  uint32_t size=ok?f.fileSize():0;
  uint8_t *buf=ok&&size?(uint8_t*)malloc(size):NULL;
  ok=buf&&f.read(buf,size)==(int)size;
  f.close();
  xSemaphoreGive(sdCardSemaphore);
  ok=ok&&zpModParse(buf,size);
  free(buf);
  return ok;
}

// The song's length in 50 Hz frames, as upstream's AudioGeneratorMOD::
// getPlaybackTime() works it out (OpenMPT's GetLength: Fxx, Bxx, Dxx, E6x,
// EEx, stopping at a row already played) - but from the patterns in
// memory, not a seek and a read of the SD card for every row.
static uint32_t zpModLength(){
  if(!zpMod.ok||!zpMod.songLen) return 0;
  int songLen=zpMod.songLen>128?128:zpMod.songLen;
  static uint8_t visited[128][8];       // a bit a row
  static uint32_t loopStates[512];
  memset(visited,0,sizeof(visited));
  int nStates=0;
  uint8_t loopCount[4]={0},loopRow[4]={0};
  float secs=0,bpm=125;
  int speed=6,order=0,row=0,rowsInLoops=0;
  while(order<songLen){
    if(rowsInLoops>32768) break;        // an endless loop: upstream calls it endless too
    uint16_t hash=0; bool loops=false;
    for(int ch=0;ch<4;ch++) if(loopCount[ch]){ hash^=loopCount[ch]<<(ch*2); loops=true; }
    if(!loops){
      if(visited[order][row>>3]&(1<<(row&7))) break;
    }else{
      uint32_t key=(uint32_t)order<<24|(uint32_t)row<<16|hash;
      bool seen=false;
      for(int i=0;i<nStates;i++) if(loopStates[i]==key){ seen=true; break; }
      if(seen||nStates>=512) break;
      loopStates[nStates++]=key;
    }
    visited[order][row>>3]|=1<<(row&7);
    int pattern=zpMod.order[order];
    bool brk=false,jump=false; int nextOrder=-1,nextRow=-1,pendingLoop=-1,delay=0;
    for(int ch=0;ch<4;ch++){
      ZPCell c=zpCell(pattern,row,ch);
      int x=c.param>>4,y=c.param&15;
      switch(c.fx){
        case 0xB: nextOrder=c.param>=songLen?0:c.param; nextRow=0; jump=true; break;
        case 0xD: nextRow=x*10+y>=64?0:x*10+y; if(!jump&&!brk) nextOrder=order+1; brk=true; break;
        case 0xE:
          if(x==6){
            if(!y) loopRow[ch]=row;
            else{
              if(loopCount[ch]){ if(!--loopCount[ch]) break; }
              else loopCount[ch]=y;
              pendingLoop=loopRow[ch];
              rowsInLoops++;
            }
          }else if(x==0xE) delay=1+y;
          break;
        case 0xF: if(c.param<32){ if(c.param) speed=c.param; } else bpm=c.param; break;
      }
    }
    secs+=2.5f*speed*(delay>0?delay:1)/bpm;
    if(pendingLoop>=0){ row=pendingLoop; continue; }
    if(brk||jump){
      order=nextOrder>=0?nextOrder:order+1;
      row=nextRow>=0?nextRow:0;
      continue;
    }
    if(++row>=64){ row=0; order++; }
  }
  return (uint32_t)(secs*50);
}
