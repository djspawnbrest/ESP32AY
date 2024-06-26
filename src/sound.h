#define SOUND_BUF_MAX_SIZE  (TIMER_RATE/50)

hw_timer_t* DACTimer=NULL;

enum{
  PLAY_FAST=TIMER_RATE*1000/100000,
  PLAY_NORMAL=TIMER_RATE*1000/50000,
  PLAY_SLOW=TIMER_RATE*1000/25000
};

volatile uint32_t frame_cnt=0;
volatile uint32_t frame_div=0;
volatile uint32_t frame_max=PLAY_NORMAL;

const uint8_t* const sounds_list[] = {
  cancel_data,
  move_data,
  select_data,
  // startup_data,
};

struct{
  volatile uint8_t dac;
  volatile const uint8_t* sample_data;
  volatile int32_t sample_ptr;
  volatile int32_t sample_ptr_prev;
  volatile int32_t sample_step;
  volatile int32_t sample_len;
  uint8_t buf_1[SOUND_BUF_MAX_SIZE];
  uint8_t buf_2[SOUND_BUF_MAX_SIZE];
  uint8_t* buf_rd;
  uint8_t* buf_wr;
  int buf_ptr_rd;
  int buf_ptr_wr;
  bool buf_do_update;
  int int_counter;  //counts samples, can be used for profiling purposes
}Sound;

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
  Sound.dac=0x80;
  sound_clear_buf();
}

void sound_play(int id){
  Sound.sample_data=NULL;
  const uint8_t* data=sounds_list[id];
  int sample_rate=pgm_read_byte((const void*)&data[24])+(pgm_read_byte((const void*)&data[25])<<8);
  Sound.sample_ptr=44<<8;
  Sound.sample_ptr_prev=-1;
  Sound.sample_step=(sample_rate<<8)/TIMER_RATE;
  Sound.sample_len=(pgm_read_byte((const void*)&data[40])+(pgm_read_byte((const void*)&data[41])<<8)+(pgm_read_byte((const void*)&data[42])<<16))<<8;
  Sound.sample_data=data;
}

void sound_update(){
  if(Sound.sample_data){  //either a system sound or emulated beeper sound is active at a given moment, to avoid clipping
    if(!Config.sound_vol){
      Sound.dac=0x80;
    }else{
      if((Sound.sample_ptr&~0xff)!=(Sound.sample_ptr_prev&~0xff)){
        Sound.dac=0x80+((pgm_read_byte((const void*)&Sound.sample_data[Sound.sample_ptr>>8])-128)*Config.sound_vol/2);
        Sound.sample_ptr_prev=Sound.sample_ptr;
      }
      Sound.sample_ptr+=Sound.sample_step;
      Sound.sample_len-=Sound.sample_step;
      if(Sound.sample_len<=0){
        Sound.sample_data=NULL;
        Sound.dac=0x80;
      }
    }
  }else if(Sound.buf_rd&&PlayerCTRL.isPlay){
    Sound.dac=(PlayerCTRL.isPlay)?0x80+Sound.buf_rd[Sound.buf_ptr_rd++]:0x80;  //rendered beeper sound is always unsigned
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
    Sound.dac=0x80;
  }
  Sound.int_counter++;
}

void IRAM_ATTR DACTimer_ISR(){
  sigmaDeltaWrite(0,Sound.dac);
  sound_update();
  // counting frames
  if(PlayerCTRL.isPlay&&!PlayerCTRL.isFinish){
    frame_div++;
    if(frame_div>=frame_max){
      frame_div=0;
      frame_cnt++;
    }
  }
}

void DACInit(){
  sigmaDeltaSetup(DAC1,0,F_CPU/256); // 312500 freq
  DACTimer=timerBegin(0,80,true); // timer_id = 0; divider=80; countUp = true;
  timerAttachInterrupt(DACTimer,&DACTimer_ISR,true); // edge = true
  timerAlarmWrite(DACTimer,F_CPU/TIMER_RATE/(F_CPU/1000000),true); //1000 ms
  timerAlarmEnable(DACTimer);
  sound_init();
}