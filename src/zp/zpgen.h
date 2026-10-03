// zpgen.h - the player screen of what the ESP plays itself (S3M, XM, the
// MODs the card does not take, MP3, WAV, TAP, TZX), on zplayout.h's layout.
// The real play is upstream's: the note bins its players fill (bufEQ, 96
// notes of the AY table's), read here and let fall as its fastEQ() did.
// A MOD is the exception: its player fills the bins once a row, and only for
// a period found exactly in its table (no slide, no vibrato, no finetune),
// which left them nearly still - so its channels are read every frame
// instead, as the card's are (zpplayer.h), on the same 36 bars.

#include <math.h>
#include <ctype.h>

extern int tap_cur_block,tzx_cur_block,tap_total_blocks,tzx_total_blocks;

static uint8_t zpgPeak[96];
static uint8_t zpgBar36[36],zpgPeak36[36];   // a MOD's bars (zplBars36)

static bool zpgMod(){ return PlayerCTRL.music_type==TYPE_MOD&&mod; }

// each sounding channel of upstream's MOD player at its note, as loud as it
// is now - the sample's byte where it plays, as the card's bars take the
// generator's output byte (the card's volume is its DAC's, not in the byte):
// 856 is ProTracker's C-1, the first bar (octave-3, as the card has it)
static void zpgModStrikes(){
  for(int ch=0;ch<32;ch++){
    uint16_t period; uint8_t vol,level;
    if(!mod->getChannelNote(ch,&period,&vol,&level)) continue;
    int b=(int)lroundf(12.0f*log2f(856.0f/period));
    if(b<0||b>=36) continue;
    uint8_t h=level>=128?40:level*40/128;
    if(h>zpgBar36[b]) zpgBar36[b]=h;
    if(h>zpgPeak36[b]) zpgPeak36[b]=h;
  }
}
static uint32_t zpgSize;

static bool zpGenType(){
  switch(PlayerCTRL.music_type){
    case TYPE_S3M:
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    case TYPE_XM:
  #endif
    case TYPE_MP3: case TYPE_WAV: case TYPE_TAP: case TYPE_TZX:
      return true;
    case TYPE_MOD:
      return !GS.active;                // the card's modules are zpplayer.h's
  }
  return false;
}
static bool zpGenMine(){
  if(!zpSkin()) return false;
  return PlayerCTRL.screen_mode==SCR_PLAYER&&zpGenType()&&lfsConfig.playerSource==PLAYER_MODE_SD;
}

static bool zpgAudio(){ return PlayerCTRL.music_type==TYPE_MP3||PlayerCTRL.music_type==TYPE_WAV; }
static bool zpgTape(){ return PlayerCTRL.music_type==TYPE_TAP||PlayerCTRL.music_type==TYPE_TZX; }
static int zpgChannels(){
  if(zpgAudio()||zpgTape()) return 2;
  int n=modChannelsEQ?modChannelsEQ:modChannels;
  return n<1?1:n>8?8:n;
}

static void zpGenStatic(){
  zpClear(ZX_BLACK);
  zplTop(zpgAudio()?"for ZxPod \x7f the ESP's own decoder":zpgTape()?"for ZxPod \x7f a tape for the Spectrum":"for ZxPod \x7f the ESP's own tracker");
  if(zpgMod()) zplOctaves(3,3); else zplOctaves(1,8);
  memset(zpgPeak,0,sizeof(zpgPeak));
  memset(zpgBar36,0,sizeof(zpgBar36)); memset(zpgPeak36,0,sizeof(zpgPeak36));
  zpgSize=0;
  FsFile f;
  xSemaphoreTake(sdCardSemaphore,portMAX_DELAY);
  if(f.open(playFileName,O_RDONLY)){ zpgSize=f.fileSize(); f.close(); }
  xSemaphoreGive(sdCardSemaphore);
}

static bool zpGenDraw(){
  if(!zpGenMine()) return false;
  static uint32_t last;
  bool full=PlayerCTRL.scr_mode_update[SCR_PLAYER]||!zpOwns;
  if(!zpInit()) return false;
  full|=zpShow(100+PlayerCTRL.music_type);   // its subtitle and octaves are the type's
  zpClaim();
  if(full) zpGenStatic();
  else if(millis()-last<40) return true;
  last=millis();
  zpTick++;

  char h[24]; const char *e=file_ext_list[PlayerCTRL.music_type];
  snprintf(h,sizeof(h),"%s Information:",e);
  for(char *p=h;*p&&*p!=' ';p++) *p=toupper(*p);
  zplHeader(h);
  const char *name=AYInfo.Name[0]?AYInfo.Name:(strrchr(playFileName,'/')?strrchr(playFileName,'/')+1:playFileName);
  zplName(name);
  zplProgress(PlayerCTRL.trackFrame,AYInfo.Length>0?AYInfo.Length:0);
  char a[16],b[16];
  zplTime(a,sizeof(a),PlayerCTRL.trackFrame);
  if(AYInfo.Length>0) zplTime(b,sizeof(b),AYInfo.Length); else strcpy(b,"--");
  zplLine(0,"Time",a,"of",b);
  if(zpgTape()){
    int cur=PlayerCTRL.music_type==TYPE_TAP?tap_cur_block:tzx_cur_block,tot=PlayerCTRL.music_type==TYPE_TAP?tap_total_blocks:tzx_total_blocks;
    snprintf(a,sizeof(a),"%d/%d",cur+1>tot?tot:cur+1,tot);
    zplLine(1,"Block",a,"","");
  }else{
    snprintf(a,sizeof(a),"%d",zpgAudio()?2:modChannels);
    zplLine(1,"Channels",a,"","");
  }
  static const char *sep[3]={"Full","Half","Mono"};
  snprintf(b,sizeof(b),"%d%%",(int)roundf(lfsConfig.dacGain*100));
  zplLine(2,"Stereo",sep[lfsConfig.modStereoSeparation%3],"Gain",b);
  if(PlayerCTRL.music_type==TYPE_MP3) snprintf(a,sizeof(a),"%s%dk",isVBR?"~":"",bitrate); else strcpy(a,"44.1kHz");
  snprintf(b,sizeof(b),"%uK",(unsigned)((zpgSize+1023)/1024));
  zplLine(3,"Rate",a,"Size",b);
  char lab[8]; snprintf(lab,sizeof(lab),"%s",e); for(char *p=lab;*p;p++) *p=toupper(*p);
  zplLine(4,"Label",lab,"Chan",zpgChannels()>2?"multi":"L/R");

  if(zpgMod()){                         // a MOD: its channels, as the card's
    zpgModStrikes();
    zplBars36(zpgBar36,zpgPeak36,4);
  }else{
    // the bins: 96 notes of 2 px across 8 octaves, falling as fastEQ() let
    // them; a bin is set once and falls, so its peak falls with it - a peak
    // that hung on would be left alone in the air
    int hh=48;
    for(int i=0;i<96;i++){
      int x=i*ZP_W/96;
      uint8_t v=bufEQ[i];
      int hgt=v*hh/16; if(hgt>hh) hgt=hh;
      if(hgt>zpgPeak[i]) zpgPeak[i]=hgt;
      zpFill(x,ZPL_BARS,2,hh,ZX_BLACK);
      if(hgt) zpFill(x,ZPL_BARS+hh-hgt,2,hgt,ZX_BYELLOW);
      if(zpgPeak[i]) zpFill(x,ZPL_BARS+(zpgPeak[i]+2>hh?0:hh-zpgPeak[i]-2),2,1,ZX_BYELLOW);
      if(bufEQ[i]) bufEQ[i]--;
      if(zpgPeak[i]) zpgPeak[i]--;
    }
  }
  for(int ch=0;ch<8;ch++) if(modEQchn[ch]) modEQchn[ch]--;   // upstream's fastEQ() let these fall too

  zplPlaylist();
  zpFlush();
  return true;
}
