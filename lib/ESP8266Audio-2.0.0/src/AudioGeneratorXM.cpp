#define PGM_READ_UNALIGNED 0

#include "AudioGeneratorXM.h"

#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")

#ifndef min
#define min(X,Y)((X)<(Y)?(X):(Y))
#endif

#ifndef max
#define max(X,Y)((X)>(Y)?(X):(Y))
#endif

#define MSN(x)(((x)&0xf0)>>4)
#define LSN(x)((x)&0x0f)

extern "C"{
  void xm_set_eq_buffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer);
}

AudioGeneratorXM::~AudioGeneratorXM() {
  if(running){
    stop();
  }
}

AudioGeneratorXM::AudioGeneratorXM(){
  sampleRate=44100;
  stereoSeparation=32;
  usePAL=false;
  running=false;
  isPaused=false;
  Looped=false;
  output=NULL;
  file=NULL;
}

bool AudioGeneratorXM::initializeFile(AudioFileSource *source,const char** message){
  if(!source) return false;
  file=source;
  if(!file->isOpen()) return false; // Can't read the file!
  size_t fileSize=file->getSize();
  int status=xmCTX(file,fileSize);
  if(status<=0){
    switch(status){
      case 0:
      case -1:
        if(message) *message="Invalid XM file!";
        break;
      case -2:
        if(message) *message="XM file too big!";
        break;
    }
    return false;
  }
  SetSeparation(stereoSeparation);
  return true;
}

bool AudioGeneratorXM::begin(AudioFileSource *source,AudioOutput *out){
  if(running) stop();

  if(!out) return false;
  output=out;

  // Set the output values properly
  if(!output->SetRate(sampleRate)) return false;
  if(!output->SetBitsPerSample(16)) return false;
  if(!output->SetChannels(2)) return false;
  if(!output->begin()) return false;

  if(!file){
    if(!initializeFile(source)) return false;
  }

  initialize_xm(output,playerTask);
  
  // if(!file->isOpen()) return false; // Can't read the file!
  // size_t fileSize=file->getSize();
  // int status=xmCTX(file,fileSize);
  // if(status<=0) return false;
  // SetSeparation(stereoSeparation);

  setPause(false); // command to start playing

  running=true;
  return running;
}

void AudioGeneratorXM::setPause(bool pause){
  isPaused=!pause;
  XMPlaybackControl(isPaused); // false - pause, true - resume
}

bool AudioGeneratorXM::loop(){
  if(!running||!output){
    return false;
  }

  if(!playerTask) xm_player_loop();
  
  return true;
}

bool AudioGeneratorXM::stop(){
  if(!running) return true;  // Already stopped
  // Give time for any ongoing sample processing to complete
	vTaskDelay(pdMS_TO_TICKS(10));  
  stop_xm();
  // Flush output
  output->flush();  //flush I2S output buffer, if the player was actually running before.
  if(file&&file->isOpen()) file->close();
  if(output) output->stop();
  running=false;

  return true;
}

// Aditional protected
void AudioGeneratorXM::getTitle(char* lfn,size_t maxLen){
  if(!file){
    lfn[0]='\0';
    return;
  }
  const char* title=xm_get_title();
  if(title&&strlen(title)>0){
    strncpy(lfn,title,maxLen-1);
    lfn[maxLen-1]='\0';
    removeExtraSpaces(lfn);
  }else{
    lfn[0]='\0';
  }
}

uint8_t AudioGeneratorXM::getNumberOfChannels(){
  if(!file){
    return 0;
  }
  return xm_get_channels();
}

void AudioGeneratorXM::getDescription(char* description,size_t maxLen){
  if(!file){
    description[0]='\0';
    return;
  }
  xm_get_description(description,maxLen);
  removeExtraSpaces(description);
}

void AudioGeneratorXM::setSpeed(uint8_t speed){
  if(!running) return;
  xm_set_speed(speed);
}

void AudioGeneratorXM::SetSeparation(int sep){
  if (sep>=0&&sep<=64){
    stereoSeparation=sep;
    xm_set_separation(stereoSeparation);
  }
}

void AudioGeneratorXM::removeExtraSpaces(char* str){
  char* dest=str;
  bool inSpace=false;
  while(*str!='\0'){
    if(*str!=' '||(inSpace==false)){
      *dest++=*str;
    }
    inSpace=(*str==' ');
    str++;
  }
  *dest='\0';
  // Remove spaces at the end of a line
  if(dest>str&&*(dest-1)==' '){
    *(dest-1)='\0';
  }
}

void AudioGeneratorXM::initTrackFrame(unsigned long* tF){
  if(tF!=nullptr){
    xm_init_track_frame(tF);
    trackFrameInitialized=true;
  }
}

signed long AudioGeneratorXM::getPlaybackTime(bool oneFiftieth){
  if(!file) return -1;
  return xm_get_playback_time(oneFiftieth);
}

void AudioGeneratorXM::SetLoop(bool isLooped){
  Looped=isLooped;
  xm_set_loop(Looped);
}

void AudioGeneratorXM::initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer){
  this->eqBuffer=eqBuffer;
  this->channelEQBuffer=channelEQBuffer;
  // Clear buffers
  memset(eqBuffer,0,sizeof(uint8_t));
  memset(channelEQBuffer,0,sizeof(uint8_t));
  buffersInitialized=true; // Set the initialization flag
  // Pass buffers to XM engine
  xm_set_eq_buffers(eqBuffer,channelEQBuffer);
}

void AudioGeneratorXM::playerTaskEnable(bool enable){
  playerTask=enable;
}