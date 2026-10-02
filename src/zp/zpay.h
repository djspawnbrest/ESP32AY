// zpay.h - the player screen of the AY formats, on zplayout.h's layout:
// PT3/PT2 with their own position, pattern and speed; every AY-register
// format (PT1..YRG) with the chips' registers as they are written - each
// channel's tone at its volume on the note bars, the real play.

#include <math.h>
#include <ctype.h>


static uint8_t zpaBar[36],zpaPeak[36];

static bool zpAyType(){
  return PlayerCTRL.music_type>=TYPE_PT1&&PlayerCTRL.music_type<=TYPE_YRG;
}
static bool zpAyMine(){
  if(!zpSkin()) return false;
  return PlayerCTRL.screen_mode==SCR_PLAYER&&zpAyType()&&lfsConfig.playerSource==PLAYER_MODE_SD;
}
static bool zpConfigMine();
static bool zpAboutMine();
static bool zpMine(){ return zpPlayerMine()||zpAyMine()||zpGenMine()||zpBrowserMine()||zpConfigMine()||zpAboutMine(); }

static const uint8_t *zpaRegs(int chip){ return chip?ay_reg_1:ay_reg_2; }   // ay_write(): chip 0 -> ay_reg_2

// a channel's tone as a note, 0 = C-0; -1 for none
static int zpaNote(const uint8_t *r,int ch){
  int period=r[ch*2]|((r[ch*2+1]&0x0F)<<8);
  if(period<1) return -1;
  float f=(float)lfsConfig.ay_clock/(16.0f*period);
  if(f<16.0f) return -1;
  int n=(int)lroundf(12.0f*log2f(f/16.3516f));
  return n>=0&&n<108?n:-1;
}

// PT3/PT2's own state; their headers' #defines would mangle ->PT3 and friends
#pragma push_macro("PT3")
#pragma push_macro("PT2")
#pragma push_macro("PT2_Delay")
#pragma push_macro("PT2_NumberOfPositions")
#pragma push_macro("PT2_LoopPosition")
#pragma push_macro("PT2_PositionList")
#undef PT3
#undef PT2
#undef PT2_Delay
#undef PT2_NumberOfPositions
#undef PT2_LoopPosition
#undef PT2_PositionList
struct ZPAState{ bool known; int pos,npos,loop,pattern,speed; char label[8]; };
static ZPAState zpaState(){
  ZPAState s={false,0,0,0,0,0,""};
  if(PlayerCTRL.music_type==TYPE_PT3&&AYInfo.data&&AYInfo.module){
    PT3_SongInfo *si=(PT3_SongInfo*)AYInfo.data;
    PT3_File *h=(PT3_File*)AYInfo.module;
    s.known=true;
    s.pos=si->PT3.CurrentPosition; s.npos=h->PT3_NumberOfPositions; s.loop=h->PT3_LoopPosition;
    s.pattern=h->PT3_PositionList[s.pos<s.npos?s.pos:0]/3; s.speed=si->PT3.Delay;
    snprintf(s.label,sizeof(s.label),"PT3.%d%s",si->PT3.Version,AYInfo.is_ts?" TS":"");
  }else if(PlayerCTRL.music_type==TYPE_PT2&&AYInfo.data&&AYInfo.module){
    PT2_SongInfo *si=(PT2_SongInfo*)AYInfo.data;
    PT2_File *h=(PT2_File*)AYInfo.module;
    s.known=true;
    s.pos=si->PT2.CurrentPosition; s.npos=h->PT2_NumberOfPositions; s.loop=h->PT2_LoopPosition;
    s.pattern=h->PT2_PositionList[s.pos<s.npos?s.pos:0]; s.speed=si->PT2.Delay;
    strcpy(s.label,"PT2");
  }else{
    const char *e=file_ext_list[PlayerCTRL.music_type];
    for(int i=0;i<3&&e[i];i++) s.label[i]=toupper(e[i]);
    s.label[3]=0;
    if(AYInfo.is_ts) strcat(s.label," TS");
  }
  return s;
}
#pragma pop_macro("PT2_PositionList")
#pragma pop_macro("PT2_LoopPosition")
#pragma pop_macro("PT2_NumberOfPositions")
#pragma pop_macro("PT2_Delay")
#pragma pop_macro("PT2")
#pragma pop_macro("PT3")

static void zpAyStatic(){
  zpClear(ZX_BLACK);
  zplTop("for ZxPod \x7f two AY-3-8910 chips");
  zplOctaves(3,3);
  memset(zpaBar,0,sizeof(zpaBar)); memset(zpaPeak,0,sizeof(zpaPeak));
}

static bool zpAyDraw(){
  if(!zpAyMine()) return false;
  static uint32_t last;
  bool full=PlayerCTRL.scr_mode_update[SCR_PLAYER]||!zpOwns;
  if(!zpInit()) return false;
  zpClaim();
  if(full) zpAyStatic();
  else if(millis()-last<40) return true;
  last=millis();
  zpTick++;

  ZPAState st=zpaState();
  int nchips=AYInfo.is_ts?2:1;

  // the bars: every sounding channel's tone, at its volume - the chips' registers as written
  int loudest=0,lastNote=-1;
  for(int chip=0;chip<nchips;chip++){
    const uint8_t *r=zpaRegs(chip);
    for(int c=0;c<3;c++){
      if(r[7]&(1<<c)) continue;         // tone off
      int v=(r[8+c]&0x10)?15:(r[8+c]&15);
      int n=zpaNote(r,c);
      if(n<0||!v) continue;
      if(v>loudest){ loudest=v; lastNote=n; }
      int b=n-36;                       // octave-3 starts at C-3
      if(b<0||b>=36) continue;
      uint8_t h=v*40/15;
      if(h>zpaBar[b]) zpaBar[b]=h;
      if(h>zpaPeak[b]) zpaPeak[b]=h;
    }
  }

  char hdr[24]; const char *e=file_ext_list[PlayerCTRL.music_type];
  snprintf(hdr,sizeof(hdr),"%c%c%c Information:",toupper(e[0]),toupper(e[1]),toupper(e[2]));
  zplHeader(hdr);
  zplName(AYInfo.Name);
  zplProgress(PlayerCTRL.trackFrame,AYInfo.Length>0?AYInfo.Length:0);
  char a[16],b[16];
  if(st.known) snprintf(a,sizeof(a),"%02X/%02X",st.pos,st.npos); else strcpy(a,"--");
  if(st.known) snprintf(b,sizeof(b),"%02X",st.speed); else strcpy(b,"--");
  zplLine(0,"Position",a,"Speed",b);
  if(st.known) snprintf(a,sizeof(a),"%02X",st.pattern); else strcpy(a,"--");
  zplLine(1,"Pattern",a,"Chips",AYInfo.is_ts?"2 TS":"1");
  if(lastNote>=0) snprintf(a,sizeof(a),"%s%d",zpNoteNames[lastNote%12],lastNote/12); else strcpy(a,"---");
  snprintf(b,sizeof(b),"%X",loudest);
  zplLine(2,"Note",a,"Volume",b);
  snprintf(b,sizeof(b),"%uK",(unsigned)((AYInfo.file_len+1023)/1024));
  zplLine(3,"Label",st.label,"Size",b);
  zplTime(a,sizeof(a),PlayerCTRL.trackFrame);
  if(AYInfo.Length>0) zplTime(b,sizeof(b),AYInfo.Length); else strcpy(b,"--");
  zplLine(4,"Time",a,"of",b);

  zplBars36(zpaBar,zpaPeak);
  zplPlaylist();
  zpFlush();
  return true;
}
