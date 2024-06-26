#include <TFT_eSPI.h>
#include "driver/adc.h"

#include "res/WildFont.h"
#include "res/sprites.h"

#define WILD_CYAN 0x05B6
#define WILD_CYAN_D 0x0555
#define WILD_CYAN_D2 0x0659
#define WILD_YELLOW 0x9CE0
#define WILD_GREEN 0x0500
#define WILD_RED 0xF800

#define VIEW_WDT	224
#define VIEW_HGT	320

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

char decode[MAX_PATH];

enum {
  ALIGN_NONE = 0,
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
};

const char* utf8rus(const char *source){
  unsigned int i,j,k;
  unsigned char n;
  char m[2]={'0','\0'};
  strcpy(decode,"");k=strlen(source);i=j=0;
  while(i<k){
    n=source[i];i++;
    if (n>=127){
      switch (n){
        case 208:{
          n=source[i];i++;
          if(n==129){n=192;break;} // recode charter Ё
          break;
        }
        case 209:{
          n=source[i];i++;
          if(n==145){n=193;break;} // recode charter ё
          break;
        }
      }
    }
    m[0]=n;strcat(decode,m);
    j++;if(j>=sizeof(decode)) break;
  }
  return decode;
}

size_t tft_strlen(const String &str,uint8_t textSize = 1){ // returns length of charters UTF-8 strings
  int c,i,ix,q;
  for(q=0,i=0,ix=str.length();i<ix;i++,q++){
    c=(unsigned char)str[i];
    if(c>=0 && c<=127)i+=0;
    else if((c&0xE0)==0xC0)i+=1;
    else if((c&0xF0)==0xE0)i+=2;
    else if((c&0xF8)==0xF0)i+=3;
    //else if(($c&0xFC)==0xF8)i+=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
    //else if(($c&0xFE)==0xFC)i+=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
    else return 0; //invalid utf8
  }
  return q*(textSize*5);
}

size_t utf8_strlen(const String &str){ // returns length of charters UTF-8 strings
  int c,i,ix,q;
  for(q=0,i=0,ix=str.length();i<ix;i++,q++){
    c=(unsigned char)str[i];
    if(c>=0 && c<=127)i+=0;
    else if((c&0xE0)==0xC0)i+=1;
    else if((c&0xF0)==0xE0)i+=2;
    else if((c&0xF8)==0xF0)i+=3;
    //else if(($c&0xFC)==0xF8)i+=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
    //else if(($c&0xFE)==0xFC)i+=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
    else return 0; //invalid utf8
  }
  return q;
}

void spr_print(TFT_eSprite &spr, int16_t x, int16_t y, const char* str, uint8_t size=1, uint16_t color=TFT_WHITE, uint16_t bgColor=TFT_BLACK, const GFXfont *font=&WildFont){
  spr.setFreeFont(font);
  spr.setTextSize(size);
  spr.setTextColor(color, bgColor);
  spr.setCursor(x,y);
  spr.print(utf8rus(str));
}

void spr_println(TFT_eSprite &spr, uint16_t x, uint16_t row, const char* str, uint8_t size=1, uint8_t hAlign=ALIGN_NONE, uint16_t color=TFT_WHITE, uint16_t bgColor=TFT_BLACK, const GFXfont* font=&WildFont){
  uint16_t cY=row*(size*8);
  uint16_t strWidth=tft_strlen(str,size);
  uint16_t sprWidth=spr.width();
  switch (hAlign){
  case ALIGN_NONE:
    spr_print(spr,x,cY,str,size,color,bgColor,font);
    break;
  case ALIGN_LEFT:
    spr_print(spr,0,cY,str,size,color,bgColor,font);
    break;
  case ALIGN_CENTER:
    spr_print(spr,(sprWidth/2)-(strWidth/2),cY,str,size,color,bgColor,font);
    break;
  case ALIGN_RIGHT:
    spr_print(spr,sprWidth-strWidth,cY,str,size,color,bgColor,font);
    break;
  default:
    spr_print(spr,x,cY,str,size,color,bgColor,font);
    break;
  }
}

void spr_printmenu_item(TFT_eSprite &spr, uint16_t row, uint8_t size, const char* str_left, uint16_t leftColor=TFT_WHITE, uint16_t ptrColor=TFT_BLACK, const char* str_right="", uint16_t rightColor=TFT_WHITE, uint16_t bgColor=TFT_BLACK, const GFXfont* font=&WildFont){
  uint16_t cY=row*(size*8);
  spr.fillRoundRect(0,cY-(size*8),224,size*8,3,ptrColor);
  spr_println(spr,0,row,str_left,size,ALIGN_LEFT,leftColor);
  spr_println(spr,0,row,str_right,size,ALIGN_RIGHT,rightColor);
}

void spr_draw_buttons(TFT_eSprite &spr, uint16_t row, uint8_t size=1, uint8_t cursor=0, const char* btn1msg="OK", const char* btn2msg="CANCEL", uint8_t buttons=1){
  uint16_t cY=row*(size*8);
  uint16_t msg1width=tft_strlen(btn1msg,size);
  uint16_t msg2width=tft_strlen(btn2msg,size);
  uint16_t btn1x=(spr.width()/2/2)-(msg1width/2);
  uint16_t btn2x=((spr.width()/2)+(spr.width()/2/2))-(msg1width/2);
  switch(buttons){
  case 1:
    spr.fillRoundRect((spr.width()/2)-(msg1width/2)-2,cY-(size*8)-2,msg1width+4,(size*8)+4,4,WILD_CYAN_D);
    spr.setTextSize(size);
    spr.setTextColor(0);
    spr.setCursor((spr.width()/2)-(msg1width/2),cY);
    spr.print(utf8rus(btn1msg));
    break;
  case 2:
    switch(cursor){
      case YES:
        spr.drawRoundRect(btn1x-4,cY-(size*8)-4,msg1width+8,(size*8)+8,4,WILD_CYAN_D);
        spr.fillRoundRect(btn1x-4,cY-(size*8)-4,msg1width+8,(size*8)+8,4,WILD_CYAN);
        spr.setTextSize(size);
        spr.setTextColor(0);
        spr.setCursor(btn1x,cY);
        spr.print(utf8rus(btn1msg));
        spr.drawRoundRect(btn2x-4,cY-(size*8)-4,msg2width+8,(size*8)+8,4,WILD_CYAN);
        spr.setTextColor(WILD_CYAN_D2);
        spr.setCursor(btn2x,cY);
        spr.print(utf8rus(btn2msg));
        break;
      case NO:
        spr.drawRoundRect(btn1x-4,cY-(size*8)-4,msg1width+8,(size*8)+8,4,WILD_CYAN);
        spr.setTextSize(size);
        spr.setTextColor(WILD_CYAN_D2);
        spr.setCursor(btn1x,cY);
        spr.print(utf8rus(btn1msg));
        spr.drawRoundRect(btn2x-4,cY-(size*8)-4,msg2width+8,(size*8)+8,4,WILD_CYAN_D);
        spr.fillRoundRect(btn2x-4,cY-(size*8)-4,msg2width+8,(size*8)+8,4,WILD_CYAN);
        spr.setTextColor(0);
        spr.setCursor(btn2x,cY);
        spr.print(utf8rus(btn2msg));
        break;
    }
    break;
  default:
    spr.fillRoundRect((spr.width()/2)-(msg1width/2)-2,cY-(size*8)-2,msg1width+4,(size*8)+4,4,WILD_CYAN_D);
    spr.setTextSize(size);
    spr.setTextColor(0);
    spr.setCursor((spr.width()/2)-(msg1width/2),cY);
    spr.print(utf8rus(btn1msg));
    break;
  }
}

void initVoltage(){
  pinMode(VOLTPIN, INPUT);
  analogReadResolution(12);
  randomSeed(analogRead(VOLTPIN));
  pinMode(CHGSENS, INPUT);
}

void voltage() {
  if (batChange || millis() - mlsV > vUp) {
    static int chgP = 0;
    uint64_t InVolt = 0;
    //Reading from a port with averaging
    for (int i = 0; i < READ_CNT; i++) {
      InVolt += analogReadMilliVolts(VOLTPIN);
    }
    InVolt = InVolt/READ_CNT;

    volt = ((InVolt/1000.0)*VoltMult)+0.1;

    if (volt <= v_min){
      precent = 0;
    }else if (volt >= v_max){
      precent = 100;
    }else{
      precent = ((volt - v_min) * 100) / (v_max - v_min);
    }

    img.setColorDepth(8);
    img.createSprite(40,9);
    img.fillScreen(0);
    img.drawBitmap(20,0,batSptites16x9,18,9,WILD_CYAN);
    img.setTextSize(1);
    img.setCursor(0,9);
    if (precent < 100) img.setCursor(5,9);
    if (precent < 10) img.setCursor(2*5,9);
    img.setTextColor(WILD_CYAN);
    img.print(precent);
    img.print("%");

    int batColor = TFT_GREEN;
  
    if (!digitalRead(CHGSENS)) { // if not charger
      if (precent < 41) batColor = TFT_YELLOW;
      if (precent < 21) batColor = WILD_RED;
      img.fillRect(22, 2, map(precent, 0, 100, 1, 13), 5, batColor);
      vUp = V_UPD;
    } else { // charge
      chgP++;
      int pos = map(precent, 0, 100, 1, 13);
      if ((pos+chgP) > 13) chgP = 0;
      if (pos+chgP < 14) {
        if (pos+chgP < 7) batColor = TFT_YELLOW;
        if (pos+chgP < 4) batColor = WILD_RED;
        img.fillRect(22, 2, pos+chgP, 5, batColor);
      }
      img.drawBitmap(26,0,chgFlash5x9,5,9,TFT_MAGENTA);
      vUp = 250;
    }
    mlsV = millis();
    batChange = false;
    img.pushSprite(194,8);
    img.deleteSprite();
  }
}

void display_brightness(uint8_t Value){
  if (Value > 0 || Value < 100) {
    analogWrite(TFT_BL, map(Value, 100, 0, 0, 100) * 2.55);
  }
}

void scrTimeout(){
  if(enc.action() || dn.action() || up.action()){
    keysEvent=true;
  }
  if(keysEvent) {
    display_brightness(Config.scr_bright);
    mlsScr=millis();
    keysEvent=false;
  }
  if (Config.scr_timeout > 0 && millis() - mlsScr > Config.scr_timeout*1000) {
    analogWrite(TFT_BL,255);
  }
}

void show_frame(){
  tft.fillScreen(0);
  tft.setTextWrap(false);
  //show frame
  tft.pushImage(0,0,4,320,frameLeft4x320);
  tft.pushImage(4,0,240,4,frameTop240x4);
  tft.pushImage(236,0,4,320,frameRight4x320);
  tft.pushImage(4,316,240,4,frameBottom240x4);
}

void introTFT(){
  int scrollPos=-112;
  img.createSprite(240,32);
  while(scrollPos<128){
    img.fillScreen(0);
    img.pushImage(scrollPos-1,0,112,32,introWild112x32);
    img.pushSprite(0,78);
    scrollPos++;
  }
  img.deleteSprite();
  scrollPos=210;
  img.createSprite(99,210);
  while(scrollPos>0){
    img.fillScreen(0);
    img.pushImage(0,scrollPos-1,99,88,introGromo99x88);
    img.pushSprite(76,110);
    scrollPos--;
  }
  img.deleteSprite();
  scrollPos=240;
  img.createSprite(240,32);
  while(scrollPos>0){
    img.fillScreen(0);
    img.pushImage(scrollPos-1,0,136,32,introPlayer136x32);
    img.pushSprite(0,210);
    scrollPos--;
  }
  img.deleteSprite();
  scrollPos=-66;
  img.createSprite(66,110+50);
  while(scrollPos<110){
    img.fillScreen(0);
    img.pushImage(0,scrollPos-1,66,50,introNotes66x50);
    img.pushSprite(10,0);
    scrollPos++;
  }
  img.deleteSprite();
}

void clear_display_field(){
  tft.fillRect(8,8,224,304,0);
}

void sdEject(){
  img.setColorDepth(8);
  img.createSprite(224,304);
  img.fillScreen(0);
  img.setTextWrap(false);
  img.setTextColor(TFT_RED);
  img.setTextSize(2);
  img.setFreeFont(&WildFont);
  spr_println(img,0,9,PSTR("Insert SD Card!"),2,ALIGN_CENTER,TFT_RED);
  img.pushSprite(8,8);
  img.deleteSprite();
}

void TFTInit(){
  tft.init(TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&WildFont);
  display_brightness(Config.scr_bright);
}