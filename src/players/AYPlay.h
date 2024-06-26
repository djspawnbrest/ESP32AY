/*
  A completely custom AY file format loader/player,
  with warped address space to reduce amount of RAM
  By Shiru, created specifically for the hway project
  (unlike all other third party format players there)
*/

#include "z80/z80emu.h"

extern "C" {
  void mem_remap_wr(unsigned short int adr, unsigned char val);
  unsigned char mem_remap_rd(unsigned short int adr);
  int port_out(unsigned short int port, unsigned char value, int cycles);
};

//original ZX Spectrum ROM contents is needed by some beeper engines, mostly as a noise source, to have proper drum sound
//it may be replaced with 16K of random data, but it will be much noticeable by the ear in the (very popular) SpecialFX engine
#include "z80/spectrum_rom.h"

#define AY_Z80_CLOCK  3500000
#define AY_FRAME_RATE 50

#define PLAY_LOOP_ADR	0x4000

#define PAGE_SIZE 		256	//must be a power of two!
#define PAGE_SIZE_MASK	(PAGE_SIZE-1)
#define PAGE_UNDEF		255
#define PAGES_MAX		sizeof(music_data)/PAGE_SIZE

TaskHandle_t AYPlay;

uint8_t frqHz;

struct{
  uint8_t mem_remap[48*1024/PAGE_SIZE];
  uint8_t mem_remap_ptr;
  bool mem_remap_fail;
  int duration; //in 1/50 of second
  char author[32];
  char name[32];
  uint16_t init_adr;
  uint16_t init_sp;
  uint16_t interrupt_adr;
  uint8_t hi_reg;
  uint8_t lo_reg;
  uint8_t speaker_state;
  bool active;
  Z80_STATE z80;
  uint8_t ay_reg;
  uint8_t ay_data;
  uint8_t num_songs;
  int cycles_per_sample_fp;
  int prev_elapsed;
  int sample_acc;
  int cycles_acc;
}AyPlayer;

void mem_remap_reset(void){
  memset(music_data,0,sizeof(music_data));
  memset(AyPlayer.mem_remap,PAGE_UNDEF,sizeof(AyPlayer.mem_remap));
  AyPlayer.mem_remap_ptr=0;
  AyPlayer.mem_remap_fail=false;
}

void mem_remap_wr(unsigned short int adr,unsigned char val){
  if(adr<16*1024) return;
  int page_ptr=(adr-16384)/PAGE_SIZE;
  int page=AyPlayer.mem_remap[page_ptr];
  if(page==PAGE_UNDEF){
    if(AyPlayer.mem_remap_ptr<PAGES_MAX){
      //printf("adding page #%2.2x to offset %u\n",page_ptr,mem_remap_ptr);
      AyPlayer.mem_remap[page_ptr]=AyPlayer.mem_remap_ptr;
      page=AyPlayer.mem_remap_ptr;
      AyPlayer.mem_remap_ptr++;
    }else{
      //printf("remap fail, out of memory\n");
      AyPlayer.mem_remap_fail = true;
      return;
    }
  }
  music_data[page*PAGE_SIZE+(adr&PAGE_SIZE_MASK)]=val;
}

unsigned char mem_remap_rd(unsigned short int adr){
  if(adr<16*1024) return pgm_read_byte(&spectrum_rom[adr]);
  int page=AyPlayer.mem_remap[(adr-16384)/PAGE_SIZE];
  if(page==PAGE_UNDEF) return 0;
  return music_data[page*PAGE_SIZE+(adr&PAGE_SIZE_MASK)];
}

int AY_FlushBuf(int cycles);

int port_out(unsigned short int port,unsigned char value,int cycles){
  //Amstrad CPC ports
  if((port>>8)==0xf4){
    AyPlayer.ay_data=value;
    return 0;
  }
  if((port>>8)==0xf6){
    if((value&0xc0)==0xc0){
      AyPlayer.ay_reg=AyPlayer.ay_data;
    }
    else{
      ay_write_remap(0,AyPlayer.ay_reg,AyPlayer.ay_data);
    }
    return 0;
  }
  //ZX Spectrum ports
  if(!(port&0x0001)){
    if(AyPlayer.speaker_state!=(value&16)){
      int ret = AY_FlushBuf(cycles-AyPlayer.prev_elapsed);
      AyPlayer.speaker_state=value&16;
      AyPlayer.prev_elapsed=cycles;
      return ret;
    }
  }else{
    if(!(port&0x0002)){
      if(!(port&0x4000)){
        ay_write_remap(0,AyPlayer.ay_reg,value);
      }else{
        AyPlayer.ay_reg=value;
      }
    }
  }
  return 0;
}

uint16_t read_word_hl(const uint8_t* data){
  return (data[0]<<8)+data[1];
}

int AY_Load(AYSongInfo &info,int subsong){
  uint8_t header[768],data[64];
  sd_play_file.read((char*)header,sizeof(header));
  if(memcmp(header,"ZXAYEMUL",8)!=0) return FILE_ERR_UNK_FORMAT;
  mem_remap_reset();
  AyPlayer.num_songs=header[16];
  if(subsong>AyPlayer.num_songs) subsong=AyPlayer.num_songs;
  unsigned int song_author_off=12+read_word_hl(&header[12]);
  //unsigned int song_misc_off=14+read_word_hl(&header[14]);
  unsigned int song_st_off=18+read_word_hl(&header[18]);
  unsigned int song_name_off=song_st_off+0+subsong*4+read_word_hl(&header[song_st_off+0+subsong*4]);
  unsigned int song_data_off=song_st_off+2+subsong*4+read_word_hl(&header[song_st_off+2+subsong*4]);
  unsigned int pointers_off=song_data_off+10+read_word_hl(&header[song_data_off+10]);
  unsigned int blocks_off=song_data_off+12+read_word_hl(&header[song_data_off+12]);
  if((song_author_off>=sizeof(header)-1)||
     (song_name_off>=sizeof(header)-1)||
     (song_st_off>=sizeof(header)-1)||
     (song_data_off>=sizeof(header)-1)||
     (pointers_off>=sizeof(header)-1)){
    return FILE_ERR_TOO_LARGE;
  }
  AyPlayer.duration=read_word_hl(&header[song_data_off+4]);
  if(AyPlayer.duration==0) AyPlayer.duration=15000; // 5 minutes
  strncpy(AyPlayer.author,(char*)&header[song_author_off],sizeof(AyPlayer.author)-1);
  int i=strlen(AyPlayer.author)-1;
  while(AyPlayer.author[i]==32) AyPlayer.author[i--]=0;
  strncpy(AyPlayer.name,(char*)&header[song_name_off],sizeof(AyPlayer.name)-1);
  i=strlen(AyPlayer.name)-1;
  while(AyPlayer.name[i]==32) AyPlayer.name[i--]=0;
  AyPlayer.init_sp=read_word_hl(&header[pointers_off+0]);
  AyPlayer.init_adr=read_word_hl(&header[pointers_off+2]);
  AyPlayer.interrupt_adr=read_word_hl(&header[pointers_off+4]);
  AyPlayer.hi_reg=header[song_data_off+8];
  AyPlayer.lo_reg=header[song_data_off+9];
  while(1){
    if(blocks_off>=sizeof(header)-1) return FILE_ERR_TOO_LARGE;
    unsigned short adr=read_word_hl(&header[blocks_off+0]);
    if(adr==0) break;
    if(AyPlayer.init_adr==0) AyPlayer.init_adr=adr; //use the first block's loading address if no init address is set
    unsigned short len = read_word_hl(&header[blocks_off+2]);
    unsigned int off = blocks_off + 4 + read_word_hl(&header[blocks_off+4]);
    sd_play_file.seekSet(off);
    while(len>0){
      int part_size=len<sizeof(data)?len:sizeof(data);
      sd_play_file.read((char*)data,part_size);
      for(int i=0;i<part_size;i++) mem_remap_wr(adr+i,data[i]);
      if(AyPlayer.mem_remap_fail) return false;
      adr+=part_size;
      len-=part_size;
    }
    blocks_off+=6;
  }
  int adr=PLAY_LOOP_ADR;
  if(!AyPlayer.interrupt_adr){
    mem_remap_wr(adr+0,0xf3);	//di
    mem_remap_wr(adr+1,0xcd);	//call
    mem_remap_wr(adr+2,AyPlayer.init_adr&255);
    mem_remap_wr(adr+3,AyPlayer.init_adr/256);
    mem_remap_wr(adr+4,0xed);	//im2
    mem_remap_wr(adr+5,0x5e);
    mem_remap_wr(adr+6,0xfb);	//ei
    mem_remap_wr(adr+7,0x76);	//halt
    mem_remap_wr(adr+8,0x18);	//jr
    mem_remap_wr(adr+9,0xfa);	//#4004
  }else{
    mem_remap_wr(adr+0,0xf3);	//di
    mem_remap_wr(adr+1,0xcd);	//call
    mem_remap_wr(adr+2,AyPlayer.init_adr&255);
    mem_remap_wr(adr+3,AyPlayer.init_adr/256);
    mem_remap_wr(adr+4,0xed);	//im1
    mem_remap_wr(adr+5,0x56);
    mem_remap_wr(adr+6,0xfb);	//ei
    mem_remap_wr(adr+7,0x76);	//halt
    mem_remap_wr(adr+8,0xcd);	//call
    mem_remap_wr(adr+9,AyPlayer.interrupt_adr&255);
    mem_remap_wr(adr+10,AyPlayer.interrupt_adr/256);
    mem_remap_wr(adr+11,0x18);	//jr
    mem_remap_wr(adr+12,0xf7);	//#4004
  }
  //todo: would be nice to run a few frames of Z80 emulation to catch the out of RAM situation early on
  //return FILE_ERR_NOT_ENOUGH_MEMORY when it happens
  return FILE_ERR_NONE;
}



void AY_Init(AYSongInfo &info){
  AyPlayer.active=false;

  AyPlayer.speaker_state=0;
  AyPlayer.ay_reg=0;
  AyPlayer.cycles_per_sample_fp=(int)(AY_Z80_CLOCK*256.0/(double)TIMER_RATE);
  AyPlayer.sample_acc=0;
  AyPlayer.cycles_acc=0;

  Z80Reset(&AyPlayer.z80);

  AyPlayer.z80.pc=PLAY_LOOP_ADR;
  AyPlayer.z80.registers.word[Z80_SP]=AyPlayer.init_sp;
  AyPlayer.z80.registers.byte[Z80_A]=AyPlayer.hi_reg;
  AyPlayer.z80.registers.byte[Z80_F]=AyPlayer.lo_reg;
  AyPlayer.z80.registers.byte[Z80_B]=AyPlayer.hi_reg;
  AyPlayer.z80.registers.byte[Z80_C]=AyPlayer.lo_reg;
  AyPlayer.z80.registers.byte[Z80_D]=AyPlayer.hi_reg;
  AyPlayer.z80.registers.byte[Z80_E]=AyPlayer.lo_reg;
  AyPlayer.z80.registers.byte[Z80_H]=AyPlayer.hi_reg;
  AyPlayer.z80.registers.byte[Z80_L]=AyPlayer.lo_reg;
  AyPlayer.z80.registers.byte[Z80_IXH]=AyPlayer.hi_reg;
  AyPlayer.z80.registers.byte[Z80_IXL]=AyPlayer.lo_reg;
  AyPlayer.z80.registers.byte[Z80_IYH]=AyPlayer.hi_reg;
  AyPlayer.z80.registers.byte[Z80_IYL]=AyPlayer.lo_reg;

  AyPlayer.active=true;
}

//returns 1 when render buffer is all filled up, 0 otherwise
#define MAX_LEVEL 96

int AY_FlushBuf(int cycles){
  cycles=(cycles<<8);
  if(AyPlayer.speaker_state) AyPlayer.sample_acc+=cycles;
  AyPlayer.cycles_acc+=cycles;
  while(AyPlayer.cycles_acc>=AyPlayer.cycles_per_sample_fp){
    AyPlayer.cycles_acc-=AyPlayer.cycles_per_sample_fp;
    int s=AyPlayer.sample_acc;
    if(s>=AyPlayer.cycles_per_sample_fp){
      s=MAX_LEVEL;
      AyPlayer.sample_acc-=AyPlayer.cycles_per_sample_fp;
    }else{
      s=s*MAX_LEVEL/AyPlayer.cycles_per_sample_fp;
      AyPlayer.sample_acc=0;
    }
    Sound.buf_wr[Sound.buf_ptr_wr++]=s;
    if(Sound.buf_ptr_wr>=SOUND_BUF_MAX_SIZE) return 1;
  }
  return 0;
}

void AY_PlayBuf(){
  if(!AyPlayer.active) return;
  Sound.buf_ptr_wr=0;
  AyPlayer.prev_elapsed=0;
  Z80Interrupt(&AyPlayer.z80,0xff);
  Z80Emulate(&AyPlayer.z80,AY_Z80_CLOCK/AY_FRAME_RATE);
  while(!AY_FlushBuf(4));
}

void AY_GetInfo(AYSongInfo &info){
  info.Length=AyPlayer.duration?AyPlayer.duration:-1;
  ay_sys_getstr(info.Author,sizeof(info.Author),(unsigned char*)AyPlayer.author,strlen(AyPlayer.author));
  ay_sys_getstr(info.Name,sizeof(info.Name),(unsigned char*)AyPlayer.name,strlen(AyPlayer.name));
}

void AY_Cleanup(AYSongInfo &info){
  AyPlayer.active = false;
  memset(Sound.buf_rd,0,sizeof(Sound.buf_rd));
  memset(Sound.buf_wr,0,sizeof(Sound.buf_wr));
  memset(Sound.buf_1,0,sizeof(Sound.buf_1));
  memset(Sound.buf_2,0,sizeof(Sound.buf_2));
  Sound.buf_do_update=false;
}

bool AY_IsMemFail(){
  return AyPlayer.mem_remap_fail;
}

int AY_GetNumSongs(){
  return AyPlayer.num_songs;
}
