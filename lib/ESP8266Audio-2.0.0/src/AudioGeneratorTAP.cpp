/*
  AudioGeneratorTAP-ZX Spectrum TAP file to audio converter
  Converts TAP cassette images to audio waveform for DAC playback

  Copyright (C) 2025 by Spawn
*/

#include "AudioGeneratorTAP.h"

AudioGeneratorTAP::AudioGeneratorTAP(){
  sampleRate=44100;
  level=0;
  state=STATE_READ_BLOCK;
  pulseRemain=0;
  currentSample=0;
  baseSample=0;
  sampleAccumulator=0;
  headerParsed=false;
  memset(programName,0,sizeof(programName));
  eqBuffer=nullptr;
  channelEQBuffer=nullptr;
  lastSample[0]=0;
  lastSample[1]=0;
  output=nullptr;
  file=nullptr;
  running=false;
  totalBlocks=0;
  currentBlock=0;
}

AudioGeneratorTAP::~AudioGeneratorTAP(){
}

bool AudioGeneratorTAP::initializeFile(AudioFileSource* source,const char** message){
  if(!source) return false;
  file=source;
  if(!file->isOpen()) return false;
  
  uint32_t fileSize=file->getSize();
  if(fileSize<2){
    if(message) *message="Invalid TAP file!";
    return false;
  }
  
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  
  uint8_t lenBuf[2];
  if(file->read(lenBuf,2)!=2){
    if(message) *message="Invalid TAP file!";
    file->seek(currentPos,SEEK_SET);
    return false;
  }
  
  uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
  if(blockLen==0||blockLen>65535){
    if(message) *message="Invalid TAP file!";
    file->seek(currentPos,SEEK_SET);
    return false;
  }
  
  file->seek(currentPos,SEEK_SET);
  return true;
}

bool AudioGeneratorTAP::begin(AudioFileSource *source,AudioOutput *out){
  uint8_t savedCurrentBlock=currentBlock;
  bool savedBlockAlreadyRead=blockAlreadyRead;
  State savedState=state;
  uint32_t savedBaseSample=baseSample;
  
  if(running) stop();

  if(!file){
    if(!initializeFile(source)) return false;
  }
  if(!out) return false;
  output=out;
  
  if(!file->isOpen()) return false;// Can't read the file!

  // Set the output values properly
  if(!output->SetRate(sampleRate)) return false;
  if(!output->SetBitsPerSample(16)) return false;
  if(!output->SetChannels(2)) return false;
  if(!output->begin()) return false;
  
  level=32767; // Start with HIGH level
  running=true;
  stopping=false;
  headerParsed=false;
  
  if(savedCurrentBlock>0||savedBlockAlreadyRead){
    currentBlock=savedCurrentBlock;
    blockAlreadyRead=savedBlockAlreadyRead;
    state=savedState;
    baseSample=savedBaseSample;
    currentSample=baseSample;
  }else{
    state=STATE_READ_BLOCK;
    currentSample=0;
    baseSample=0;
    sampleAccumulator=0;
    blockAlreadyRead=false;
  }
  memset(programName,0,sizeof(programName));
  // DON'T reset eqBuffer and channelEQBuffer-they are set by initEQBuffers() before begin()
  return true;
}

bool AudioGeneratorTAP::stop(){
  if(!running) return true;
  stopping=true;
  vTaskDelay(pdMS_TO_TICKS(10));
  if(output) output->flush();
  if(file&&file->isOpen()) file->close();
  output->stop();
  running=false;
  return true;
}

bool AudioGeneratorTAP::isRunning(){
  return running;
}

void AudioGeneratorTAP::generatePulse(uint16_t durationUs){
  pulseDuration=durationUs;
  uint32_t samples=(sampleRate*durationUs)/(1000000*speedMultiplier);
  pulseRemain=(samples>0)?samples:1;
}

bool AudioGeneratorTAP::readNextBlock(){
  uint8_t lenBuf[2];
  if(file->read(lenBuf,2)!=2) return false;
  blockLen=lenBuf[0]|(lenBuf[1]<<8);
  if(blockLen==0) return false;
  bytesRemain=blockLen;
  uint8_t flag;
  if(file->read(&flag,1)!=1) return false;
  // Parse header block for program name
  if(!headerParsed&&flag==0x00&&blockLen==19){
    uint8_t headerBuf[18];
    if(file->read(headerBuf,18)==18){
      // Bytes 1-10 contain filename (after type byte)
      memcpy(programName,&headerBuf[1],10);
      programName[10]='\0';
      // Trim spaces
      for(int i=9;i>=0;i--){
        if(programName[i]==' ') programName[i]='\0';
        else break;
      }
      headerParsed=true;
    }
    file->seek(file->getPos()-18,SEEK_SET);
  }
  // bytesRemain=all data bytes including flag
  bytesRemain=blockLen;
  pilotCount=(flag==0)?TAP_PILOT_HEADER:TAP_PILOT_DATA;
  pilotRemain=pilotCount;
  // Seek back to re-read flag as first data byte
  file->seek(file->getPos()-1,SEEK_SET);
  // No lead-in pause-start directly with pilot
  state=STATE_PILOT;
  return true;
}

signed long AudioGeneratorTAP::getPlaybackTime(bool oneFiftieth){
  if(!file) return -1;
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint64_t sampleCount=0;
  uint8_t lenBuf[2];
  while(file->read(lenBuf,2)==2){
    uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
    if(blockLen==0) break;
    uint8_t flag;
    if(file->read(&flag,1)!=1) break;
    sampleCount++;
    uint16_t pilotCount=(flag==0x00)?TAP_PILOT_HEADER:TAP_PILOT_DATA;
    uint32_t pilotPulseSamples=(sampleRate*TAP_PILOT_PULSE)/1000000;
    if(pilotPulseSamples==0) pilotPulseSamples=1;
    sampleCount+=(uint64_t)pilotCount*pilotPulseSamples+pilotCount;
    uint32_t sync1Samples=(sampleRate*TAP_SYNC1_PULSE)/1000000;
    if(sync1Samples==0) sync1Samples=1;
    sampleCount+=sync1Samples+1;
    uint32_t sync2Samples=(sampleRate*TAP_SYNC2_PULSE)/1000000;
    if(sync2Samples==0) sync2Samples=1;
    sampleCount+=sync2Samples+1;
    file->seek(file->getPos()-1,SEEK_SET);
    uint32_t zeroPulseSamples=(sampleRate*TAP_ZERO_PULSE)/1000000;
    uint32_t onePulseSamples=(sampleRate*TAP_ONE_PULSE)/1000000;
    if(zeroPulseSamples==0) zeroPulseSamples=1;
    if(onePulseSamples==0) onePulseSamples=1;
    for(uint16_t i=0;i<blockLen;i++){
      uint8_t byte;
      if(file->read(&byte,1)!=1) break;
      for(uint8_t bit=0;bit<8;bit++){
        uint32_t pulseSamples=(byte&0x80)?onePulseSamples:zeroPulseSamples;
        sampleCount+=pulseSamples*2+2;
        byte<<=1;
      }
    }
    sampleCount++;
    uint32_t pauseSamples=(sampleRate*TAP_PAUSE_MS)/1000;
    sampleCount+=pauseSamples+1;
  }
  file->seek(currentPos,SEEK_SET);
  return oneFiftieth?(sampleCount*50)/sampleRate:(sampleCount*1000)/sampleRate;
}

void AudioGeneratorTAP::getTitle(char* title,size_t maxLen){
  if(!file||maxLen==0){
    if(maxLen>0) title[0]='\0';
    return;
  }
  // If we already parsed the header, use program name
  if(headerParsed&&programName[0]!='\0'){
    strncpy(title, programName, maxLen-1);
    title[maxLen-1]='\0';
    return;
  }
  // Otherwise try to parse first block
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint8_t lenBuf[2];
  if(file->read(lenBuf,2)==2){
    uint16_t len=lenBuf[0]|(lenBuf[1]<<8);
    uint8_t flag;
    if(file->read(&flag,1)==1&&flag==0x00&&len==19){
      uint8_t headerBuf[18];
      if(file->read(headerBuf,18)==18){
        // Extract filename (bytes 1-10 after type)
        char name[11];
        memcpy(name,&headerBuf[1],10);
        name[10]='\0';
        // Trim spaces
        for(int i=9;i>=0;i--){
          if(name[i]==' ') name[i]='\0';
          else break;
        }
        strncpy(title,name,maxLen-1);
        title[maxLen-1]='\0';
        file->seek(currentPos,SEEK_SET);
        return;
      }
    }
  }
  file->seek(currentPos,SEEK_SET);
  title[0]='\0';
}

void AudioGeneratorTAP::getDescription(char* description,size_t maxLen){
  if(!file||maxLen==0){
    if(maxLen>0) description[0]='\0';
    return;
  }
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  description[0]='\0';
  uint8_t lenBuf[2];
  uint8_t blockData[512];
  // Search first 10 blocks for text
  for(int blockNum=0;blockNum<10;blockNum++){
    if(file->read(lenBuf,2)!=2) break;
    uint16_t len=lenBuf[0]|(lenBuf[1]<<8);
    if(len==0||len>16384) break;
    uint8_t flag;
    if(file->read(&flag,1)!=1) break;
    // Look in any block with reasonable size
    if(len>20&&len<=512){
      if(file->read(blockData, len-1)==len-1){
        int textStart=-1,textLen=0,maxTextLen=0,bestStart=-1;
        for(int i=0;i<len-1;i++){
          if(blockData[i]>=32&&blockData[i]<=126){
            if(textStart==-1) textStart=i;
            textLen++;
          }else{
            if(textLen>maxTextLen&&textLen>=15){
              maxTextLen=textLen;
              bestStart=textStart;
            }
            textStart=-1;
            textLen=0;
          }
        }
        if(textLen>maxTextLen&&textLen>=15){
          maxTextLen=textLen;
          bestStart=textStart;
        }
        if(bestStart>=0&&maxTextLen>0){
          int copyLen=(maxTextLen<maxLen-1)?maxTextLen:maxLen-1;
          memcpy(description, &blockData[bestStart],copyLen);
          description[copyLen]='\0';
          // Remove extra spaces
          char* src=description;
          char* dst=description;
          bool prevSpace=false;
          while(*src){
            if(*src==' '){
              if(!prevSpace) *dst++=*src;
              prevSpace=true;
            }else{
              *dst++=*src;
              prevSpace=false;
            }
            src++;
          }
          *dst='\0';
          while(dst>description&&*(dst-1)==' ') *(--dst)='\0';
          file->seek(currentPos,SEEK_SET);
          return;
        }
      }
    }else{
      file->seek(file->getPos()+len-1,SEEK_SET);
    }
  }
  file->seek(currentPos,SEEK_SET);
}

void AudioGeneratorTAP::initEQBuffers(uint8_t* eqBuf,uint8_t* channelEQBuf){
  eqBuffer=eqBuf;
  channelEQBuffer=channelEQBuf;
  if(eqBuffer) memset(eqBuffer,0,96);
  if(channelEQBuffer) memset(channelEQBuffer,0,8);
}

uint8_t AudioGeneratorTAP::getTotalBlocks(){
  if(!file) return 0;
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint8_t count=0;
  uint8_t lenBuf[2];
  while(file->read(lenBuf,2)==2&&count<255){
    uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
    if(blockLen==0) break;
    file->seek(file->getPos()+blockLen,SEEK_SET);
    count++;
  }
  file->seek(currentPos,SEEK_SET);
  totalBlocks=count;
  return count;
}

void AudioGeneratorTAP::setCurrentBlock(uint8_t block){
  if(!file||block>=totalBlocks) return;
  currentBlock=block;
  baseSample=getBlockStartSample(block);
  if(trackFrame){
    *trackFrame=(baseSample*50)/sampleRate; // start block time
    sampleAccumulator=0;
  }
  file->seek(0,SEEK_SET);
  for(uint8_t i=0;i<block;i++){
    uint8_t lenBuf[2];
    if(file->read(lenBuf,2)!=2) return;
    uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
    if(blockLen==0) return;
    file->seek(file->getPos()+blockLen,SEEK_SET);
  }
  uint8_t lenBuf2[2];
  if(file->read(lenBuf2,2)==2){
    blockLen=lenBuf2[0]|(lenBuf2[1]<<8);
    if(blockLen>0){
      uint8_t flag;
      if(file->read(&flag,1)==1){
        bytesRemain=blockLen;
        pilotCount=(flag==0)?TAP_PILOT_HEADER:TAP_PILOT_DATA;
        pilotRemain=pilotCount;
        file->seek(file->getPos()-1,SEEK_SET);
        state=STATE_PILOT;
        blockAlreadyRead=true;
        return;
      }
    }
  }
  state=STATE_READ_BLOCK;
  blockAlreadyRead=false;
}

uint32_t AudioGeneratorTAP::getBlockStartSample(uint8_t block){
  if(!file||block>=totalBlocks) return 0;
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint64_t sampleCount=0;
  uint8_t lenBuf[2];
  for(uint8_t i=0;i<block;i++){
    if(file->read(lenBuf,2)!=2){
      file->seek(currentPos,SEEK_SET);
      return 0;
    }
    uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
    if(blockLen==0){
      file->seek(currentPos,SEEK_SET);
      return 0;
    }
    uint8_t flag;
    if(file->read(&flag, 1)!=1){
      file->seek(currentPos,SEEK_SET);
      return 0;
    }
    sampleCount++;
    uint16_t pilotCount=(flag==0x00)?TAP_PILOT_HEADER:TAP_PILOT_DATA;
    uint32_t pilotPulseSamples=(sampleRate*TAP_PILOT_PULSE)/1000000;
    if(pilotPulseSamples==0) pilotPulseSamples=1;
    sampleCount+=(uint64_t)pilotCount*pilotPulseSamples+pilotCount;
    uint32_t sync1Samples=(sampleRate*TAP_SYNC1_PULSE)/1000000;
    if(sync1Samples==0) sync1Samples=1;
    sampleCount+=sync1Samples+1;
    uint32_t sync2Samples=(sampleRate*TAP_SYNC2_PULSE)/1000000;
    if(sync2Samples==0) sync2Samples=1;
    sampleCount+=sync2Samples+1;
    uint32_t zeroPulseSamples=(sampleRate*TAP_ZERO_PULSE)/1000000;
    uint32_t onePulseSamples=(sampleRate*TAP_ONE_PULSE)/1000000;
    if(zeroPulseSamples==0) zeroPulseSamples=1;
    if(onePulseSamples==0) onePulseSamples=1;
    for(uint8_t bit=0;bit<8;bit++){
      uint32_t pulseSamples=(flag&0x80)?onePulseSamples:zeroPulseSamples;
      sampleCount+=pulseSamples*2+2;
      flag<<=1;
    }
    for(uint16_t j=1;j<blockLen;j++){
      uint8_t byte;
      if(file->read(&byte,1)!=1){
        file->seek(currentPos,SEEK_SET);
        return 0;
      }
      for(uint8_t bit=0;bit<8;bit++){
        uint32_t pulseSamples=(byte&0x80)?onePulseSamples:zeroPulseSamples;
        sampleCount+=pulseSamples*2+2;
        byte<<=1;
      }
    }
    sampleCount++;
    uint32_t pauseSamples=(sampleRate*TAP_PAUSE_MS)/1000;
    sampleCount+=pauseSamples+1;
  }
  file->seek(currentPos,SEEK_SET);
  return (uint32_t)sampleCount;
}

void AudioGeneratorTAP::getBlockName(uint8_t block, char* name,size_t maxLen){
  if(!file||maxLen==0||block>=totalBlocks){
    if(maxLen>0) name[0]='\0';
    return;
  }
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  for(uint8_t i=0;i<=block;i++){
    uint8_t lenBuf[2];
    if(file->read(lenBuf,2)!=2){
      name[0]='\0';
      file->seek(currentPos,SEEK_SET);
      return;
    }
    uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
    if(blockLen==0){
      name[0]='\0';
      file->seek(currentPos,SEEK_SET);
      return;
    }
    if(i==block){
      uint8_t flag;
      if(file->read(&flag,1)!=1){
        name[0]='\0';
        file->seek(currentPos,SEEK_SET);
        return;
      }
      if(flag==0x00&&blockLen==19){
        uint8_t type;
        if(file->read(&type,1)!=1){
          snprintf(name, maxLen,"Block %u",block+1);
          file->seek(currentPos,SEEK_SET);
          return;
        }
        char blockName[11];
        if(file->read((uint8_t*)blockName,10)==10){
          blockName[10]='\0';
          // Stop at first non-printable
          for(int j=0;j<10;j++){
            if(blockName[j]<32||blockName[j]>=127){
              blockName[j]='\0';
              break;
            }
          }
          // Trim spaces from both sides
          int firstNonSpace=-1,lastNonSpace=-1;
          for(int j=0;j<10&&blockName[j];j++){
            if(blockName[j]!=' '){
              if(firstNonSpace==-1) firstNonSpace=j;
              lastNonSpace=j;
            }
          }
          if(firstNonSpace>=0&&lastNonSpace>=firstNonSpace){
            int len=lastNonSpace-firstNonSpace+1;
            memmove(blockName,&blockName[firstNonSpace],len);
            blockName[len]='\0';
            strncpy(name,blockName,maxLen-1);
            name[maxLen-1]='\0';
            file->seek(currentPos,SEEK_SET);
            return;
          }
        }
      }
      snprintf(name,maxLen,"Block %u",block+1);
      file->seek(currentPos,SEEK_SET);
      return;
    }
    file->seek(file->getPos()+blockLen,SEEK_SET);
  }
  file->seek(currentPos,SEEK_SET);
  name[0]='\0';
}

bool AudioGeneratorTAP::loop(){
  if(stopping||!running) goto done;
  if(!file||!output){
    running=false;
    goto done;
  }
  if(!output->ConsumeSample(lastSample)) goto done;
  do {
    currentSample++;
    if(trackFrame){
      sampleAccumulator+=speedMultiplier;
      if(sampleAccumulator>=sampleRate/50){
        (*trackFrame)++;
        sampleAccumulator-=sampleRate/50;
      }
    }
    if(pulseRemain>0){
      lastSample[0]=lastSample[1]=level;
      // Update EQ for tape formats
      if(eqBuffer&&channelEQBuffer){
        // Map pulse duration to frequency index (shorter pulse=higher freq)
        // Typical pulse durations: 667us (pilot), 190us (sync), 244us (zero), 489us (one)
        int freq_idx=48;
        if(pulseDuration>0&&pulseDuration<2000){
          freq_idx=95-((pulseDuration*96)/2000);
          if(freq_idx>=96) freq_idx=95;
          if(freq_idx<0) freq_idx=0;
        }
        // Add intensity to EQ bar
        if(eqBuffer[freq_idx]<8) eqBuffer[freq_idx]+=2;
        // Set volume based on pulse type: Pilot=48, Sync1=64, Sync2=56, One=64, Zero=56
        uint8_t volume=64;
        if(pulseDuration>=TAP_PILOT_PULSE-10&&pulseDuration<=TAP_PILOT_PULSE+10){
          volume=48;  // Pilot tone
        }else if(pulseDuration>=TAP_SYNC1_PULSE-10&&pulseDuration<=TAP_SYNC1_PULSE+10){
          volume=64;  // Sync1
        }else if(pulseDuration>=TAP_SYNC2_PULSE-10&&pulseDuration<=TAP_SYNC2_PULSE+10){
          volume=56;  // Sync2
        }else if(pulseDuration>=TAP_ONE_PULSE-10&&pulseDuration<=TAP_ONE_PULSE+10){
          volume=64;  // One bit
        }else if(pulseDuration>=TAP_ZERO_PULSE-10&&pulseDuration<=TAP_ZERO_PULSE+10){
          volume=56;  // Zero bit
        }
        channelEQBuffer[0]=volume;
        channelEQBuffer[1]=volume;
      }
      pulseRemain--;
      if(pulseRemain==0){
        level=-level; // Toggle level after each pulse
      }
      continue;
    }
    switch(state){
      case STATE_READ_BLOCK:
        if(blockAlreadyRead){
          blockAlreadyRead=false;
          break;
        }
        if(!readNextBlock()){
          // finalPauseRemain=(sampleRate*5)/speedMultiplier;
          // state=STATE_FINAL_PAUSE;
          state=STATE_DONE;
          goto done;
        }
        break;
      case STATE_PILOT:
        generatePulse(TAP_PILOT_PULSE);
        pilotRemain--;
        if(pilotRemain==0){
          state=STATE_SYNC1;
        }
        break;
      case STATE_SYNC1:
        generatePulse(TAP_SYNC1_PULSE);
        state=STATE_SYNC2;
        break;
      case STATE_SYNC2:
        generatePulse(TAP_SYNC2_PULSE);
        state=STATE_DATA;
        currentBit=0;
        pass=0;
        break;
      case STATE_DATA:
        if(currentBit==0){
          if(bytesRemain==0){
            state=STATE_PAUSE;
            // Pause affected by speed
            pauseRemain=(sampleRate*TAP_PAUSE_MS)/(1000*speedMultiplier);
            break;
          }
          if(file->read(&currentByte,1)!=1){
            state=STATE_DONE;
            running=false;
            goto done;
          }
          bytesRemain--;
          currentBit=8;
          pass=0;
        }
        generatePulse((currentByte&0x80)?TAP_ONE_PULSE:TAP_ZERO_PULSE);
        pass++;
        if(pass==2){
          currentByte<<=1;
          currentBit--;
          pass=0;
        }
        break;
      case STATE_PAUSE:
        lastSample[0]=lastSample[1]=0;
        pauseRemain--;
        if(pauseRemain==0){
          if(currentBlock<totalBlocks){
            currentBlock++;
            if(authorString&&typeString){
              char blockName[32];
              getBlockName(currentBlock,blockName,sizeof(blockName));
              if(strlen(blockName)==0){
                snprintf(authorString,180,"Block %d",currentBlock+1);
              }else{
                snprintf(authorString,180,"%s",blockName);
              }
              const char* blockType=getBlockType(currentBlock);
              snprintf(typeString,16,"%s",blockType);
            }
          }
          state=STATE_READ_BLOCK;
        }
        break;
      case STATE_FINAL_PAUSE:
        lastSample[0]=lastSample[1]=0;
        finalPauseRemain--;
        if(finalPauseRemain==0){
          state=STATE_DONE;
          running=false;
          goto done;
        }
        break;
      case STATE_DONE:
        running=false;
        goto done;
    }
  }while(output->ConsumeSample(lastSample));
done:
  output->loop();
  return running;
}

const char* AudioGeneratorTAP::getBlockType(uint8_t block){
  if(!file||block>=totalBlocks) return "Block";
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  for(uint8_t i=0;i<=block;i++){
    uint8_t lenBuf[2];
    if(file->read(lenBuf,2)!=2){
      file->seek(currentPos,SEEK_SET);
      return "Block";
    }
    uint16_t blockLen=lenBuf[0]|(lenBuf[1]<<8);
    if(blockLen==0){
      file->seek(currentPos,SEEK_SET);
      return "Block";
    }
    if(i==block){
      uint8_t flag;
      if(file->read(&flag,1)!=1){
        file->seek(currentPos,SEEK_SET);
        return "Block";
      }
      if(flag==0x00&&blockLen==19){
        uint8_t type;
        if(file->read(&type,1)==1){
          file->seek(currentPos,SEEK_SET);
          switch(type){
            case 0: return "Program";
            case 1: return "Number array";
            case 2: return "Char array";
            case 3: return "Bytes";
            default: return "Block";
          }
        }
      }
      file->seek(currentPos,SEEK_SET);
      return "Data";
    }
    file->seek(file->getPos()+blockLen,SEEK_SET);
  }
  file->seek(currentPos,SEEK_SET);
  return "Block";
}

unsigned long AudioGeneratorTAP::setSpeed(uint8_t speed){
  uint8_t oldSpeed=speedMultiplier;
  uint8_t newSpeed=(speed==0)?1:(speed==1)?2:4;
  speedMultiplier=newSpeed;
  if(pulseRemain>0){
    pulseRemain=(pulseRemain*oldSpeed)/newSpeed;
    if(pulseRemain==0) pulseRemain=1;
  }
  if(state==STATE_PAUSE&&pauseRemain>0){
    pauseRemain=(pauseRemain*oldSpeed)/newSpeed;
  }
  return 0;
}
