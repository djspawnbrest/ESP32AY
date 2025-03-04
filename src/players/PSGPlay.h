#include <GyverFIFO.h>

const int bufSize=256; // select a multiple of 16
GyverFIFO<byte,bufSize>playBuf;
byte yrgFrame=0;

void fillBuffer(){
  // if (xSemaphoreTake(sdCardSemaphore, portMAX_DELAY) == pdTRUE) {
    if(sd_fat.card()->sectorCount()){
      while(playBuf.availableForWrite()&&sd_play_file.available()){
        playBuf.write(sd_play_file.read());
      }
      if(PlayerCTRL.music_type==TYPE_PSG||PlayerCTRL.music_type==TYPE_RSF){
        if(playBuf.availableForWrite()&&!sd_play_file.available()) playBuf.write(0xFD);
      }
    }else{
      muteAYBeep();
      playBuf.clear();
      // playerCTRL.isSDeject=true;
    }
  //   xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  // }
}

void PSG_Cleanup(){
  playBuf.clear();
  // if (xSemaphoreTake(sdCardSemaphore, portMAX_DELAY) == pdTRUE) {
    sd_play_file.close();
  //   xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  // }
}

void PSG_Play(){
  if(skipCnt>0){
    skipCnt--;
  }else{
    while(playBuf.available()){
      byte b=playBuf.read();
      if(b==0xFF){break;}
      if(b==0xFD){ 
        // playerCTRL.currentFramePosition=0;
        // playerCTRL.isFinish=true;
        // playerCTRL.isPlay=false;
        // playFinish();
        break;
      }
      if(b==0xFE){ 
        skipCnt=playBuf.read()*4-1; // one tact is 20ms, it is necessary to skip times 80 ms minus this tact (20ms)
        break;
      }
      if(b<=0xFC){
        byte v=playBuf.read();
        if(b<16){
          ay_writeay(&AYInfo,b,v,0);
        }
      }
    }
  }
}

void RSF_Cleanup(){
  playBuf.clear();
  // if (xSemaphoreTake(sdCardSemaphore, portMAX_DELAY) == pdTRUE) {
    sd_play_file.close();
  //   xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  // }
}

void RSF_Play(){
  if(skipCnt>0){
    skipCnt--;
  }else{
    while(playBuf.available()>0){
      byte b=playBuf.read();
      byte v;
      if(b==0xFF){
        break;
        //return;
      }
      if(b==0xFE){ 
        if(playBuf.available()>0){
          skipCnt=playBuf.read()-1;
          break;
          //return;
        }
      }
      if(b==0xFD){ 
        // playerCTRL.currentFramePosition=0;
        // playerCTRL.isFinish=true;
        // playerCTRL.isPlay=false;
        // playBuf.clear();
        //playFinish();
        break;
      }
      if(b<=0xFC){
        byte mask1,mask2;
        if(playBuf.available()>0){
          mask2=b;
          mask1=playBuf.read();
          byte regg=0;
          while(mask1!=0){
            if(mask1&1){
              v=playBuf.read();
              ay_writeay(&AYInfo,regg,v,0);
              //readEQ(regg,v,AY_EQ);
            }
            mask1>>=1;
            regg++;
          }
          regg=8;
          while(mask2!=0){
            if(mask2&1){
              v=playBuf.read();
              ay_writeay(&AYInfo,regg,v,0);
              //readEQ(regg,v,AY_EQ);
            }
            mask2>>=1;
            regg++;
          }
        }
        break;
        //return;
      }
    }
  }
}

void YRG_Cleanup(){
  playBuf.clear();
  // if (xSemaphoreTake(sdCardSemaphore, portMAX_DELAY) == pdTRUE) {
    sd_play_file.close();
  //   xSemaphoreGive(sdCardSemaphore);  // Release the semaphore
  // }
}

void YRG_Play(){
  for(byte reg=0;reg<14;reg++){
    if(reg==13&&playBuf.peek()==255){playBuf.read();break;}
    byte v=playBuf.read();
    ay_writeay(&AYInfo,reg,v,0);
  }
  playBuf.read();
  playBuf.read();
  yrgFrame++;
  if(yrgFrame>bufSize/16){yrgFrame=0;}
  if(!sd_play_file.available()&&!playBuf.available()){
    //playFinish();
    yrgFrame=0;
    // playerCTRL.currentFramePosition=0;
    // playerCTRL.isFinish=true;
    // playerCTRL.isPlay=false;
  }
}