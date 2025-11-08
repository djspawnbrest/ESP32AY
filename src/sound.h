#include <Arduino.h>
#include "driver/i2s.h"
#include <AudioOutputI2S.h>

AudioOutputI2S *out;

#define SOUND_BUF_MAX_SIZE (TIMER_RATE/50)

int16_t sampleZX[2];

hw_timer_t* DACTimer=NULL;

struct SoundData{
  const unsigned char* data;
  size_t size;
};

struct{
  volatile uint8_t dac;
  uint8_t buf_1[SOUND_BUF_MAX_SIZE];
  uint8_t buf_2[SOUND_BUF_MAX_SIZE];
  uint8_t* buf_rd;
  uint8_t* buf_wr;
  int buf_ptr_rd;
  int buf_ptr_wr;
  bool buf_do_update;
  int int_counter;  //counts samples, can be used for profiling purposes
}Sound;

uint16_t frameMax(uint8_t speed=1){ // NORMAL default
  uint16_t frMax=(lfsConfig.zx_int)?TIMER_RATE*1000/50075:TIMER_RATE*1000/48880;
  switch(speed){
    case 0: // SLOW
    frMax=(lfsConfig.zx_int)?TIMER_RATE*1000/25037:TIMER_RATE*1000/24440;
      break;
    case 1: // NORMAL
      frMax=(lfsConfig.zx_int)?TIMER_RATE*1000/50075:TIMER_RATE*1000/48880;
      break;
    case 2: // FAST
      frMax=(lfsConfig.zx_int)?TIMER_RATE*1000/100150:TIMER_RATE*1000/97760;
      break;
  }
  return frMax;
}

void sound_clear_buf(){
  memset(Sound.buf_1,0,sizeof(Sound.buf_1));
  memset(Sound.buf_2,0,sizeof(Sound.buf_2));
  Sound.buf_rd=Sound.buf_1;
  Sound.buf_wr=Sound.buf_2;
  Sound.buf_ptr_rd=0;
  Sound.buf_ptr_wr=0;
  Sound.buf_do_update=false;
}

void sound_init(){
  memset(&Sound,0,sizeof(Sound));
  Sound.dac=0x00; // silent on dac
}

void sound_update(){
  if(Sound.buf_rd&&PlayerCTRL.isPlay&&PlayerCTRL.music_type==TYPE_AY&&ayPlayerAct){  // else if AY buffer not empty (playing AY beeper)
    Sound.dac=0x80+((Sound.buf_rd[Sound.buf_ptr_rd++])*7/10);  // rendered beeper sound is always unsigned
    if (Sound.buf_ptr_rd>=SOUND_BUF_MAX_SIZE){
      Sound.buf_ptr_rd=0;
      Sound.buf_do_update=true;
      if (Sound.buf_rd==Sound.buf_1){
        Sound.buf_rd=Sound.buf_2;
        Sound.buf_wr=Sound.buf_1;
      }else{
        Sound.buf_rd=Sound.buf_1;
        Sound.buf_wr=Sound.buf_2;
      }
    }
  }else{ 
    Sound.dac=0x00; // silent on dac
  }
  Sound.int_counter++;
}

void IRAM_ATTR DACTimer_ISR(){
  BaseType_t xHigherPriorityTaskWoken=pdFALSE;
  // DAC only for ay format
  if(PlayerCTRL.music_type==TYPE_AY&&ayPlayerAct&&out&&PlayerCTRL.isPlay&&!PlayerCTRL.isFinish){
    if(Sound.dac){
      sampleZX[0]=sampleZX[1]=Sound.dac<<8;
      if(xSemaphoreTakeFromISR(outSemaphore,&xHigherPriorityTaskWoken)==pdTRUE){  
        if(!out->ConsumeSample(sampleZX)){
          xSemaphoreGiveFromISR(outSemaphore,&xHigherPriorityTaskWoken);  // Release the semaphore
          goto done;
        }
        xSemaphoreGiveFromISR(outSemaphore,&xHigherPriorityTaskWoken);  // Release the semaphore
      }
    }
  }
  sound_update();
done:
  if(xHigherPriorityTaskWoken==pdTRUE){
    portYIELD_FROM_ISR();
  }
  // counting frames
  if(PlayerCTRL.isPlay){
    frame_div++;
    if(frame_div>=frame_max){
      frame_div=0;
      frame_cnt++;
      if(PlayerCTRL.music_type!=TYPE_MOD
        &&PlayerCTRL.music_type!=TYPE_S3M
      #if defined(CONFIG_IDF_TARGET_ESP32S3)
        &&PlayerCTRL.music_type!=TYPE_XM
      #endif
        &&PlayerCTRL.music_type!=TYPE_TAP
        &&PlayerCTRL.music_type!=TYPE_TZX
        &&PlayerCTRL.music_type!=TYPE_MP3
        &&PlayerCTRL.music_type!=TYPE_WAV
      ) PlayerCTRL.trackFrame++;
    }
  }
}

void setDacGain(){
  if(out) out->SetGain(lfsConfig.dacGain);
}

void initOut(int buf=32){
  muteAmp();
  if(out){
    out->stop();
    vTaskDelay(pdMS_TO_TICKS(10));
    delete out;
    out=nullptr;
  }
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(USE_EXTERNAL_DAC)
  out=new AudioOutputI2S(0,AudioOutputI2S::EXTERNAL_I2S,buf);
  out->SetPinout(PIN_BCK,PIN_LCK,PIN_DIN);
  out->SetGain(lfsConfig.dacGain);
#else
  out=new AudioOutputI2S(0,AudioOutputI2S::INTERNAL_DAC,buf);  // I2S output
#endif
  unMuteAmp();
}

void DACInit(){
  initOut();
  DACTimer=timerBegin(0,80,true); // timer_id = 0; divider=79;(old 80) countUp = true;
  timerAttachInterrupt(DACTimer,&DACTimer_ISR,true); // edge = true
  timerAlarmWrite(DACTimer,1000000/TIMER_RATE,true);
  timerAlarmEnable(DACTimer);
  sound_init();
}
