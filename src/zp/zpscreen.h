// zpscreen.h - the Z-Player look's screen: 240 x 320 in the Spectrum's 16
// colours, 4 bits a pixel in PSRAM, pushed to the ST7789 a strip of 8 lines
// at a time, only the strips drawn into since the last push.
//
// The text is Z-Player's own 4 x 8 font (zpfont.h): 60 columns, 40 rows.
// Everything draws into the framebuffer; zpFlush() sends what changed.
// Upstream's frame and screens draw straight onto the TFT, so the one
// who draws last owns the glass: zpClaim() before a Z-Player screen,
// zpRelease() hands it back (show_frame() and a full redraw).

#include "zpfont.h"
#include "zplogo.h"

// the Spectrum's colours: 0-7 normal, 8-15 bright
enum{
  ZX_BLACK=0,ZX_BLUE,ZX_RED,ZX_MAGENTA,ZX_GREEN,ZX_CYAN,ZX_YELLOW,ZX_WHITE,
  ZX_BBLACK,ZX_BBLUE,ZX_BRED,ZX_BMAGENTA,ZX_BGREEN,ZX_BCYAN,ZX_BYELLOW,ZX_BWHITE,
};
static const uint16_t zpPal[16]={
  0x0000,0x001A,0xD000,0xD01A,0x06A0,0x06BA,0xD6A0,0xD6BA,
  0x0000,0x001F,0xF800,0xF81F,0x07E0,0x07FF,0xFFE0,0xFFFF,
};

#define ZP_W 240
#define ZP_H 320
#define ZP_COLS (ZP_W/4)
#define ZP_ROWS (ZP_H/8)

// the skin (Set-Up): Z-Player's screens, or upstream's WildPlayer ones -
// every zp*Mine() asks this first, so upstream draws when it says no
static bool zpSkin(){ return lfsConfig.skin!=SKIN_WILD; }

static uint8_t *zpFb=NULL;              // ZP_W*ZP_H/2, PSRAM, high nibble = left pixel
static uint64_t zpDirty=0;              // a bit a strip of 8 lines
static bool zpOwns=false;               // the glass is ours

static bool zpInit(){
  if(zpFb) return true;
  zpFb=(uint8_t*)heap_caps_calloc(ZP_W*ZP_H/2,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  return zpFb!=NULL;
}

static inline void zpPixel(int x,int y,uint8_t c){
  if((unsigned)x>=ZP_W||(unsigned)y>=ZP_H) return;
  uint8_t *p=zpFb+(y*ZP_W+x)/2;
  *p=(x&1)?((*p&0xF0)|c):((*p&0x0F)|(c<<4));
  zpDirty|=1ULL<<(y>>3);
}

static void zpFill(int x,int y,int w,int h,uint8_t c){
  for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++) zpPixel(i,j,c);
}

static void zpClear(uint8_t c){
  memset(zpFb,c<<4|c,ZP_W*ZP_H/2);
  zpDirty=(1ULL<<ZP_ROWS)-1;
}

// one character cell: col 0-59, row 0-39
static void zpChar(int col,int row,uint8_t ch,uint8_t ink,uint8_t paper){
  const uint8_t *g=zpFont[ch&0x7F];
  int x0=col*4,y0=row*8;
  for(int y=0;y<8;y++){
    uint8_t bits=g[y];
    for(int x=0;x<4;x++) zpPixel(x0+x,y0+y,(bits&(8>>x))?ink:paper);
  }
}

// text at a cell, clipped at `width` cells (pads with paper), returns the columns used
static int zpText(int col,int row,const char *s,uint8_t ink,uint8_t paper,int width=-1){
  int n=0;
  while(*s&&(width<0||n<width)&&col+n<ZP_COLS){
    uint8_t c=(uint8_t)*s++;
    if(c==0xC2&&(uint8_t)*s==0xB7){ c=0x7F; s++; }          // UTF-8 middle dot
    else if(c>=0x80) c='?';
    zpChar(col+n++,row,c,ink,paper);
  }
  while(width>=0&&n<width&&col+n<ZP_COLS) zpChar(col+n++,row,' ',ink,paper);
  return n;
}

static void zpTextf(int col,int row,uint8_t ink,uint8_t paper,int width,const char *fmt,...){
  char buf[80];
  va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
  zpText(col,row,buf,ink,paper,width);
}

// text at a pixel position, `scale` times the 4x8 cells (2: 8x16, 3: 12x24),
// clipped at `width` cells and padded with paper; returns the cells used
static int zpTextS(int x,int y,const char *s,uint8_t ink,uint8_t paper,int scale,int width=-1){
  int n=0;
  while(*s&&(width<0||n<width)&&x+(n+1)*4*scale<=ZP_W){
    uint8_t c=(uint8_t)*s++;
    if(c==0xC2&&(uint8_t)*s==0xB7){ c=0x7F; s++; }
    else if(c>=0x80) c='?';
    const uint8_t *g=zpFont[c&0x7F];
    int x0=x+n*4*scale;
    for(int r=0;r<8;r++) for(int b=0;b<4;b++)
      zpFill(x0+b*scale,y+r*scale,scale,scale,(g[r]&(8>>b))?ink:paper);
    n++;
  }
  while(width>=0&&n<width&&x+(n+1)*4*scale<=ZP_W){ zpFill(x+n*4*scale,y,4*scale,8*scale,paper); n++; }
  return n;
}

static void zpTextSf(int x,int y,uint8_t ink,uint8_t paper,int scale,int width,const char *fmt,...){
  char buf[80];
  va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
  zpTextS(x,y,buf,ink,paper,scale,width);
}

// a 1-bit bitmap, MSB left, scaled
static void zpBitmap(int x,int y,const uint8_t *bits,int w,int h,uint8_t ink,int scale=1){
  int bw=(w+7)/8;
  for(int j=0;j<h;j++) for(int i=0;i<w;i++)
    if(bits[j*bw+i/8]&(0x80>>(i&7))) zpFill(x+i*scale,y+j*scale,scale,scale,ink);
}

static void zpFrame(int x,int y,int w,int h,uint8_t c){
  zpFill(x,y,w,1,c); zpFill(x,y+h-1,w,1,c);
  zpFill(x,y,1,h,c); zpFill(x+w-1,y,1,h,c);
}

// the strips drawn into, to the TFT.  Upstream's key handling still draws
// its popups straight onto the glass (volume, track icons); once a second
// everything is sent again, so nothing of it stays.
static void zpFlush(){
  static uint16_t line[ZP_W*8];         // internal RAM: one strip
  static uint32_t lastAll;
  if(millis()-lastAll>1000){ lastAll=millis(); zpDirty=(1ULL<<ZP_ROWS)-1; }
  if(!zpDirty||!zpOwns) return;
  tft.setSwapBytes(true);
  for(int s=0;s<ZP_ROWS;s++){
    if(!(zpDirty&(1ULL<<s))) continue;
    const uint8_t *src=zpFb+s*8*ZP_W/2;
    for(int i=0;i<ZP_W*8/2;i++){
      line[2*i]=zpPal[src[i]>>4];
      line[2*i+1]=zpPal[src[i]&15];
    }
    tft.pushImage(0,s*8,ZP_W,8,line);
  }
  tft.setSwapBytes(false);
  zpDirty=0;
}

// the glass: ours for a Z-Player screen, upstream's otherwise
static void zpClaim(){
  if(zpOwns) return;
  zpOwns=true;
  zpDirty=(1ULL<<ZP_ROWS)-1;            // everything, over whatever was there
}

static void zpRelease(){
  if(!zpOwns) return;
  zpOwns=false;
  show_frame();                         // upstream's frame, then its screen in full
  for(int i=0;i<(int)(sizeof(PlayerCTRL.scr_mode_update)/sizeof(PlayerCTRL.scr_mode_update[0]));i++)
    PlayerCTRL.scr_mode_update[i]=true;
}
