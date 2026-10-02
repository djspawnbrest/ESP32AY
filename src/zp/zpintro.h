// zpintro.h - the boot screen, in place of upstream's introTFT(): the
// border of a Spectrum loading from tape (the pilot tone's red and cyan,
// then the data's blue and yellow), then Z-Player's letters and the
// credits.  About 2 s; setup() goes on to upstream's frame after it.

#define ZP_VERSION "0.1.0"

static void zpBorder(int phase,int frame){
  // stripes of the border, 16 px each side; the paper in the middle is left alone
  uint32_t seed=frame*2654435761u;
  int y=0;
  while(y<ZP_H){
    seed=seed*1103515245u+12345u;
    int h=phase?1+(seed>>28)%4:6+(seed>>29);        // data: thin and busy; pilot: even
    uint8_t c=phase?((seed>>27)&1?ZX_BLUE:ZX_YELLOW):((y/h+frame)&1?ZX_RED:ZX_CYAN);
    for(int j=y;j<y+h&&j<ZP_H;j++){
      zpFill(0,j,16,1,c); zpFill(ZP_W-16,j,16,1,c);
      if(j<16||j>=ZP_H-16) zpFill(16,j,ZP_W-32,1,c);
    }
    y+=h;
  }
}

static void zpIntro(){
  if(!zpInit()) return;
  zpClaim();
  zpClear(ZX_BLACK);
  display_brightness(lfsConfig.scr_bright);
  for(int f=0;f<24;f++){ zpBorder(f>=12,f); zpFlush(); delay(20); }
  zpBorder(1,24);
  int y=90;
  zpBitmap((ZP_W-ZP_LOGO_W*2)/2,y,zpLogo,ZP_LOGO_W,ZP_LOGO_H,ZX_BWHITE,2);
  zpText(15,(y+32)/8,"for ZxPod \x7f General Sound",ZX_BYELLOW,ZX_BLACK);
  zpTextf(22,(y+48)/8,ZX_BCYAN,ZX_BLACK,-1,"version %s",ZP_VERSION);
  zpText(9,24,"the look: Z-Player by Evgeny Muchkin",ZX_WHITE,ZX_BLACK);
  zpText(7,25,"the player: ZxPod by Alexander Spawn",ZX_WHITE,ZX_BLACK);
  zpText(12,26,"General Sound: X-Trade, 1997",ZX_WHITE,ZX_BLACK);
  zpFlush();
  for(int f=25;f<60;f++){               // the data keeps loading round the edge
    zpBorder(1,f); zpFlush(); delay(20);
  }
  zpFill(0,0,ZP_W,16,ZX_BLACK); zpFill(0,ZP_H-16,ZP_W,16,ZX_BLACK);
  zpFill(0,0,16,ZP_H,ZX_BLACK); zpFill(ZP_W-16,0,16,ZP_H,ZX_BLACK);
  zpFlush();
  zpOwns=false;                         // setup() draws upstream's frame next
}
