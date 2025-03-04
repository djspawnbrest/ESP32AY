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
  uint16_t frMax=(Config.zx_int)?TIMER_RATE*1000/50000:TIMER_RATE*1000/48828;
  switch(speed){
    case 0: // SLOW
    frMax=(Config.zx_int)?TIMER_RATE*1000/25000:TIMER_RATE*1000/24414;
      break;
    case 1: // NORMAL
      frMax=(Config.zx_int)?TIMER_RATE*1000/50000:TIMER_RATE*1000/48828;
      break;
    case 2: // FAST
      frMax=(Config.zx_int)?TIMER_RATE*1000/100000:TIMER_RATE*1000/97656;
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
  if(Sound.buf_rd&&PlayerCTRL.isPlay&&PlayerCTRL.music_type==TYPE_AY){  // else if AY buffer not empty (playing AY beeper)
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
  if(PlayerCTRL.music_type==TYPE_AY){
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
      if(PlayerCTRL.music_type!=TYPE_MOD&&PlayerCTRL.music_type!=TYPE_S3M) PlayerCTRL.trackFrame++;
    }
  }
}

void initOut(int buf=32){
  out=new AudioOutputI2S(0,AudioOutputI2S::INTERNAL_DAC,buf);  // I2S output
}

void DACInit(){
  initOut();
  DACTimer=timerBegin(0,79,true); // timer_id = 0; divider=79;(old 80) countUp = true;
  timerAttachInterrupt(DACTimer,&DACTimer_ISR,true); // edge = true
  timerAlarmWrite(DACTimer,(uint64_t)ceil((float)F_CPU/TIMER_RATE/(F_CPU/1000000L)),true); // round up from 22.675737 to 23
  timerAlarmEnable(DACTimer);
  sound_init();
}
