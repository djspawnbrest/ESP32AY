// zpplayer.h - the player screen of a module playing through General Sound,
// on zplayout.h's layout: its information at twice the size, the note bars,
// the playlist.  (Z-Player's own full screen, with the card's panel, the
// sample names and the pattern, was the first version; the user traded it
// for this on 28 Sep 2026 - retro-esp32ay-zplayer's
// .claude/docs/zplayer-look.md.)

static uint8_t zpBar[36],zpPeak[36];    // the note bars: height 0-40, peak
static uint8_t zpNote=0,zpVol=0x40;

static bool zpPlayerMine(){
  if(!zpSkin()) return false;
  return PlayerCTRL.screen_mode==SCR_PLAYER&&PlayerCTRL.music_type==TYPE_MOD&&GS.active&&zpMod.ok;
}

static const char *zpNoteNames[12]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

// What the card plays, as Z-Player 5.0 reads it: its own code on the card
// answers #EE every other frame with, per channel, CHREAL (the note
// sounding, 7Fh once the sample ended) and CHOLDV (the generator's last
// output byte, 80h silent) - read here straight from the card's RAM
// (retro-esp32ay-zplayer's .claude/docs/zplayer-look.md: "How Z-Player
// works").  A bar stands at the channel's note, as high as the channel is
// loud right now.
static void zpStrikes(){
  for(int ch=0;ch<4;ch++){
    uint16_t b=GS_CHANS+ch*GS_CHANLEN;
    uint8_t real=gs_peek(b+GS_CHREAL);
    int amp=(gs_peek(b+GS_CHMVOL)&0x7F)?abs((int)gs_peek(b+GS_CHOLDV)-0x80):0;
    if(real>=0x7F||!amp) continue;
    zpNote=real;
    zpVol=gs_peek(b+GS_CHVOL);
    int bar=real-36;                    // octave-3 starts at the card's note 36 (ProTracker's C-1)
    if(bar>=0&&bar<36){
      uint8_t h=amp>=128?40:amp*40/128;
      if(h>zpBar[bar]) zpBar[bar]=h;
      if(h>zpPeak[bar]) zpPeak[bar]=h;
    }
  }
}

static void zpPlayerStatic(){
  zpClear(ZX_BLACK);
  zplTop("for ZxPod \x7f General Sound on ESP32");
  zplOctaves(3,3);
  memset(zpBar,0,sizeof(zpBar)); memset(zpPeak,0,sizeof(zpPeak));
}

// upstream's player_screen() asks this first: true = drawn, upstream draws nothing
static bool zpPlayerDraw(){
  if(!zpPlayerMine()) return false;
  static uint32_t last;
  bool full=PlayerCTRL.scr_mode_update[SCR_PLAYER]||!zpOwns;
  if(!zpInit()) return false;
  full|=zpShow(1);
  zpClaim();
  if(full) zpPlayerStatic();
  else if(millis()-last<40) return true;  // 25 frames a second, as Z-Player
  last=millis();
  zpTick++;

  // before it sounds: the card's start (power-on), the load, the ROM's
  // preparing - the card's variables are not this module's yet, so "--"
  int pct; const char *wait=GS_Wait(&pct);
  int pos=gs_peek(GS_MTSNGPS),row=gs_peek(GS_MTPATPS)&63;
  int pattern=pos<128?zpMod.order[pos]:0;
  if(!wait) zpStrikes();

  char hdr[32];
  if(!wait) strcpy(hdr,"MOD Information:");
  else if(pct<100) snprintf(hdr,sizeof(hdr),"%s %s %d%%",wait,GS.state==GS_BOOTING?"the card":"module",pct);
  else snprintf(hdr,sizeof(hdr),"%s module...",wait);
  zplHeader(hdr);
  zplName(zpMod.title);
  if(wait) zplProgress(pct,100); else zplProgress(PlayerCTRL.trackFrame,AYInfo.Length);
  char a[16],b[16];
  if(wait) strcpy(a,"--"); else snprintf(a,sizeof(a),"%02X/%02X",pos,zpMod.songLen);
  if(wait) strcpy(b,"--"); else snprintf(b,sizeof(b),"%02X",gs_peek(GS_MTSPEED));
  zplLine(0,"Position",a,"Speed",b);
  if(wait) strcpy(a,"--"); else snprintf(a,sizeof(a),"%02X:%02X",pattern,row);
  zplLine(1,"Pattern",a,"Mode",!wait?"GS":GS.state==GS_BOOTING?"BOOT":"LOAD");
  if(wait) strcpy(a,"---"); else snprintf(a,sizeof(a),"%s%d",zpNoteNames[zpNote%12],zpNote/12);
  if(wait) strcpy(b,"--"); else snprintf(b,sizeof(b),"%02X",zpVol);
  zplLine(2,"Note",a,"Volume",b);
  snprintf(b,sizeof(b),"%uK",(unsigned)((zpMod.size+1023)/1024));
  zplLine(3,"Label",zpMod.label,"Size",b);
  zplTime(a,sizeof(a),PlayerCTRL.trackFrame);
  if(wait) snprintf(b,sizeof(b),"%d%%",pct); else zplTime(b,sizeof(b),AYInfo.Length);
  zplLine(4,"Time",a,!wait?"of":GS.state==GS_BOOTING?"Card":"Load",b,wait?ZX_BYELLOW:ZX_BCYAN);

  zplBars36(zpBar,zpPeak,4);
  zplPlaylist();
  zpFlush();
  return true;
}
