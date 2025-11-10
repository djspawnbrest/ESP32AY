#include "ayFreqTable.h"

#define EQ_HEIGHT 24

uint8_t ay_reg_1[16];
uint8_t ay_reg_2[16];

static byte bufEQ[96];
static bool bufEQcl[96];
static bool abcCl[3];
static bool colCh=false;

uint8_t A,B,C;
uint8_t tA=0,tB=0,tC=0;
uint8_t tA1=0,tB1=0,tC1=0;
uint8_t tA2=0,tB2=0,tC2=0;

void clearEQ(){
  memset(bufEQ,0,sizeof(bufEQ));
  tA=tB=tC=0;
}

// Helper: draw channel bar with auto-decrement
static inline void drawBar(uint8_t &val,uint8_t x,uint8_t y,uint8_t h,uint8_t maxW,uint8_t scale){
  if(val>0){
    int w=(val*maxW)/scale;
    if(w>maxW)w=maxW;
    img.fillRectHGradient(x,y,w,h,TFT_GREEN,TFT_RED);
    val--;
  }
}

// Helper: setup text style once
static inline void setupText(){
  img.setFreeFont(&WildFont);
  img.setTextSize(2);
  img.setTextColor(WILD_CYAN);
}

void fastEQ(){
  img.setColorDepth(8);
  img.createSprite(224,68);
  img.fillScreen(0);
  uint8_t shift=0;
  for(uint8_t i=0;i<56;i++){
    img.fillRect(shift,26,2,2,TFT_BLUE);
    img.fillRect(shift,66,2,2,TFT_BLUE);
    shift+=4;
  }
  shift=16;
  uint8_t shift2=16;
  for(uint8_t i=0;i<96;i++){
    if(i%2==0){
      if(bufEQ[i]!=0){
        int h=(EQ_HEIGHT/2)*bufEQ[i]/8;
        if(h>EQ_HEIGHT) h=EQ_HEIGHT;
        img.fillRectVGradient(shift,24-h,3,h,TFT_RED,TFT_GREEN);
        bufEQ[i]--;
      }
      shift+=4;
    }else{
      if(bufEQ[i]!=0){
        int h=(EQ_HEIGHT/2)*bufEQ[i]/8;
        if(h>EQ_HEIGHT) h=EQ_HEIGHT;
        img.fillRectVGradient(shift2,24+16+24-h,3,h,TFT_RED,TFT_GREEN);
        bufEQ[i]--;
      }
      shift2+=4;
    }
  }
  img.pushSprite(9,85);
  img.deleteSprite();
  //ABC for AY or ABCDEFGH channels for mod
  img.setColorDepth(16);
  img.createSprite(222,20);
  img.fillScreen(0);
  shift=0;
  for(uint8_t i=0;i<56;i++){
    img.fillRect(shift,0,2,2,TFT_BLUE);
    shift+=4;
  }
  if((PlayerCTRL.music_type==TYPE_MOD
    ||PlayerCTRL.music_type==TYPE_S3M
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    ||PlayerCTRL.music_type==TYPE_XM
  #endif
  )&&!lfsConfig.playerSource==PLAYER_MODE_UART){
    setupText();
    img.setCursor(0,20);img.print("A");
    img.setCursor(56,20);img.print("B");
    img.setCursor(112,20);img.print("C");
    img.setCursor(168,20);img.print("D");
    switch(modChannelsEQ){
      case 6:
        drawBar(modEQchn[0],11,6,6,44,64);
        drawBar(modEQchn[1],11,12,6,44,64);
        drawBar(modEQchn[2],67,6,12,44,64);
        drawBar(modEQchn[3],123,6,12,44,64);
        drawBar(modEQchn[3],179,6,6,44,64);
        drawBar(modEQchn[3],179,12,6,44,64);
        break;
      case 8:
        drawBar(modEQchn[0],11,6,6,44,64);
        drawBar(modEQchn[1],11,12,6,44,64);
        drawBar(modEQchn[2],67,6,6,44,64);
        drawBar(modEQchn[3],67,12,6,44,64);
        drawBar(modEQchn[4],123,6,6,44,64);
        drawBar(modEQchn[5],123,12,6,44,64);
        drawBar(modEQchn[6],179,6,6,44,64);
        drawBar(modEQchn[7],179,12,6,44,64);
        break;
      default:
        drawBar(modEQchn[0],11,6,12,44,64);
        drawBar(modEQchn[1],67,6,12,44,64);
        drawBar(modEQchn[2],123,6,12,44,64);
        drawBar(modEQchn[3],179,6,12,44,64);
        break;
    }
  }else if(PlayerCTRL.music_type==TYPE_TAP||PlayerCTRL.music_type==TYPE_TZX||PlayerCTRL.music_type==TYPE_MP3||PlayerCTRL.music_type==TYPE_WAV){
    setupText();
    img.setCursor(0,20);img.print("L");
    img.setCursor(113,20);img.print("R");
    drawBar(modEQchn[0],11,6,12,98,64);
    drawBar(modEQchn[1],124,6,12,98,64);
  }else{
    setupText();
    img.setCursor(0,20);img.print("A");
    img.setCursor(73,20);img.print("B");
    img.setCursor(149,20);img.print("C");
    if(AYInfo.is_ts&&lfsConfig.playerSource==PLAYER_MODE_SD){
      drawBar(tA1,11,6,6,58,8);
      drawBar(tA2,11,12,6,58,8);
      drawBar(tB1,84,6,6,58,8);
      drawBar(tB2,84,12,6,58,8);
      drawBar(tC1,160,6,6,58,8);
      drawBar(tC2,160,12,6,58,8);
    }else{
      drawBar(tA,11,6,12,58,8);
      drawBar(tB,84,6,12,58,8);
      drawBar(tC,160,6,12,58,8);
    }
  }
  img.pushSprite(9,293);
  img.deleteSprite();
}

// Helper: find frequency index
static inline uint8_t findFreqIdx(uint16_t freq){
  for(uint8_t i=0;i<95;i++)
    if(FreqAY[i]>=freq&&FreqAY[i+1]<freq)return i;
  return 0;
}

void readEQ(uint8_t reg,uint8_t val,uint8_t chip=0){
  static uint16_t tmpA,tmpB,tmpC,regEnv;
  static uint8_t regNoise,reg13,regMix;
  
  tA1=(ay_reg_1[8]&0x1F); tB1=(ay_reg_1[9]&0x1F); tC1=(ay_reg_1[10]&0x1F);
  tA2=(ay_reg_2[8]&0x1F); tB2=(ay_reg_2[9]&0x1F); tC2=(ay_reg_2[10]&0x1F);
  val=(ay_reg_1[reg]+ay_reg_2[reg])>>1;
  
  switch(reg){
    case 0:tmpA=(tmpA&0xFF00)|val;break;
    case 1:tmpA=(tmpA&0xFF)|((val&0x0F)<<8);break;
    case 2:tmpB=(tmpB&0xFF00)|val;break;
    case 3:tmpB=(tmpB&0xFF)|((val&0x0F)<<8);break;
    case 4:tmpC=(tmpC&0xFF00)|val;break;
    case 5:tmpC=(tmpC&0xFF)|((val&0x0F)<<8);break;
    case 6:regNoise=val&0x1F;break;
    case 7:regMix=val;break;
    case 8:tA=val&0x1F;break;
    case 9:tB=val&0x1F;break;
    case 10:tC=val&0x1F;break;
    case 11:regEnv=(regEnv&0xFF00)|val;break;
    case 12:regEnv=(regEnv&0xFF)|(val<<8);break;
    case 13:reg13=val;break;
  }
  
  A=findFreqIdx(tmpA);
  B=findFreqIdx(tmpB);
  C=findFreqIdx(tmpC);
  uint8_t indxEnv=findFreqIdx(regEnv*reg13);
  uint8_t halfNoise=regNoise>>1;
  
  bufEQ[(tA&16)?indxEnv:A]|=(tA&16)?(tA&0x0F):tA;
  if(!(regMix&8))bufEQ[A]|=halfNoise;
  bufEQ[(tB&16)?indxEnv:B]|=(tB&16)?(tB&0x0F):tB;
  if(!(regMix&16))bufEQ[B]|=halfNoise;
  bufEQ[(tC&16)?indxEnv:C]|=(tC&16)?(tC&0x0F):tC;
  if(!(regMix&32))bufEQ[C]|=halfNoise;
}
