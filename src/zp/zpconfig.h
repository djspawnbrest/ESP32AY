// zpconfig.h - upstream's settings in Z-Player's look ("Hold ENTER for
// Set-Up", its own page), at twice the size of its letters (the user, 28
// Sep): green labels, cyan values, the cursor line on the blue bar, the
// value being changed in yellow between < >.  Fifteen lines show; the list
// scrolls with the cursor.  Upstream's config_screen() keeps the items,
// their order (lfsConfig.cfg_cur 0-17) and the keys; this draws the same
// values.

static bool zpConfigMine(){ return zpSkin()&&PlayerCTRL.screen_mode==SCR_CONFIG; }

#define ZPC_TOP   56                    // the first line, under the header bar
#define ZPC_LINES 15                    // 16 px each: y 56-295

struct ZPCItem{ int idx; const char *label; char value[24]; };

// the items in upstream's order; value "" for the ones that open a page
static int zpcItems(ZPCItem *it){
  static const char *const sources[]={"SD","UART"};
  static const char *const modes[]={"Once","All","Shuffle"};
  static const char *const ints[]={"PENT 48.8Hz","ZX 50.0Hz"};
  static const char *const encs[]={"Normal","Reverse"};
  static const char *const pans[]={"Full Stereo","Half Stereo","Mono"};
  int n=0;
  auto add=[&](int idx,const char *label,const char *fmt,...){
    it[n].idx=idx; it[n].label=label;
    va_list ap; va_start(ap,fmt); vsnprintf(it[n].value,sizeof(it[n].value),fmt,ap); va_end(ap);
    n++;
  };
  add(0,"Player source","%s",sources[lfsConfig.playerSource&1]);
  add(1,"ZX INT","%s",ints[lfsConfig.zx_int&1]);
  add(2,"AY stereo","%s",ay_layout_names[lfsConfig.ay_layout]);
  switch(lfsConfig.ay_clock){
    case CLK_SPECTRUM: add(3,"AY clock","ZX 1.77MHz"); break;
    case CLK_PENTAGON: add(3,"AY clock","PEN 1.75MHz"); break;
    case CLK_MSX: add(3,"AY clock","MSX 1.78MHz"); break;
    case CLK_CPC: add(3,"AY clock","CPC 1.0MHz"); break;
    case CLK_ATARIST: add(3,"AY clock","ST 2.0MHz"); break;
    default: add(3,"AY clock","%uHz",(unsigned)lfsConfig.ay_clock);
  }
  add(4,"Play mode","%s",modes[lfsConfig.play_mode%3]);
  add(5,"Brightness","%u%%",lfsConfig.scr_bright);
  if(lfsConfig.scr_timeout) add(6,"Screen timeout","%us",lfsConfig.scr_timeout);
  else add(6,"Screen timeout","Off");
  add(7,"DAC gain","%d%%",(int)roundf(lfsConfig.dacGain*100));
  add(8,"DAC panning","%s",pans[lfsConfig.modStereoSeparation%3]);
  add(9,"Skip tape files","%s",lfsConfig.skipTapeFormats?"Yes":"No");
  switch(lfsConfig.tapeSpeed){
    case TAPE_TURBO1: add(10,"Tape speed","7MHz (2x)"); break;
    case TAPE_TURBO2: add(10,"Tape speed","14MHz (4x)"); break;
    default: add(10,"Tape speed","3.5MHz (1x)");
  }
  add(11,"Encoder","%s",encs[lfsConfig.encReverse&1]);
  add(12,"Battery calib","%s%.1fV",lfsConfig.batCalib>0.0?"+":"",lfsConfig.batCalib);
  add(13,"MOD player","%s",lfsConfig.modEngine==MOD_ENGINE_NATIVE?"Built-in":"General Sound");
  add(14,"Skin","%s",lfsConfig.skin==SKIN_WILD?"WildPlayer":"Z-Player");
  if(foundRtc) add(15,"Date & time","");
  add(16,"Reset to default","");
  add(17,"About","");
  return n;
}

bool zpConfigDraw(){
  if(!zpConfigMine()) return false;
  if(!zpInit()) return false;
  bool full=!zpOwns;
  zpClaim();
  static uint32_t lastVolt;
  static int first=0;                   // the first item shown
  bool volt_due=millis()-lastVolt>1000;
  if(!full&&!PlayerCTRL.scr_mode_update[SCR_CONFIG]&&!volt_due) return true;
  if(full||PlayerCTRL.scr_mode_update[SCR_CONFIG]){
    ZPCItem it[18];
    int n=zpcItems(it),pos=0;
    for(int i=0;i<n;i++) if(it[i].idx==lfsConfig.cfg_cur) pos=i;
    if(pos<first) first=pos;
    if(pos>=first+ZPC_LINES) first=pos-ZPC_LINES+1;
    if(first>n-ZPC_LINES) first=n>ZPC_LINES?n-ZPC_LINES:0;
    zpClear(ZX_BLACK);
    zpBitmap((ZP_W-ZP_LOGO_W*2)/2,6,zpLogo,ZP_LOGO_W,ZP_LOGO_H,ZX_BWHITE,2);
    zpTextf(15,4,ZX_BYELLOW,ZX_BLACK,-1,"for ZxPod \x7f version %s",ZP_VERSION);
    zpFill(0,ZPC_TOP-16,ZP_W,16,ZX_BLUE);
    zpTextS(4,ZPC_TOP-16,"Set-Up:",ZX_BWHITE,ZX_BLUE,2);
    char b[32];
    snprintf(b,sizeof(b),"%d/%d",pos+1,n);
    zpTextS(ZP_W-4-8*(int)strlen(b),ZPC_TOP-16,b,ZX_BCYAN,ZX_BLUE,2);
    for(int l=0;l<ZPC_LINES&&first+l<n;l++){
      const ZPCItem &c=it[first+l];
      bool here=first+l==pos,edit=here&&cfgSet;
      uint8_t paper=here?ZX_BLUE:ZX_BLACK;
      int y=ZPC_TOP+l*16;
      zpFill(0,y,ZP_W,16,paper);
      zpTextS(4,y,c.label,here?ZX_BWHITE:ZX_BGREEN,paper,2);
      if(c.value[0]){
        snprintf(b,sizeof(b),edit?"<%s>":"%s",c.value);
        zpTextS(ZP_W-4-8*(int)strlen(b),y,b,edit?ZX_BYELLOW:ZX_BCYAN,paper,2);
      }
    }
    zpFill(0,ZPC_TOP+ZPC_LINES*16+2,ZP_W,2,ZX_BLUE);
    PlayerCTRL.scr_mode_update[SCR_CONFIG]=false;
  }
  lastVolt=millis();
  char b[32];                           // the foot: the battery, the card
  snprintf(b,sizeof(b),"Battery %.2fV",volt);
  zpTextS(4,304,b,ZX_BWHITE,ZX_BLACK,2,14);
  const char *gs=GS.state==GS_OFF?"GS off":GS.state==GS_BOOTING?"GS start":"GS ready";
  zpTextS(ZP_W-4-8*(int)strlen(gs),304,gs,ZX_BGREEN,ZX_BLACK,2);
  zpFlush();
  return true;
}

// About: upstream's credits kept, this firmware's added
static bool zpAboutMine(){ return zpSkin()&&PlayerCTRL.screen_mode==SCR_ABOUT; }

bool zpAboutDraw(){
  if(!zpAboutMine()) return false;
  if(!zpInit()) return false;
  bool full=!zpOwns;
  zpClaim();
  if(!full&&!PlayerCTRL.scr_mode_update[SCR_ABOUT]){ zpFlush(); return true; }
  zpClear(ZX_BLACK);
  zpBitmap((ZP_W-ZP_LOGO_W*2)/2,6,zpLogo,ZP_LOGO_W,ZP_LOGO_H,ZX_BWHITE,2);
  zpTextf(15,4,ZX_BYELLOW,ZX_BLACK,-1,"for ZxPod \x7f version %s",ZP_VERSION);
  zpHeader(0,6,ZP_COLS,"About:");
  int r=8;
  zpText(2,r++,"The look:",ZX_BGREEN,ZX_BLACK);
  zpText(4,r++,"Z-Player for General Sound, Evgeny Muchkin",ZX_BWHITE,ZX_BLACK);
  zpText(4,r++,"(its screens, font and letters, seen running)",ZX_WHITE,ZX_BLACK);
  r++;
  zpText(2,r++,"General Sound:",ZX_BGREEN,ZX_BLACK);
  zpText(4,r++,"the card: X-Trade, 1997; its ROM: Stinger,",ZX_BWHITE,ZX_BLACK);
  zpText(4,r++,"1.05b fixes: psb & Evgeny Muchkin, 2007, 2015",ZX_BWHITE,ZX_BLACK);
  zpText(4,r++,"emulated here: a Z80 at 12MHz, 2MB, core 0",ZX_WHITE,ZX_BLACK);
  r++;
  zpText(2,r++,"The player:",ZX_BGREEN,ZX_BLACK);
  zpTextf(4,r++,ZX_BWHITE,ZX_BLACK,-1,"ZxPOD Player v.%s, %s",FULL_VERSION,BUILD_DATE);
  zpText(4,r++,"by Spawn, Andy Karpov",ZX_BWHITE,ZX_BLACK);
  r++;
  zpText(2,r++,"Powered with:",ZX_BGREEN,ZX_BLACK);
  zpText(4,r++,"libayfly, z80emu (Lin Ke-Fong), ESP8266Audio,",ZX_BCYAN,ZX_BLACK);
  zpText(4,r++,"SdFat, libxmize, TFT_eSPI, EncButton,",ZX_BCYAN,ZX_BLACK);
  zpText(4,r++,"GyverFIFO, ArduinoFFT, sjasmplus.",ZX_BCYAN,ZX_BLACK);
  zpFill(0,38*8+3,ZP_W,2,ZX_BLUE);
  zpText(2,39,"press the encoder to go back",ZX_WHITE,ZX_BLACK);
  PlayerCTRL.scr_mode_update[SCR_ABOUT]=false;
  zpFlush();
  return true;
}
