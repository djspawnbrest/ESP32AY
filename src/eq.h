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
  for(uint8_t i=0;i<sizeof(bufEQ);i++)bufEQ[i]=0;
  tA=0;tB=0;tC=0;
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
  //If Turbo Sound - draw flash labels
  // if(AYInfo.is_ts&&Config.playerSource==PLAYER_MODE_SD){
  //   img.setFreeFont(&WildFont);
  //   img.setTextSize(1);
  //   img.setTextColor((colCh)?TFT_RED:TFT_WHITE);
  //   uint8_t x,y=0;
  //   bool sx,sy=false;
  //   for(uint8_t i=0;i<4;i++){
  //     img.setCursor(5+x,7+y);
  //     img.print("T");
  //     img.setCursor(5+x,15+y);
  //     img.print("S");
  //     img.setCursor(5+x,23+y);
  //     img.print("!");
  //     sx=(i==0||i==2)?true:false;
  //     sy=(i==0||i==1)?true:false;
  //     x=(sx)?208:0;
  //     y=(sy)?38:0;
  //   }
  //   colCh=!colCh;
  // }
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
    img.setFreeFont(&WildFont);
    img.setTextSize(2);
    img.setTextColor(WILD_CYAN);
    img.setCursor(0,20);
    img.print("A");
    img.setCursor(56,20);
    img.print("B");
    img.setCursor(112,20);
    img.print("C");
    img.setCursor(168,20);
    img.print("D");
    switch(modChannelsEQ){
      case 6:
        //A1
        if(modEQchn[0]>0){
          int w=(modEQchn[0]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(11,6,w,6,TFT_GREEN,TFT_RED);
          modEQchn[0]--;
        }
        //A2
        if(modEQchn[1]>0){
          int w=(modEQchn[1]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(11,12,w,6,TFT_GREEN,TFT_RED);
          modEQchn[1]--;
        }
        //B
        if(modEQchn[2]>0){
          int w=(modEQchn[2]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(67,6,w,12,TFT_GREEN,TFT_RED);
          modEQchn[2]--;
        }
        //C
        if(modEQchn[3]>0){
          int w=(modEQchn[3]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(123,6,w,12,TFT_GREEN,TFT_RED);
          modEQchn[3]--;
        }
        //D1
        if(modEQchn[3]>0){
          int w=(modEQchn[3]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(179,6,w,6,TFT_GREEN,TFT_RED);
          modEQchn[3]--;
        }
        //D2
        if(modEQchn[3]>0){
          int w=(modEQchn[3]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(179,12,w,6,TFT_GREEN,TFT_RED);
          modEQchn[3]--;
        }
        break;
      case 8:
        //A1
        if(modEQchn[0]>0){
          int w=(modEQchn[0]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(11,6,w,6,TFT_GREEN,TFT_RED);
          modEQchn[0]--;
        }
        //A2
        if(modEQchn[1]>0){
          int w=(modEQchn[1]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(11,12,w,6,TFT_GREEN,TFT_RED);
          modEQchn[1]--;
        }
        //B1
        if(modEQchn[2]>0){
          int w=(modEQchn[2]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(67,6,w,6,TFT_GREEN,TFT_RED);
          modEQchn[2]--;
        }
        //B2
        if(modEQchn[3]>0){
          int w=(modEQchn[3]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(67,12,w,6,TFT_GREEN,TFT_RED);
          modEQchn[3]--;
        }
        //C1
        if(modEQchn[4]>0){
          int w=(modEQchn[4]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(123,6,w,6,TFT_GREEN,TFT_RED);
          modEQchn[4]--;
        }
        //C2
        if(modEQchn[5]>0){
          int w=(modEQchn[5]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(123,12,w,6,TFT_GREEN,TFT_RED);
          modEQchn[5]--;
        }
        //D1
        if(modEQchn[6]>0){
          int w=(modEQchn[6]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(179,6,w,6,TFT_GREEN,TFT_RED);
          modEQchn[6]--;
        }
        //D2
        if(modEQchn[7]>0){
          int w=(modEQchn[7]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(179,12,w,6,TFT_GREEN,TFT_RED);
          modEQchn[7]--;
        }
        break;
      default:
        //A
        if(modEQchn[0]>0){
          int w=(modEQchn[0]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(11,6,w,12,TFT_GREEN,TFT_RED);
          modEQchn[0]--;
        }
        //B
        if(modEQchn[1]>0){
          int w=(modEQchn[1]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(67,6,w,12,TFT_GREEN,TFT_RED);
          modEQchn[1]--;
        }
        //C
        if(modEQchn[2]>0){
          int w=(modEQchn[2]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(123,6,w,12,TFT_GREEN,TFT_RED);
          modEQchn[2]--;
        }
        //D
        if(modEQchn[3]>0){
          int w=(modEQchn[3]*44)/64;
          if(w>44) w=44;
          img.fillRectHGradient(179,6,w,12,TFT_GREEN,TFT_RED);
          modEQchn[3]--;
        }
        break;
    }
  }else if(PlayerCTRL.music_type==TYPE_TAP||PlayerCTRL.music_type==TYPE_TZX){
    // For tape formats show L and R channels
    img.setFreeFont(&WildFont);
    img.setTextSize(2);
    img.setTextColor(WILD_CYAN);
    img.setCursor(0,20);
    img.print("L");
    img.setCursor(113,20);
    img.print("R");
    // L channel
    if(modEQchn[0]>0){
      int w=(modEQchn[0]*98)/64;
      if(w>98) w=98;
      img.fillRectHGradient(11,6,w,12,TFT_GREEN,TFT_RED);
      modEQchn[0]--;
    }
    // R channel
    if(modEQchn[1]>0){
      int w=(modEQchn[1]*98)/64;
      if(w>98) w=98;
      img.fillRectHGradient(124,6,w,12,TFT_GREEN,TFT_RED);
      modEQchn[1]--;
    }
  }else{
    img.setFreeFont(&WildFont);
    img.setTextSize(2);
    img.setTextColor(WILD_CYAN);
    img.setCursor(0,20);
    img.print("A");
    img.setCursor(73,20);
    img.print("B");
    img.setCursor(149,20);
    img.print("C");
    if(AYInfo.is_ts&&lfsConfig.playerSource==PLAYER_MODE_SD){
      if(tA1>0){
        int w=(58/2)*tA1/8;
        if(w>58) w=58;
        img.fillRectHGradient(11,6,w,6,TFT_GREEN,TFT_RED);
        tA1--;
      }
      if(tA2>0){
        int w=(58/2)*tA2/8;
        if(w>58) w=58;
        img.fillRectHGradient(11,12,w,6,TFT_GREEN,TFT_RED);
        tA2--;
      }
      if(tB1>0){
        int w=(58/2)*tB1/8;
        if(w>58) w=58;
        img.fillRectHGradient(84,6,w,6,TFT_GREEN,TFT_RED);
        tB1--;
      }
      if(tB2>0){
        int w=(58/2)*tB2/8;
        if(w>58) w=58;
        img.fillRectHGradient(84,12,w,6,TFT_GREEN,TFT_RED);
        tB2--;
      }
      if(tC1>0){
        int w=(58/2)*tC1/8;
        if(w>58) w=58;
        img.fillRectHGradient(160,6,w,6,TFT_GREEN,TFT_RED);
        tC1--;
      }
      if(tC2>0){
        int w=(58/2)*tC2/8;
        if(w>58) w=58;
        img.fillRectHGradient(160,12,w,6,TFT_GREEN,TFT_RED);
        tC2--;
      }
    }else{
      if(tA>0){
        int w=(58/2)*tA/8;
        if(w>58) w=58;
        img.fillRectHGradient(11,6,w,12,TFT_GREEN,TFT_RED);
        tA--;
      }
      if(tB>0){
        int w=(58/2)*tB/8;
        if(w>58) w=58;
        img.fillRectHGradient(84,6,w,12,TFT_GREEN,TFT_RED);
        tB--;
      }
      if(tC>0){
        int w=(58/2)*tC/8;
        if(w>58) w=58;
        img.fillRectHGradient(160,6,w,12,TFT_GREEN,TFT_RED);
        tC--;
      }
    }
  }
  img.pushSprite(9,293);
  img.deleteSprite();
}

void readEQ(uint8_t reg,uint8_t val,uint8_t chip=0){
	uint16_t tmpA=0,tmpB=0,tmpC=0;
	uint16_t regEnv=0,indxEnv=0;
	uint8_t regNoise=0,reg13=0;
	uint8_t regMix=0xFF;
  tA1=(ay_reg_1[8]&0x1F); tB1=(ay_reg_1[9]&0x1F); tC1=(ay_reg_1[10]&0x1F);
  tA2=(ay_reg_2[8]&0x1F); tB2=(ay_reg_2[9]&0x1F); tC2=(ay_reg_2[10]&0x1F);
  val=(ay_reg_1[reg]+ay_reg_2[reg])/2;
  switch(reg){
    case 0:
      tmpA|=val;
    break;
    case 1:
      tmpA=(val&0x0F)<<8;
    break;
    case 2:
      tmpB|=val;
    break;
    case 3:
      tmpB=(val&0x0F)<<8;
    break;
    case 4:
      tmpC|=val;
    break;
    case 5:
      tmpC=(val&0x0F)<<8;
    break;
    case 6:
      regNoise=val&0x1F;
    break;
    case 7:
      regMix=val;
    break;
    case 8:
      tA=(val&0x1F);
    break;
    case 9:
      tB=(val&0x1F);
    break;
    case 10:
      tC=(val&0x1F);
    break;
    case 11:
      regEnv|=val;
    break;
    case 12:
      regEnv=val<<8;
    break;
    case 13:
      reg13=val;
    break;
    default:
    break;
  }
	for(uint8_t i=0;i<96;i++){
		if(FreqAY[i]>=tmpA&&FreqAY[i+1]<tmpA)A=i;
		if(FreqAY[i]>=tmpB&&FreqAY[i+1]<tmpB)B=i;
		if(FreqAY[i]>=tmpC&&FreqAY[i+1]<tmpC)C=i;
		if(FreqAY[i]>=regEnv*reg13&&FreqAY[i+1]<regEnv*reg13)indxEnv=i;
	}
	if((tA&16)==0)bufEQ[A]|=tA;
  else bufEQ[indxEnv]|=tA&0x0F;
	if((regMix&8)==0)bufEQ[A]|=regNoise/2;
	if((tB&16)==0)bufEQ[B]|=tB;
	else bufEQ[indxEnv]|=tB&0x0F;
	if((regMix&16)==0)bufEQ[B]|=regNoise/2;
	if((tC&16)==0)bufEQ[C]|=tC;	
	else bufEQ[indxEnv]|=tC&0x0F;
	if((regMix&32)==0)bufEQ[C]|=regNoise/2;
}
