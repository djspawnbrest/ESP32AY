/*
  AudioGeneratorMOD
  Audio output generator that plays Amiga MOD tracker files
    
  Copyright (C) 2017  Earle F. Philhower,III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation,either version 3 of the License,or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not,see <http://www.gnu.org/licenses/>.
*/
#define PGM_READ_UNALIGNED 0

#include "AudioGeneratorMOD.h"

#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")

// #define NOTE(r,c)(Player.currentPattern.note8[r][c]==NONOTE8?NONOTE:8*Player.currentPattern.note8[r][c])
#if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
#define NOTE(r,c)((Player.usingPsramPattern?Player.psramPattern->note8[r][c]:Player.currentPattern.note8[r][c])==NONOTE8?NONOTE:8*(Player.usingPsramPattern?Player.psramPattern->note8[r][c]:Player.currentPattern.note8[r][c]))
#else
#define NOTE(r,c)(Player.currentPattern.note8[r][c]==NONOTE8?NONOTE:8*Player.currentPattern.note8[r][c])
#endif

#ifndef min
#define min(X,Y)((X)<(Y)?(X):(Y))
#endif

// Sorted Amiga periods
static const uint16_t amigaPeriods[296]PROGMEM={
  907,900,894,887,881,875,868,862,//  -8 to -1
  856,850,844,838,832,826,820,814,// C-1 to +7
  808,802,796,791,785,779,774,768,// C#1 to +7
  762,757,752,746,741,736,730,725,// D-1 to +7
  720,715,709,704,699,694,689,684,// D#1 to +7
  678,675,670,665,660,655,651,646,// E-1 to +7
  640,636,632,628,623,619,614,610,// F-1 to +7
  604,601,597,592,588,584,580,575,// F#1 to +7
  570,567,563,559,555,551,547,543,// G-1 to +7
  538,535,532,528,524,520,516,513,// G#1 to +7
  508,505,502,498,494,491,487,484,// A-1 to +7
  480,477,474,470,467,463,460,457,// A#1 to +7
  453,450,447,444,441,437,434,431,// B-1 to +7
  428,425,422,419,416,413,410,407,// C-2 to +7
  404,401,398,395,392,390,387,384,// C#2 to +7
  381,379,376,373,370,368,365,363,// D-2 to +7
  360,357,355,352,350,347,345,342,// D#2 to +7
  339,337,335,332,330,328,325,323,// E-2 to +7
  320,318,316,314,312,309,307,305,// F-2 to +7
  302,300,298,296,294,292,290,288,// F#2 to +7
  285,284,282,280,278,276,274,272,// G-2 to +7
  269,268,266,264,262,260,258,256,// G#2 to +7
  254,253,251,249,247,245,244,242,// A-2 to +7
  240,238,237,235,233,232,230,228,// A#2 to +7
  226,225,223,222,220,219,217,216,// B-2 to +7
  214,212,211,209,208,206,205,203,// C-3 to +7
  202,200,199,198,196,195,193,192,// C#3 to +7
  190,189,188,187,185,184,183,181,// D-3 to +7
  180,179,177,176,175,174,172,171,// D#3 to +7
  170,169,167,166,165,164,163,161,// E-3 to +7
  160,159,158,157,156,155,154,152,// F-3 to +7
  151,150,149,148,147,146,145,144,// F#3 to +7
  143,142,141,140,139,138,137,136,// G-3 to +7
  135,134,133,132,131,130,129,128,// G#3 to +7
  127,126,125,125,123,123,122,121,// A-3 to +7
  120,119,118,118,117,116,115,114,// A#3 to +7
  113,113,112,111,110,109,109,108  // B-3 to +7
};

#define ReadAmigaPeriods(a)(uint16_t)pgm_read_word(amigaPeriods+(a))

static const uint16_t zxPeriods[3][96]PROGMEM={
  { // Period 1
//C-1 C#1 D-1 D#1 E-1 F-1 F#1 G-1 G#1 A-1 A#1 B-1
  856,808,762,720,678,640,604,570,538,508,480,453,
//C-2 C#2 D-2 D#2 E-2 F-2 F#2 G-2 G#2 A-2 A#2 B-2
  850,802,757,715,675,636,601,567,535,505,477,450,
//C-3 C#3 D-3 D#3 E-3 F-3 F#3 G-3 G#3 A-3 A#3 B-3
  844,796,752,709,670,632,597,563,532,502,474,447,
//C-4 C#4 D-4 D#4 E-4 F-4 F#4 G-4 G#4 A-4 A#4 B-4
  838,791,746,704,665,628,592,559,528,498,470,444,
//C-5 C#5 D-5 D#5 E-5 F-5 F#5 G-5 G#5 A-5 A#5 B-5
  832,785,741,699,660,623,588,555,524,494,467,441,
// C-6 C#6 D-6 D#6 E-6 F-6 F#6 G-6 G#6 A-6 A#6 B-6
  826,779,736,694,655,619,584,551,520,491,463,437,
//C-7 C#7 D-7 D#7 E-7 F-7 F#7 G-7 G#7 A-7 A#7 B-7
  820,774,730,689,651,614,580,547,516,487,460,434,
//C-8 C#8 D-8 D#8 E-8 F-8 F#8 G-8 G#8 A-8 A#8 B-8
  814,768,725,684,646,610,575,543,513,484,457,431
 },
 { // Period 2
//C-1 C#1 D-1 D#1 E-1 F-1 F#1 G-1 G#1 A-1 A#1 B-1
  428,404,381,360,339,320,302,285,269,254,240,226,
//C-2 C#2 D-2 D#2 E-2 F-2 F#2 G-2 G#2 A-2 A#2 B-2
  425,401,379,357,337,318,300,284,268,253,238,225,
//C-3 C#3 D-3 D#3 E-3 F-3 F#3 G-3 G#3 A-3 A#3 B-3
  422,398,376,355,335,316,298,282,266,251,237,223,
//C-4 C#4 D-4 D#4 E-4 F-4 F#4 G-4 G#4 A-4 A#4 B-4
  419,395,373,352,332,314,296,280,264,249,235,222,
//C-5 C#5 D-5 D#5 E-5 F-5 F#5 G-5 G#5 A-5 A#5 B-5
  416,392,370,350,330,312,294,278,262,247,233,220,
//C-6 C#6 D-6 D#6 E-6 F-6 F#6 G-6 G#6 A-6 A#6 B-6
  413,390,368,347,328,309,292,276,260,245,232,219,
//C-7 C#7 D-7 D#7 E-7 F-7 F#7 G-7 G#7 A-7 A#7 B-7
  410,387,365,345,325,307,290,274,258,244,230,217,
//C-8 C#8 D-8 D#8 E-8 F-8 F#8 G-8 G#8 A-8 A#8 B-8
  407,384,363,342,323,305,288,272,256,242,228,216
 },
 { //Period 3
//C-1 C#1 D-1 D#1 E-1 F-1 F#1 G-1 G#1 A-1 A#1 B-1
  214,212,211,209,208,206,205,203,202,200,199,198,
//C-2 C#2 D-2 D#2 E-2 F-2 F#2 G-2 G#2 A-2 A#2 B-2
  196,195,193,192,190,189,188,187,185,184,183,181,
//C-3 C#3 D-3 D#3 E-3 F-3 F#3 G-3 G#3 A-3 A#3 B-3
  180,179,177,176,175,174,172,171,170,169,167,166,
//C-4 C#4 D-4 D#4 E-4 F-4 F#4 G-4 G#4 A-4 A#4 B-4
  165,164,163,161,160,159,158,157,156,155,154,152,
//C-5 C#5 D-5 D#5 E-5 F-5 F#5 G-5 G#5 A-5 A#5 B-5
  151,150,149,148,147,146,145,144,143,142,141,140,
//C-6 C#6 D-6 D#6 E-6 F-6 F#6 G-6 G#6 A-6 A#6 B-6
  139,138,137,136,135,134,133,132,131,130,129,128,
//C-7 C#7 D-7 D#7 E-7 F-7 F#7 G-7 G#7 A-7 A#7 B-7
  127,126,125,125,123,123,122,121,120,119,118,118,
//C-8 C#8 D-8 D#8 E-8 F-8 F#8 G-8 G#8 A-8 A#8 B-8
  117,116,115,114,113,113,112,111,110,109,109,108
 }
};

static const uint8_t sine[64]PROGMEM={
  0,24,49,74,97,120,141,161,
  180,197,212,224,235,244,250,253,
  255,253,250,244,235,224,212,197,
  180,161,141,120,97,74,49,24
};

#define ReadSine(a)pgm_read_byte(sine+(a))

static inline uint16_t MakeWord(uint8_t h,uint8_t l){return h<<8|l;}

AudioGeneratorMOD::AudioGeneratorMOD(){
  sampleRate=44100;
  samplerateOriginal=sampleRate;
  #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
  fatBufferSize=6*1024;
  #endif
  stereoSeparation=32;
  mixerTick=0;
  usePAL=false;
  UpdateAmiga();
  running=false;
  oldFormat=false;
  isPaused=false;
  file=NULL;
  output=NULL;
  // Initialize Player structure
	memset(&Player,0,sizeof(Player));
  // Initialize Mixer structure
	memset(&Mixer,0,sizeof(Mixer));
	// Initialize FatBuffer
  #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
	memset(&FatBuffer,0,sizeof(FatBuffer));
  #endif
  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  // Allocate pattern in PSRAM
  Pattern* psramPattern = (Pattern*)ps_malloc(sizeof(Pattern));
  if(psramPattern){
    memset(psramPattern,0,sizeof(Pattern));
    Player.psramPattern=psramPattern;
    Player.usingPsramPattern=true;
  }
  #endif
}

AudioGeneratorMOD::~AudioGeneratorMOD(){
  freeFatBuffer();
}

bool AudioGeneratorMOD::stop(){
  if(!running) return true;  // Already stopped
	stopping=true;
  // Give time for any ongoing sample processing to complete
	vTaskDelay(pdMS_TO_TICKS(10));
  // Flush output
  output->flush();  //flush I2S output buffer,if the player was actually running before.
  if(file&&file->isOpen()) file->close();
  if(output) output->stop();
  running=false;
  if(!bufferFreed){
		freeFatBuffer();
		bufferFreed=true;
	}
  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  if(Player.usingPsramPattern&&Player.psramPattern){
    free(Player.psramPattern);
    Player.psramPattern=nullptr;
    Player.usingPsramPattern=false;
  }
  #endif
  return true;
}

void AudioGeneratorMOD::setPause(bool pause){
  isPaused=pause;
}

bool AudioGeneratorMOD::loop(){
  if(isPaused) goto done; // Easy-peasy
  if(!running) goto done; // Easy-peasy
  // First,try and push in the stored sample.  If we can't,then punt and try later
  if(!output->ConsumeSample(lastSample)) goto done; // FIFO full,wait...

  // Now advance enough times to fill the i2s buffer
  do{
    if(mixerTick==0){
      running=RunPlayer();
      if(!running){
        stop();
        goto done;
      }
      mixerTick=Player.samplesPerTick;
    }
    GetSample(lastSample);
    mixerTick--;
  }while(output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();
  return running;
}

bool AudioGeneratorMOD::initializeFile(AudioFileSource *source){
  if(!source) return false;
  file=source;
  return true;
}

bool AudioGeneratorMOD::begin(AudioFileSource *source,AudioOutput *out){
  if(running) stop();

  if(!file){
    if(!initializeFile(source)) return false;
  }
  if(!out) return false;
  output=out;
  
  if(!file->isOpen()) return false; // Can't read the file!

  // Set the output values properly
  if(!output->SetRate(sampleRate)) return false;
  if(!output->SetBitsPerSample(16)) return false;
  if(!output->SetChannels(2)) return false;
  if(!output->begin()) return false;

  UpdateAmiga();

  if(!LoadMOD()){
    stop();
    return false;
  }

  #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
  // Initialize buffer for s3m channels
	for(uint8_t channel=0;channel<Mod.numberOfChannels;channel++){
		FatBuffer.samplePointer[channel]=0;
		FatBuffer.channelSampleNumber[channel]=0xFF;
		// Allocate buffer
		FatBuffer.channels[channel]=(uint8_t*)malloc(fatBufferSize);
		if(!FatBuffer.channels[channel]){
			// Allocation failed-clean up
			freeFatBuffer();
			return false;
		}
	}
  #endif

  running=true;
  return true;
}

bool AudioGeneratorMOD::LoadHeader(){
  uint8_t i;
  uint8_t temp[4];
  uint8_t junk[22];
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
  // Offset 1080
  file->seek(1080,SEEK_SET); // Seek to offset 1080
  if(4!=file->read(temp,4)) return false;
  if(!strncmp(reinterpret_cast<const char*>(temp),"2CHN",4)){
    Mod.numberOfChannels=2;
  }else if(!strncmp(reinterpret_cast<const char*>(temp),"M.K.",4)||
    !strncmp(reinterpret_cast<const char*>(temp),"M!K!",4)||
    !strncmp(reinterpret_cast<const char*>(temp),"4CHN",4)||
	  !strncmp(reinterpret_cast<const char*>(temp),"FLT4",4)){
	  Mod.numberOfChannels=4;
  }else if(!strncmp(reinterpret_cast<const char*>(temp),"6CHN",4)){
	  Mod.numberOfChannels=6;
  }else if(!strncmp(reinterpret_cast<const char*>(temp),"8CHN",4)||
    !strncmp(reinterpret_cast<const char*>(temp),"OKTA",4)||
    !strncmp(reinterpret_cast<const char*>(temp),"FLT8",4)||
    !strncmp(reinterpret_cast<const char*>(temp),"CD81",4)){
	  Mod.numberOfChannels=8;
  }else if(!strncmp(reinterpret_cast<const char*>(temp+2),"CH",2)){
    Mod.numberOfChannels=(temp[0]-'0')*10+temp[1]-'0';
  }else{
    // Check for old format with 15 samples
    Mod.numberOfChannels=4; // Default to 4 channels if unknown
    file->seek(1080,SEEK_SET); // Seek to offset 1080
    if(4!=file->read(temp,4)) return false;
    if(strncmp(reinterpret_cast<const char*>(temp),"2CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"M.K.",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"M!K!",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"4CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"FLT4",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"6CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"8CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"OKTA",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"FLT8",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"CD81",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp+2),"CH",2)!=0){
      // No known signature found,assume old format
      oldFormat=true;
    }
  }
  // Return the pointer to its original position
  file->seek(currentPos,SEEK_SET);
  // Detecting old format or not
  uint8_t smpls=(oldFormat)?15:SAMPLES;

  if(20!=file->read(junk,20)) return false; // Skip MOD name
  for(i=0;i<smpls;i++){
    if(22!=file->read(junk,22)) return false; // Skip sample name
    if(2!=file->read(temp,2)) return false;
    Mod.samples[i].length=MakeWord(temp[0],temp[1])*2;
    if(1!=file->read(reinterpret_cast<uint8_t*>(&Mod.samples[i].fineTune),1)) return false;
    if(Mod.samples[i].fineTune>7) Mod.samples[i].fineTune-=16;
    if(1!=file->read(&Mod.samples[i].volume,1)) return false;
    if(2!=file->read(temp,2)) return false;
    Mod.samples[i].loopBegin=MakeWord(temp[0],temp[1])*2;
    if(2!=file->read(temp,2)) return false;
    Mod.samples[i].loopLength=MakeWord(temp[0],temp[1])*2;
    if(Mod.samples[i].loopBegin+Mod.samples[i].loopLength>Mod.samples[i].length)
      Mod.samples[i].loopLength=Mod.samples[i].length-Mod.samples[i].loopBegin;
  }

  if(1!=file->read(&Mod.songLength,1)) return false;
  if(1!=file->read(temp,1)) return false; // Discard this byte

  Mod.numberOfPatterns=0;
  for(i=0;i<128;i++){
    if(1!=file->read(&Mod.order[i],1)) return false;
    if(Mod.order[i]>Mod.numberOfPatterns)
      Mod.numberOfPatterns=Mod.order[i];
  }
  Mod.numberOfPatterns++;
  
  if(Mod.numberOfChannels>CHANNELS){
    audioLogger->printf("\nAudioGeneratorMOD::LoadHeader abort-too many channels (configured: %d,needed: %d)\n",CHANNELS,Mod.numberOfChannels);
    return(false);
  }

  return true;
}

void AudioGeneratorMOD::LoadSamples(){
  uint8_t i;
  uint32_t formatOffset=(oldFormat)?600:1084;
  uint8_t smpls=(oldFormat)?15:SAMPLES;
  uint32_t fileOffset=formatOffset+Mod.numberOfPatterns*ROWS*Mod.numberOfChannels*4-1;
  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  uint32_t initialPos=file->getPos();
  #endif
  for(i=0;i<smpls;i++){
    if(Mod.samples[i].length){
      Mixer.sampleBegin[i]=fileOffset;
      Mixer.sampleEnd[i]=fileOffset+Mod.samples[i].length;
      if(Mod.samples[i].loopLength>2){
        Mixer.sampleloopBegin[i]=fileOffset+Mod.samples[i].loopBegin;
        Mixer.sampleLoopLength[i]=Mod.samples[i].loopLength;
        Mixer.sampleLoopEnd[i]=Mixer.sampleloopBegin[i]+Mixer.sampleLoopLength[i];
      }else{
        Mixer.sampleloopBegin[i]=0;
        Mixer.sampleLoopLength[i]=0;
        Mixer.sampleLoopEnd[i]=0;
      }
      fileOffset+=Mod.samples[i].length;
    }
  }
  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  //read samples in PSRAM
  for(i=0;i<smpls;i++){
    if(Mod.samples[i].length){
      Mod.samples[i].data=(uint8_t*)ps_malloc(Mod.samples[i].length);
      if(!Mod.samples[i].data){
        printf("Failed to allocate PSRAM for sample [%d] data\n",i);
        free(Mod.samples[i].data);
        file->seek(initialPos,SEEK_SET);
        return;
			}
      file->seek(Mixer.sampleBegin[i],SEEK_SET); //set position to sample begin
      if(file->read(Mod.samples[i].data,Mod.samples[i].length)!=Mod.samples[i].length){
        printf("Failed to read raw sample data\n");
        free(Mod.samples[i].data);
        file->seek(initialPos,SEEK_SET);
        return;
      }
      Mod.samples[i].isAllocated=true;
    }
    // printf("Free PSRAM: %d bytes after sample [%d]\n",ESP.getFreePsram(),i);
  }
  // printf("Free PSRAM: %d bytes after samples load.\n",ESP.getFreePsram());
  file->seek(initialPos,SEEK_SET);
  #endif
}

bool AudioGeneratorMOD::LoadPattern(uint8_t pattern){
  uint8_t row;
  uint8_t channel;
  uint8_t i;
  uint8_t temp[4];
  uint16_t amigaPeriod;
  uint32_t  formatOffset=(oldFormat)?600:1084;
  if(!file->seek(formatOffset+pattern*ROWS*Mod.numberOfChannels*4,SEEK_SET)) return false;
  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  if(Player.usingPsramPattern&&Player.psramPattern){
    free(Player.psramPattern);
    Player.psramPattern=nullptr;
    Player.usingPsramPattern=false;
  }
  Pattern* currentPattern=Player.usingPsramPattern?Player.psramPattern:&Player.currentPattern;
  #else
  Pattern* currentPattern = &Player.currentPattern;
  #endif
  for(row=0;row<ROWS;row++){
    for(channel=0;channel<Mod.numberOfChannels;channel++){
      if(4!=file->read(temp,4)) return false;
      Player.currentPattern.sampleNumber[row][channel]=(temp[0]&0xF0)+(temp[2]>>4);
      amigaPeriod=((temp[0]&0xF)<<8)+temp[1];
        // Player.currentPattern.note[row][channel]=NONOTE;
      Player.currentPattern.note8[row][channel]=NONOTE8;
      for(i=1;i<37;i++)
        if(amigaPeriod>ReadAmigaPeriods(i*8)-3&&
            amigaPeriod<ReadAmigaPeriods(i*8)+3)
          Player.currentPattern.note8[row][channel]=i;

      Player.currentPattern.effectNumber[row][channel]=temp[2]&0xF;
      Player.currentPattern.effectParameter[row][channel]=temp[3];
    }
  }
  return true;
}

void AudioGeneratorMOD::Portamento(uint8_t channel){
  if(Player.lastAmigaPeriod[channel]<Player.portamentoNote[channel]){
    Player.lastAmigaPeriod[channel]+=Player.portamentoSpeed[channel];
    if(Player.lastAmigaPeriod[channel]>Player.portamentoNote[channel])
      Player.lastAmigaPeriod[channel]=Player.portamentoNote[channel];
  }
  if(Player.lastAmigaPeriod[channel]>Player.portamentoNote[channel]){
    Player.lastAmigaPeriod[channel]-=Player.portamentoSpeed[channel];
    if(Player.lastAmigaPeriod[channel]<Player.portamentoNote[channel])
      Player.lastAmigaPeriod[channel]=Player.portamentoNote[channel];
  }
  Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
}

void AudioGeneratorMOD::Vibrato(uint8_t channel){
  uint16_t delta;
  uint16_t temp;
  temp=Player.vibratoPos[channel]&31;
  switch(Player.waveControl[channel]&3){
    case 0:
      delta=ReadSine(temp);
      break;
    case 1:
      temp<<=3;
      if(Player.vibratoPos[channel]<0)
        temp=255-temp;
      delta=temp;
      break;
    case 2:
      delta=255;
      break;
    case 3:
      delta=rand()&255;
      break;
  }
  delta*=Player.vibratoDepth[channel];
  delta>>=7;
  if(Player.vibratoPos[channel]>=0)
    Mixer.channelFrequency[channel]=Player.amiga/(Player.lastAmigaPeriod[channel]+delta);
  else
    Mixer.channelFrequency[channel]=Player.amiga/(Player.lastAmigaPeriod[channel]-delta);
  Player.vibratoPos[channel]+=Player.vibratoSpeed[channel];
  if(Player.vibratoPos[channel]>31) Player.vibratoPos[channel]-=64;
}

void AudioGeneratorMOD::Tremolo(uint8_t channel){
  uint16_t delta;
  uint16_t temp;
  temp=Player.tremoloPos[channel]&31;
  switch(Player.waveControl[channel]&3){
    case 0:
      delta=ReadSine(temp);
      break;
    case 1:
      temp<<=3;
      if(Player.tremoloPos[channel]<0)
        temp=255-temp;
      delta=temp;
      break;
    case 2:
      delta=255;
      break;
    case 3:
      delta=rand()&255;
      break;
  }
  delta*=Player.tremoloDepth[channel];
  delta>>=6;
  if(Player.tremoloPos[channel]>=0){
    if(Player.volume[channel]+delta>64) delta=64-Player.volume[channel];
    Mixer.channelVolume[channel]=Player.volume[channel]+delta;
  }else{
    if(Player.volume[channel]-delta<0) delta=Player.volume[channel];
    Mixer.channelVolume[channel]=Player.volume[channel]-delta;
  }
  Player.tremoloPos[channel]+=Player.tremoloSpeed[channel];
  if(Player.tremoloPos[channel]>31) Player.tremoloPos[channel]-=64;
}

bool AudioGeneratorMOD::ProcessRow(){
  bool jumpFlag;
  bool breakFlag;
  uint8_t channel;
  uint8_t sampleNumber;
  uint16_t note;
  uint8_t effectNumber;
  uint8_t effectParameter;
  uint8_t effectParameterX;
  uint8_t effectParameterY;
  uint16_t sampleOffset;

  int8_t volSum[96]={0}; // Array for summing records
  int noteCount[96]={0}; // Array for counting the number of notes encountered

  if(!running) return false;

  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  Pattern* currentPattern=Player.usingPsramPattern?Player.psramPattern:&Player.currentPattern;
  #else
  Pattern* currentPattern = &Player.currentPattern;
  #endif

  Player.lastRow=Player.row++;
  jumpFlag=false;
  breakFlag=false;
  for(channel=0;channel<Mod.numberOfChannels; channel++){
    sampleNumber=Player.currentPattern.sampleNumber[Player.lastRow][channel];
    note=NOTE(Player.lastRow,channel);
    effectNumber=Player.currentPattern.effectNumber[Player.lastRow][channel];
    effectParameter=Player.currentPattern.effectParameter[Player.lastRow][channel];
    effectParameterX=effectParameter>>4;
    effectParameterY=effectParameter&0xF;
    sampleOffset=0;
    if(sampleNumber){
      Player.lastSampleNumber[channel]=sampleNumber-1;
      if(!(effectNumber==0xE&&effectParameterX==NOTEDELAY))
        Player.volume[channel]=Mod.samples[Player.lastSampleNumber[channel]].volume;
    }
    if(note!=NONOTE){
      Player.lastNote[channel]=note;
      Player.amigaPeriod[channel]=ReadAmigaPeriods(note+Mod.samples[Player.lastSampleNumber[channel]].fineTune);
      if(effectNumber!=TONEPORTAMENTO&&effectNumber!=PORTAMENTOVOLUMESLIDE)
        Player.lastAmigaPeriod[channel]=Player.amigaPeriod[channel];
      if(!(Player.waveControl[channel]&0x80)) Player.vibratoPos[channel]=0;
      if(!(Player.waveControl[channel]&0x08)) Player.tremoloPos[channel]=0;
    }
    switch(effectNumber){
      case TONEPORTAMENTO:
        if(effectParameter) Player.portamentoSpeed[channel]=effectParameter;
        Player.portamentoNote[channel]=Player.amigaPeriod[channel];
        note=NONOTE;
        break;
      case VIBRATO:
        if(effectParameterX) Player.vibratoSpeed[channel]=effectParameterX;
        if(effectParameterY) Player.vibratoDepth[channel]=effectParameterY;
        break;
      case PORTAMENTOVOLUMESLIDE:
        Player.portamentoNote[channel]=Player.amigaPeriod[channel];
        note=NONOTE;
        break;
      case TREMOLO:
        if(effectParameterX) Player.tremoloSpeed[channel]=effectParameterX;
        if(effectParameterY) Player.tremoloDepth[channel]=effectParameterY;
        break;
      case SETCHANNELPANNING:
        Mixer.channelPanning[channel]=effectParameter>>1;
        break;
      case SETSAMPLEOFFSET:
        sampleOffset=effectParameter<<8;
        if(sampleOffset>Mod.samples[Player.lastSampleNumber[channel]].length)
          sampleOffset=Mod.samples[Player.lastSampleNumber[channel]].length;
        if(jumpFlag||breakFlag) sampleOffset=0;
        break;
      case JUMPTOORDER:
        Player.orderIndex=effectParameter;
        if(Player.orderIndex>=Mod.songLength)
          Player.orderIndex=0;
        Player.row=0;
        jumpFlag=true;
        break;
      case SETVOLUME:
        if(effectParameter>64) Player.volume[channel]=64;
        else Player.volume[channel]=effectParameter;
        break;
      case BREAKPATTERNTOROW:
        Player.row=effectParameterX*10+effectParameterY;
        if(Player.row>=ROWS)
          Player.row=0;
        if(!jumpFlag&&!breakFlag){
          Player.orderIndex++;
          if(Player.orderIndex>=Mod.songLength)
            Player.orderIndex=0;
        }
        breakFlag=true;
        break;
      case ESUBSET:
        switch(effectParameterX){
          case FINEPORTAMENTOUP:
            Player.lastAmigaPeriod[channel]-=effectParameterY;
            break;
          case FINEPORTAMENTODOWN:
            Player.lastAmigaPeriod[channel]+=effectParameterY;
            break;
          case SETVIBRATOWAVEFORM:
            Player.waveControl[channel]&=0xF0;
            Player.waveControl[channel]|=effectParameterY;
            break;
          case SETFINETUNE:
            Mod.samples[Player.lastSampleNumber[channel]].fineTune=effectParameterY;
            if(Mod.samples[Player.lastSampleNumber[channel]].fineTune>7)
              Mod.samples[Player.lastSampleNumber[channel]].fineTune-=16;
            break;
          case PATTERNLOOP:
            if(effectParameterY){
              if(Player.patternLoopCount[channel])
                Player.patternLoopCount[channel]--;
              else
                Player.patternLoopCount[channel]=effectParameterY;
              if(Player.patternLoopCount[channel])
                Player.row=Player.patternLoopRow[channel];
            } else
              Player.patternLoopRow[channel]=Player.row+1;
            break;
          case SETTREMOLOWAVEFORM:
            Player.waveControl[channel]&=0xF;
            Player.waveControl[channel]|=effectParameterY<<4;
            break;
          case FINEVOLUMESLIDEUP:
            Player.volume[channel]+=effectParameterY;
            if(Player.volume[channel]>64) Player.volume[channel]=64;
            break;
          case FINEVOLUMESLIDEDOWN:
            Player.volume[channel]-=effectParameterY;
            if(Player.volume[channel]<0) Player.volume[channel]=0;
            break;
          case NOTECUT:
            note=NONOTE;
            break;
          case NOTEDELAY:
            break;
          case PATTERNDELAY:
            Player.patternDelay=effectParameterY;
            break;
          case INVERTLOOP:
            break;
        }
        break;
      case SETSPEED:
        if(effectParameter<0x20){
          Player.speed=effectParameter;
        }else{
          bpmOriginal=effectParameter;
          Player.samplesPerTick=sampleRate/(2*effectParameter/5);
        }
        break;
    }
    if(note!=NONOTE||(Player.lastAmigaPeriod[channel]&&
        effectNumber!=VIBRATO&&effectNumber!=VIBRATOVOLUMESLIDE&&
        !(effectNumber==0xE&&effectParameterX==NOTEDELAY)))
      Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
    if(note!=NONOTE)
      Mixer.channelSampleOffset[channel]=sampleOffset<<FIXED_DIVIDER;
    if(sampleNumber)
      Mixer.channelSampleNumber[channel]=Player.lastSampleNumber[channel];
    if(effectNumber!=TREMOLO)
      Mixer.channelVolume[channel]=Player.volume[channel];
    switch(channel%4){
      case 0:
      case 3:
        Mixer.channelPanning[channel]=stereoSeparation;
        break;
      default:
        Mixer.channelPanning[channel]=128-stereoSeparation;
    }
    // Calculate which element of the channelEQBuffer this channel should contribute to
    // and properly average values when multiple channels map to the same element
    if(buffersInitialized){
      int8_t vol=Mixer.channelVolume[channel];
      int noteIndex=getNoteIndex(Player.amigaPeriod[channel]);
      if(vol>64) vol=64;
      // Map channel to one of the 8 elements in channelEQBuffer
      uint8_t targetIndex=channel%8;
      // Keep track of how many channels have contributed to each buffer element
      static uint8_t channelCount[8]={0};
      // Reset counts at the start of each row processing
      if(channel==0){
        memset(channelCount,0,sizeof(channelCount));
      }
      // Update the buffer with a running average
      channelCount[targetIndex]++;
      // Calculate running average: newAvg = ((oldAvg * (n-1)) + newValue) / n
      if(channelCount[targetIndex]==1){
        // First value for this element
        channelEQBuffer[targetIndex]=vol;
      }else{
        // Average with existing values
        channelEQBuffer[targetIndex]=((channelEQBuffer[targetIndex]*(channelCount[targetIndex]-1))+vol)/channelCount[targetIndex];
      }
      // Process note volume for the equalizer
      if(noteIndex>=0&&noteIndex<=95){
        volSum[noteIndex]+=vol;
        noteCount[noteIndex]++;
      }
    }
  }
  if(buffersInitialized){
    // Calculate the average volume value for each note
    for(int i=0;i<96;++i){
      if(noteCount[i]>0){
        eqBuffer[i]=(volSum[i]/noteCount[i])>>2;
      }else{
        eqBuffer[i]=0; // Or another default value
      }
    }
  }
  return true;
}

bool AudioGeneratorMOD::ProcessTick(){
  uint8_t channel;
  uint8_t sampleNumber;
  uint16_t note;
  uint8_t effectNumber;
  uint8_t effectParameter;
  uint8_t effectParameterX;
  uint8_t effectParameterY;
  uint16_t tempNote;
  if(!running) return false;
  for(channel=0;channel<Mod.numberOfChannels;channel++){
    if(Player.lastAmigaPeriod[channel]){
      sampleNumber=Player.currentPattern.sampleNumber[Player.lastRow][channel];
      note=NOTE(Player.lastRow,channel);
      effectNumber=Player.currentPattern.effectNumber[Player.lastRow][channel];
      effectParameter=Player.currentPattern.effectParameter[Player.lastRow][channel];
      effectParameterX=effectParameter>>4;
      effectParameterY=effectParameter&0xF;
      switch(effectNumber){
        case ARPEGGIO:
          if(effectParameter)
            switch(Player.tick%3){
              case 0:
                Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
                break;
              case 1:
                tempNote=Player.lastNote[channel]+effectParameterX*8+Mod.samples[Player.lastSampleNumber[channel]].fineTune;
                if(tempNote<296) Mixer.channelFrequency[channel]=Player.amiga/ReadAmigaPeriods(tempNote);
                break;
              case 2:
                tempNote=Player.lastNote[channel]+effectParameterY*8+Mod.samples[Player.lastSampleNumber[channel]].fineTune;
                if(tempNote<296) Mixer.channelFrequency[channel]=Player.amiga/ReadAmigaPeriods(tempNote);
                break;
            }
          break;
        case PORTAMENTOUP:
          Player.lastAmigaPeriod[channel]-=effectParameter;
          if(Player.lastAmigaPeriod[channel]<113) Player.lastAmigaPeriod[channel]=113;
          Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
          break;
        case PORTAMENTODOWN:
          Player.lastAmigaPeriod[channel]+=effectParameter;
          if(Player.lastAmigaPeriod[channel]>856) Player.lastAmigaPeriod[channel]=856;
          Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
          break;
        case TONEPORTAMENTO:
          Portamento(channel);
          break;
        case VIBRATO:
          Vibrato(channel);
          break;
        case PORTAMENTOVOLUMESLIDE:
          Portamento(channel);
          Player.volume[channel]+=effectParameterX-effectParameterY;
          if(Player.volume[channel]<0) Player.volume[channel]=0;
          else if(Player.volume[channel]>64) Player.volume[channel]=64;
          Mixer.channelVolume[channel]=Player.volume[channel];
          break;
        case VIBRATOVOLUMESLIDE:
          Vibrato(channel);
          Player.volume[channel]+=effectParameterX-effectParameterY;
          if(Player.volume[channel]<0) Player.volume[channel]=0;
          else if(Player.volume[channel]>64) Player.volume[channel]=64;
          Mixer.channelVolume[channel]=Player.volume[channel];
          break;
        case TREMOLO:
          Tremolo(channel);
          break;
        case VOLUMESLIDE:
          Player.volume[channel]+=effectParameterX-effectParameterY;
          if(Player.volume[channel]<0) Player.volume[channel]=0;
          else if(Player.volume[channel]>64) Player.volume[channel]=64;
          Mixer.channelVolume[channel]=Player.volume[channel];
          break;
        case ESUBSET:
          switch(effectParameterX){
            case RETRIGGERNOTE:
              if(!effectParameterY) break;
              if(!(Player.tick%effectParameterY)){
                Mixer.channelSampleOffset[channel]=0;
              }
              break;
            case NOTECUT:
              if(Player.tick==effectParameterY)
                Mixer.channelVolume[channel]=Player.volume[channel]=0;
              break;
            case NOTEDELAY:
              if(Player.tick==effectParameterY){
                if(sampleNumber) Player.volume[channel]=Mod.samples[Player.lastSampleNumber[channel]].volume;
                if(note!=NONOTE) Mixer.channelSampleOffset[channel]=0;
                Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
                Mixer.channelVolume[channel]=Player.volume[channel];
              }
              break;
          }
          break;
      }
    }
  }
  return true;
}

bool AudioGeneratorMOD::RunPlayer(){
  if(!running) return false;

  if(trackFrameInitialized){
    // Calculate how many 1/50th second frames should pass for this tick
    // One tick duration in seconds=Player.samplesPerTick/sampleRate
    // Number of 1/50th frames=(samplesPerTick/sampleRate)*50
    
    float secondsPerTick=(float)Player.samplesPerTick/(float)sampleRate;
    float trackFramesPerTick=secondsPerTick*50.0f;
    
    // Accumulate and round to nearest whole number
    static float frameAccumulator=0.0f;
    frameAccumulator+=trackFramesPerTick;
    
    if(frameAccumulator>=1.0f){
      int framesToAdd=(int)frameAccumulator;
      (*trackFrame)+=framesToAdd;
      frameAccumulator-=framesToAdd;
    }
  }

  if(Player.tick==Player.speed){
    Player.tick=0;

    if(Player.row==ROWS){
      Player.orderIndex++;
      if(Player.orderIndex==Mod.songLength)
      {
        //Player.orderIndex=0;
        // No loop,just say we're done!
        // printf("Song end\n");
        return false;
      }
      Player.row=0;
    }

    if(Player.patternDelay){
      Player.patternDelay--;
    }else{
      if(Player.orderIndex!=Player.oldOrderIndex)
        if(!LoadPattern(Mod.order[Player.orderIndex])) return false;
      Player.oldOrderIndex=Player.orderIndex;
      if(!ProcessRow()) return false;
    }

  }else{
    if(!ProcessTick()) return false;
  }

  Player.tick++;
  return true;
}

void AudioGeneratorMOD::GetSample(int16_t sample[2]){
  int32_t sumL;
  int32_t sumR;
  uint8_t channel;
  uint32_t samplePointer;
  int8_t current;
  int8_t next;
  int16_t out;
  int32_t out32;
  // Early exit check with combined conditions
	if(!running||!file||!output||stopping||bufferFreed||isPaused){
		sample[AudioOutput::LEFTCHANNEL]=sample[AudioOutput::RIGHTCHANNEL]=0;
		return;
	}
  sumL=0;
  sumR=0;

  for(channel=0;channel<Mod.numberOfChannels;channel++){

    if(!Mixer.channelFrequency[channel]||!Mod.samples[Mixer.channelSampleNumber[channel]].length) continue;

    Mixer.channelSampleOffset[channel]+=Mixer.channelFrequency[channel];

    if(!Mixer.channelVolume[channel]) continue;
    
    #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
    uint8_t sampleNum=Mixer.channelSampleNumber[channel];
    samplePointer=Mixer.sampleBegin[sampleNum]+(Mixer.channelSampleOffset[channel]>>FIXED_DIVIDER);
    // Checking the boundaries BEFORE processing the loop
    if(sampleNum>=SAMPLES||!Mod.samples[sampleNum].length){
      Mixer.channelFrequency[channel]=0;
      continue;
    }
    if(Mixer.sampleLoopLength[sampleNum]){
      if(samplePointer>=Mixer.sampleLoopEnd[sampleNum]){
        Mixer.channelSampleOffset[channel]-=Mixer.sampleLoopLength[sampleNum]<<FIXED_DIVIDER;
        samplePointer-=Mixer.sampleLoopLength[sampleNum];
      }
    }else{
      if(samplePointer>=Mixer.sampleEnd[sampleNum]){
        Mixer.channelFrequency[channel]=0;
        samplePointer=Mixer.sampleEnd[sampleNum]-1;
      }
    }
    // Final check after all calculations
    if(samplePointer>=Mixer.sampleEnd[sampleNum]){
      samplePointer=Mixer.sampleEnd[sampleNum]-1;
    }
    if(samplePointer<FatBuffer.samplePointer[channel]||samplePointer>=FatBuffer.samplePointer[channel]+fatBufferSize-1||sampleNum!=FatBuffer.channelSampleNumber[channel]){
      uint32_t toRead=Mixer.sampleEnd[sampleNum]-samplePointer+1;
      if(toRead>(uint32_t)fatBufferSize) toRead=fatBufferSize;
      if(!file->seek(samplePointer,SEEK_SET)) continue;
      if(toRead!=file->read(FatBuffer.channels[channel],toRead)) continue;
      FatBuffer.samplePointer[channel]=samplePointer;
      FatBuffer.channelSampleNumber[channel]=sampleNum;
    }
    uint32_t bufferOffset=samplePointer-FatBuffer.samplePointer[channel];
    current=FatBuffer.channels[channel][bufferOffset];
    next=(bufferOffset+1<fatBufferSize&&samplePointer+1<Mixer.sampleEnd[sampleNum])?FatBuffer.channels[channel][bufferOffset+1]:current;
    #else
    uint8_t sampleNum=Mixer.channelSampleNumber[channel];
    samplePointer=Mixer.channelSampleOffset[channel]>>FIXED_DIVIDER;
    // Checking the boundaries BEFORE processing the loop
    if(sampleNum>=SAMPLES||!Mod.samples[sampleNum].data||!Mod.samples[sampleNum].length){
      Mixer.channelFrequency[channel]=0;
      continue;
    }
    if(Mod.samples[sampleNum].loopLength>2){
      uint32_t loopEnd=Mod.samples[sampleNum].loopBegin+Mod.samples[sampleNum].loopLength;
      if(samplePointer>=loopEnd){
        Mixer.channelSampleOffset[channel]-=Mod.samples[sampleNum].loopLength<<FIXED_DIVIDER;
        samplePointer=Mixer.channelSampleOffset[channel]>>FIXED_DIVIDER;
      }
    }else{
      if(samplePointer>=Mod.samples[sampleNum].length){
        Mixer.channelFrequency[channel]=0;
        samplePointer=Mod.samples[sampleNum].length-1;
      }
    }
    // Final check after all calculations
    if(samplePointer>=Mod.samples[sampleNum].length){
      samplePointer=Mod.samples[sampleNum].length-1;
    }
    current=static_cast<int8_t>(Mod.samples[sampleNum].data[samplePointer]);
    next=(samplePointer+1< Mod.samples[sampleNum].length)?static_cast<int8_t>(Mod.samples[sampleNum].data[samplePointer+1]):current;
    #endif
	
    // preserve a few more bits from sample interpolation,by upscaling input values.
    // This does (slightly) reduce quantization noise in higher frequencies,typically above 8kHz.
    // Actually we could could even gain more bits,I was just not sure if more bits would cause overflows in other conputations.
    int16_t current16=(int16_t)current<<2;
    int16_t next16=(int16_t)next<<2;	  
    out=current16;
    // Integer linear interpolation-only works correctly in 16bit
    out+=(next16-current16)*(Mixer.channelSampleOffset[channel]&((1<<FIXED_DIVIDER)-1))>>FIXED_DIVIDER;
    // Upscale to BITDEPTH,considering the we already gained two bits in the previous step
    out32=(int32_t)out<<(BITDEPTH-10);
    // Channel volume
    out32=out32*Mixer.channelVolume[channel]>>6;
    // Channel panning
    sumL+=out32*min(128-Mixer.channelPanning[channel],64)>>6;
    sumR+=out32*min(Mixer.channelPanning[channel],64)>>6;
  }
  // Downscale to BITDEPTH-a bit faster because the compiler can replaced division by constants with proper "right shift"+correct handling of sign bit
  if(Mod.numberOfChannels<=4){
    // up to 4 channels
    sumL/=4;
    sumR/=4;
  }else{
    if(Mod.numberOfChannels<=6){
      // 5 or 6 channels-pre-multiply be 1.5,then divide by 8 -> same as division by 6
      sumL=(sumL+(sumL/2))/8;
      sumR=(sumR+(sumR/2))/8;      
    }else{
      // 7,8,or more channels
      sumL/=8;
      sumR/=8;
    }
  }
  // clip samples to 16bit (with saturation in case of overflow)
  if(sumL<=INT16_MIN) sumL=INT16_MIN;
  else if(sumL>=INT16_MAX) sumL=INT16_MAX;
  if(sumR<=INT16_MIN) sumR=INT16_MIN;
  else if(sumR>=INT16_MAX) sumR=INT16_MAX;
  // Fill the sound buffer with signed values
  sample[AudioOutput::LEFTCHANNEL]=sumL;
  sample[AudioOutput::RIGHTCHANNEL]=sumR;
}

bool AudioGeneratorMOD::LoadMOD(){
  uint8_t channel;
  if(!LoadHeader()) return false;
  LoadSamples();
  Player.amiga=AMIGA;
  Player.samplesPerTick=sampleRate/(2*125/5); // Hz=2*BPM/5
  Player.speed=6;
  Player.tick=Player.speed;
  bpmOriginal=125;
  Player.row=0;
  Player.orderIndex=0;
  Player.oldOrderIndex=0xFF;
  Player.patternDelay=0;
  for(channel=0;channel<Mod.numberOfChannels;channel++){
    Player.patternLoopCount[channel]=0;
    Player.patternLoopRow[channel]=0;
    Player.lastAmigaPeriod[channel]=0;
    Player.waveControl[channel]=0;
    Player.vibratoSpeed[channel]=0;
    Player.vibratoDepth[channel]=0;
    Player.vibratoPos[channel]=0;
    Player.tremoloSpeed[channel]=0;
    Player.tremoloDepth[channel]=0;
    Player.tremoloPos[channel]=0;
    #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
    FatBuffer.samplePointer[channel]=0;
    FatBuffer.channelSampleNumber[channel]=0xFF;
    #endif
    Mixer.channelSampleOffset[channel]=0;
    Mixer.channelFrequency[channel]=0;
    Mixer.channelVolume[channel]=0;
    switch(channel%4){
      case 0:
      case 3:
        Mixer.channelPanning[channel]=stereoSeparation;
        break;
      default:
        Mixer.channelPanning[channel]=128-stereoSeparation;
    }
  }
  return true;
}

// Aditional protected
void AudioGeneratorMOD::removeExtraSpaces(char* str){
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

int AudioGeneratorMOD::getNoteIndex(uint16_t value){
  // Period 1
  if(value<=856&&value>=431){
    for(int i=0;i<96;i++){
      if(zxPeriods[0][i]==value){
        return i;
      }
    }
  }
  // Period 2
  else if(value<=428&&value>=216){
    for(int i=0;i<96;i++){
      if(zxPeriods[1][i]==value){
        return i;
      }
    }
  }
  // Period 3
  else if(value<=214&&value>=108){
    for(int i=0;i<96;i++){
      if(zxPeriods[2][i]==value){
        return i;
      }
    }
  }
  return -1;
}

float AudioGeneratorMOD::calcRow(){
  Calc.patternBreak=false;
  Calc.patternJump=false;
  Calc.patternDelay=0;
  file->seek(Calc.patternsOffset+(Calc.patternNumber*ROWS*4*Calc.channels)+(Calc.row*4*Calc.channels),SEEK_SET);
  for(int channel=0;channel<Calc.channels;channel++){
    uint8_t noteData[4];
    file->read(noteData,sizeof(noteData));
    uint8_t effect=noteData[2]&0x0F;
    uint8_t effectParamXY=noteData[3];
    uint8_t effectParamX=effectParamXY>>4;
    uint8_t effectParamY=effectParamXY&0xF;
    switch(effect){
      case JUMPTOORDER: // Position Jump
        // printf("Jump to order detected!, orderIdx: %d, order: %d,row: %d,channel: %d,nextOrder: %d,prevOrder: %d\n",Calc.orderIndex,Calc.orderTable[Calc.orderIndex],Calc.row,channel,Calc.orderTable[effectParamXY],Calc.orderTable[Calc.prevOrder]);
        if(Calc.orderIndex==Calc.songLength-1) break; // break to prevent song loop if jump to order if last order
        Calc.nextOrder=effectParamXY;
        if(Calc.orderIndex>=Calc.songLength)
          Calc.nextOrder=0;
        Calc.nextRow=0;
        Calc.patternJump=true;
        break;
      case BREAKPATTERNTOROW: // Pattern Break
        // printf("Pattern break detected-order: %d,row: %d,channel: %d,nextRow: %d\n",Calc.orderIndex,Calc.row,channel,effectParamX*10+effectParamY);
        Calc.nextRow=(effectParamX*10+effectParamY>=64)?0:effectParamX*10+effectParamY;
        if(!Calc.patternJump&&!Calc.patternBreak){
          Calc.nextOrder=Calc.orderIndex+1;
        }
        Calc.patternBreak=true;
        if(Calc.orderIndex==Calc.songLength-1&&Calc.nextOrder==Calc.orderIndex&&Calc.nextRow<=Calc.row){Calc.songEnd=true; break;}
        if(Calc.nextOrder>=Calc.songLength){Calc.songEnd=true; break;}
        break;
      case ESUBSET: // Extended Effects
        switch(effectParamX){
          case PATTERNLOOP: // Pattern Loop
            // printf("Pattern loop detected-order: %d,row: %d,channel: %d,paramY: %d\n",Calc.orderIndex,Calc.row,channel,effectParamY);
            if(effectParamY){
              if(Calc.patternLoopRow[channel]>0&&(Calc.row==62||Calc.row==63||Calc.row==57||Calc.row==58)&&effectParamY==1/*&&Calc.orderIndex==Calc.songLength-1*/){Calc.patternLoopRow[channel]=0; break;}
              if(Calc.patternLoopCount[channel])
                Calc.patternLoopCount[channel]--;
              else
                Calc.patternLoopCount[channel]=effectParamY;
              if(Calc.patternLoopCount[channel]){
                Calc.nextRow=Calc.patternLoopRow[channel];
                Calc.nextOrder=Calc.orderIndex;
              }
            } else
              Calc.patternLoopRow[channel]=Calc.row;
            break;
          case NOTEDELAY: // Note Delay
            break;
          case PATTERNDELAY: // Pattern Delay
            Calc.patternDelay=effectParamY;
            break;
        }
        break;
      case SETSPEED: // Set Speed/BPM
        if(effectParamXY<32){
          Calc.currentSpeed=effectParamXY;
        }else{
          Calc.currentBPM=effectParamXY;
        }
        break;
    }
  }
  // Calculate time for this row
  float rowTime=(Calc.currentSpeed*0.02f)*(125.0f/Calc.currentBPM);
  float totalRowTime=rowTime+(Calc.patternDelay*((Calc.currentSpeed*0.02f)*(125.0f/Calc.currentBPM)));
  return totalRowTime;
}

void AudioGeneratorMOD::freeFatBuffer(){
	static bool inCleanup=false;
	if(inCleanup) return;
	inCleanup=true;
  #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
	if(FatBuffer.channels){
		for(int i=0;i<CHANNELS;i++){
			if(FatBuffer.channels[i]){	// Check if pointer is not NULL
				free(FatBuffer.channels[i]);
				FatBuffer.channels[i]=nullptr;	// Always NULL after free
			}
		}
	}
	memset(&FatBuffer,0,sizeof(FatBuffer));
  #endif
  #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
  uint8_t smpls=(oldFormat)?15:SAMPLES;
  for(uint8_t i=0;i<smpls;i++){
    if(Mod.samples[i].isAllocated&&Mod.samples[i].data!=nullptr){
      free(Mod.samples[i].data);
      Mod.samples[i].data=nullptr;
      Mod.samples[i].isAllocated=false;
    }
  }
  // printf("Free PSRAM: %d bytes after free buffer\n",ESP.getFreePsram());
  #endif
  bufferFreed=true;
	inCleanup=false;
}

// Aditional public
void AudioGeneratorMOD::getTitle(char* lfn,size_t maxLen){
  if(!file){
    lfn[0]='\0'; // If the file is not open,return an empty string
    return;
  }
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
  // Move the pointer to the beginning of the file
  file->seek(0,SEEK_SET);
  // Read first 20 bytes or less if maxLen is less than 20
  size_t readLen=(maxLen<20) ? maxLen : 20;
  file->read((uint8_t*)lfn,readLen);
  // Add a trailing zero
  if(readLen<maxLen){
    lfn[readLen]='\0';
  }else{
    lfn[maxLen-1]='\0';
  }
  // Return the pointer to its original position
  file->seek(currentPos,SEEK_SET);
}

void AudioGeneratorMOD::getDescription(char* description,size_t maxLen){
  if(!file){
    description[0]='\0'; // If the file is not open,return an empty string
    return;
  }
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
  // Buffer for storing sample names
  char sampleDescription[31]={0}; // 30 bytes+1 for terminating zero
  size_t offset=20; // Start position for first sample
  size_t sampleCount=6; // Number of samples to describe
  // Clear description line
  description[0]='\0';
  for(size_t i=0; i<sampleCount; ++i){
    // Move the pointer to the sample position
    file->seek(offset,SEEK_SET);
    // Read the sample name
    file->read((uint8_t*)sampleDescription,30);
    sampleDescription[30]='\0'; // Terminate the string with a null character
    // Remove extra spaces in sample name
    removeExtraSpaces(sampleDescription);
    // Добавить название семпла к описанию
    strncat(description,sampleDescription,maxLen-strlen(description)-1);
    // Add space between sample names
    if(i<sampleCount-1){
      strncat(description," ",maxLen-strlen(description)-1);
    }
    // Update offset for next sample
    offset+=30;
  }
  // Remove extra spaces in the final description line
  removeExtraSpaces(description);
  // Return the pointer to its original position
  file->seek(currentPos,SEEK_SET);
}

signed long AudioGeneratorMOD::getPlaybackTime(bool oneFiftieth){
  if(!file) return -1;
  Calc.oldMod=false;
  Calc.channels=getNumberOfChannels();
  Calc.patternsOffset=(Calc.oldMod)?600:1084;
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_END);
  uint32_t fileSize=file->getPos();
  file->seek(0,SEEK_SET);
  uint8_t header[Calc.patternsOffset];
  file->read(header,sizeof(header));
  Calc.songLength=header[(Calc.oldMod)?470:950];
  Calc.songRestart=header[(Calc.oldMod)?471:951];
  Calc.totalSeconds=0.0f;
  Calc.currentBPM=125.0f;
  Calc.currentSpeed=6;
  Calc.patternBreak=false;
  Calc.patternJump=false;
  Calc.songEnd=false;
  Calc.prevOrder=0;
  Calc.nextOrder=-1;
  Calc.nextRow=-1;
  memset(Calc.patternLoopCount,0,sizeof(Calc.patternLoopCount));
  memset(Calc.patternLoopRow,0,sizeof(Calc.patternLoopRow));
  // Get order table
  uint32_t tableOrderOffset=(Calc.oldMod)?472:952;
  uint32_t beforePos=file->getPos();
  file->seek(tableOrderOffset,SEEK_SET);
  if(128!=file->read(Calc.orderTable,128)) return false;
  file->seek(beforePos,SEEK_SET);
  // Processing patterns
  Calc.songLength=(Calc.songLength>128)?128:Calc.songLength;
  for(Calc.orderIndex=0;Calc.orderIndex<Calc.songLength;Calc.orderIndex++){
    if(Calc.nextOrder!=-1){
      Calc.orderIndex=Calc.nextOrder;
      Calc.nextOrder=-1;
    }
    Calc.patternNumber=header[((Calc.oldMod)?472:952)+Calc.orderIndex];
    for(Calc.row=0;Calc.row<ROWS;Calc.row++){
      if(Calc.nextOrder!=-1) break;
      if(Calc.nextRow!=-1){
        Calc.row=Calc.nextRow;
        Calc.nextRow=-1;
      }
      Calc.totalSeconds+=calcRow();
      if(Calc.orderIndex==Calc.songLength-1&&Calc.nextRow!=-1&&Calc.nextOrder!=-1){ // if pattern loop and last pattern
        Calc.orderIndex--;
        Calc.orderIndex=(Calc.orderIndex==255)?0:Calc.orderIndex;
        Calc.nextOrder=-1;
        break;
      }
      if(Calc.nextOrder>Calc.songLength||Calc.songEnd) break;
    }
    if(Calc.patternJump&&!Calc.patternBreak&&Calc.orderTable[Calc.nextOrder]!=Calc.orderTable[Calc.orderIndex+1]&&Calc.nextOrder<=Calc.songLength){
      if(Calc.nextOrder<=Calc.orderIndex&&Calc.orderIndex+1<Calc.songLength&&Calc.nextOrder!=-1) Calc.nextOrder=Calc.orderIndex+1;
      else break;
    }
    Calc.prevOrder=Calc.orderIndex;
    if(Calc.nextOrder>Calc.songLength||Calc.songEnd) break;
  }
  file->seek(currentPos,SEEK_SET);
  signed long factor=(oneFiftieth)?50:1000;
  return static_cast<signed long>(Calc.totalSeconds*factor);
}

void AudioGeneratorMOD::setSpeed(uint8_t speed){
  switch(speed){
    case 0:
      sampleRate=samplerateOriginal*2;
      break;
    case 1:
      sampleRate=samplerateOriginal;
      break;
    case 2:
      sampleRate=samplerateOriginal/2;
      break;
    default:
      sampleRate=samplerateOriginal;
      break;
  }
  Player.samplesPerTick=sampleRate/(2*bpmOriginal/5); // Hz=2*BPM/5// Player.samplesPerTick=sampleRate/(2*125/5); // Hz=2*BPM/5
}

uint8_t AudioGeneratorMOD::getNumberOfChannels(){
  if(!file) return 0;
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
  file->seek(1080,SEEK_SET);
  // Buffer for storing identifier
  uint8_t temp[4];
  if(4!=file->read(temp,4)){
    file->seek(currentPos,SEEK_SET);
    return 0; // If reading failed,return 0
  }
  // Determining the number of channels based on the identifier
  uint8_t numberOfChannels=4; // Default 4 channels
  if(!strncmp(reinterpret_cast<const char*>(temp),"2CHN",4)){
    numberOfChannels=2;
  }else if(!strncmp(reinterpret_cast<const char*>(temp),"M.K.",4)||
          !strncmp(reinterpret_cast<const char*>(temp),"M!K!",4)||
          !strncmp(reinterpret_cast<const char*>(temp),"4CHN",4)||
          !strncmp(reinterpret_cast<const char*>(temp),"FLT4",4)){
    numberOfChannels=4;
  }else if(!strncmp(reinterpret_cast<const char*>(temp),"6CHN",4)){
    numberOfChannels=6;
  }else if(!strncmp(reinterpret_cast<const char*>(temp),"8CHN",4)||
          !strncmp(reinterpret_cast<const char*>(temp),"OKTA",4)||
          !strncmp(reinterpret_cast<const char*>(temp),"FLT8",4)||
          !strncmp(reinterpret_cast<const char*>(temp),"CD81",4)){
    numberOfChannels=8;
  }else if(!strncmp(reinterpret_cast<const char*>(temp+2),"CH",2)){
    // First verify that temp[0] and temp[1] are valid digits
    if(temp[0]>='0'&&temp[0]<='9'&&temp[1]>='0'&&temp[1]<='9'){
      uint8_t tens=temp[0]-'0';
      uint8_t ones=temp[1]-'0';
      numberOfChannels=tens*10+ones;
    }else{
      numberOfChannels=0;  // Invalid digits found
    }
  }else{
    // Check for old format with 15 samples
    numberOfChannels=4; // Default to 4 channels if unknown
    file->seek(1080,SEEK_SET); // Seek to offset 1080
    if(4!=file->read(temp,4)) return false;
    if(strncmp(reinterpret_cast<const char*>(temp),"2CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"M.K.",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"M!K!",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"4CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"FLT4",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"6CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"8CHN",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"OKTA",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"FLT8",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp),"CD81",4)!=0&&
      strncmp(reinterpret_cast<const char*>(temp+2),"CH",2)!=0){
      // No known signature found,assume old format
      Calc.oldMod=true;
    }
  }
  // Return the pointer to its original position
  file->seek(currentPos,SEEK_SET);
  return numberOfChannels;
}

void AudioGeneratorMOD::initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer){
  this->eqBuffer=eqBuffer;
  this->channelEQBuffer=channelEQBuffer;
  // Clear buffers
  memset(eqBuffer,0,sizeof(uint8_t));
  memset(channelEQBuffer,0,sizeof(uint8_t));
  buffersInitialized=true; // Set the initialization flag
}

void AudioGeneratorMOD::initTrackFrame(unsigned long* tF){
  if(tF!=nullptr){
    trackFrame=tF;
    *trackFrame=0;  // Initialize the value
    trackFrameInitialized=true;
  }
}

void AudioGeneratorMOD::SetSeparation(int sep){
  stereoSeparation=sep;
  for(int ch=0; ch<Mod.numberOfChannels; ch++){
    switch(ch%4){
      case 0:
      case 3:
        Mixer.channelPanning[ch]=stereoSeparation;
        break;
      default:
        Mixer.channelPanning[ch]=128-stereoSeparation;
    }
  }
}
