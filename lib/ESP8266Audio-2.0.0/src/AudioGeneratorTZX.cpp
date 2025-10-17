/*
  AudioGeneratorTZX - ZX Spectrum TZX file to audio converter
  Supports ID10, ID11, ID12, ID13, ID14, ID20, TAP blocks
  
  Copyright (C) 2025 by Spawn
*/

#include "AudioGeneratorTZX.h"

AudioGeneratorTZX::AudioGeneratorTZX(){
  sampleRate=44100;
  level=0;
  state=STATE_HEADER;
  pulseRemain=0;
  currentSample=0;
  baseSample=0;
  sampleAccumulator=0;
  totalBlocks=0;
  currentBlock=0;
  hasText=false;
  memset(textDescription,0,sizeof(textDescription));
  memset(groupName,0,sizeof(groupName));
  eqBuffer=nullptr;
  channelEQBuffer=nullptr;
  lastSample[0]=0;
  lastSample[1]=0;
  output=nullptr;
  file=nullptr;
  running=false;
}

AudioGeneratorTZX::~AudioGeneratorTZX(){
}

bool AudioGeneratorTZX::initializeFile(AudioFileSource* source,const char** message){
  if(!source) return false;
  file=source;
  if(!file->isOpen()) return false;
  uint32_t fileSize=file->getSize();
  if(fileSize<10){
    if(message) *message="Invalid TZX file!";
    return false;
  }
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint8_t hdr[10];
  if(file->read(hdr,10)!=10){
    if(message) *message="Invalid TZX file!";
    file->seek(currentPos,SEEK_SET);
    return false;
  }
  if(hdr[0]!='Z'||hdr[1]!='X'||hdr[2]!='T'||hdr[3]!='a'||hdr[4]!='p'||hdr[5]!='e'||hdr[6]!='!'){
    if(message) *message="Invalid TZX file!";
    file->seek(currentPos,SEEK_SET);
    return false;
  }
  file->seek(currentPos,SEEK_SET);
  return true;
}

bool AudioGeneratorTZX::begin(AudioFileSource *source,AudioOutput *out){
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
  hasText=false;
  if(savedCurrentBlock>0||savedBlockAlreadyRead){
    currentBlock=savedCurrentBlock;
    blockAlreadyRead=savedBlockAlreadyRead;
    state=savedState;
    baseSample=savedBaseSample;
    currentSample=baseSample;
  }else{
    state=STATE_HEADER;
    currentSample=0;
    baseSample=0;
    sampleAccumulator=0;
    blockAlreadyRead=false;
  }
  memset(textDescription,0,sizeof(textDescription));
  memset(groupName,0,sizeof(groupName));
  // DON'T reset eqBuffer and channelEQBuffer-they are set by initEQBuffers() before begin()
  return true;
}

bool AudioGeneratorTZX::stop(){
  if(!running) return true;
  stopping=true;
  vTaskDelay(pdMS_TO_TICKS(10));
  if(output) output->flush();
  if(file&&file->isOpen()) file->close();
  if(output) output->stop();
  running=false;
  return true;
}

bool AudioGeneratorTZX::isRunning(){
  return running;
}

void AudioGeneratorTZX::generatePulse(uint16_t durationUs){
  pulseDuration=durationUs;
  uint32_t samples=(sampleRate*durationUs)/(1000000*speedMultiplier);
  pulseRemain=(samples>0)?samples:1;
}

uint16_t AudioGeneratorTZX::readWord(){
  uint8_t buf[2];
  if(file->read(buf,2)!=2) return 0;
  return buf[0]|(buf[1]<<8);
}

uint32_t AudioGeneratorTZX::readLong(){
  uint8_t buf[3];
  if(file->read(buf,3)!=3) return 0;
  return buf[0]|(buf[1]<<8)|(buf[2]<<16);
}

uint16_t AudioGeneratorTZX::tickToUs(uint16_t ticks){
  return (uint16_t)((((float)ticks)/3.5)+0.5);
}

bool AudioGeneratorTZX::readHeader(){
  uint8_t hdr[10];
  if(file->read(hdr,10)!=10) return false;
  if(hdr[0]!='Z'||hdr[1]!='X'||hdr[2]!='T') return false;
  state=STATE_GET_ID;
  return true;
}

bool AudioGeneratorTZX::processBlock(){
  uint8_t id;
  if(file->read(&id,1)!=1){
    state=STATE_DONE;
    return false;
  }
  currentID=(BlockID)id;
  switch(currentID){
    case ID10:{
      pauseLength=readWord();
      uint16_t len=readWord();
      uint8_t flag;
      if(file->read(&flag,1)!=1) return false;
      bytesToRead=len;
      file->seek(file->getPos()-1,SEEK_SET);
      pilotCount=(flag==0)?TZX_PILOT_HEADER:TZX_PILOT_DATA;
      pilotPulse=TZX_PILOT_PULSE;
      sync1Pulse=TZX_SYNC1_PULSE;
      sync2Pulse=TZX_SYNC2_PULSE;
      zeroPulse=TZX_ZERO_PULSE;
      onePulse=TZX_ONE_PULSE;
      usedBits=8;
      pilotRemain=pilotCount;
      state=STATE_PILOT;
      break;
    }
    case ID11:{
      pilotPulse=tickToUs(readWord());
      sync1Pulse=tickToUs(readWord());
      sync2Pulse=tickToUs(readWord());
      zeroPulse=tickToUs(readWord());
      onePulse=tickToUs(readWord());
      pilotCount=readWord();
      uint8_t ub;
      file->read(&ub,1);
      usedBits=ub;
      pauseLength=readWord();
      bytesToRead=readLong();
      pilotRemain=pilotCount;
      state=STATE_PILOT;
      break;
    }
    case ID12:{
      pilotPulse=tickToUs(readWord());
      pilotCount=readWord();
      pilotRemain=pilotCount;
      state=STATE_PURE_TONE;
      break;
    }
    case ID13:{
      uint8_t sp;
      file->read(&sp,1);
      seqPulses=sp;
      seqRemain=seqPulses;
      state=STATE_PULSE_SEQ;
      break;
    }
    case ID14:{
      zeroPulse=tickToUs(readWord());
      onePulse=tickToUs(readWord());
      uint8_t ub;
      file->read(&ub,1);
      usedBits=ub;
      pauseLength=readWord();
      bytesToRead=readLong();
      state=STATE_DATA;
      currentBit=0;
      pass=0;
      break;
    }
    case ID20:{
      pauseLength=readWord();
      if(pauseLength>0){
        // Pause affected by speed
        pauseRemain=(sampleRate*pauseLength)/(1000*speedMultiplier);
        state=STATE_PAUSE;
      }else{
        state=STATE_GET_ID;
      }
      break;
    }
    case ID21:{
      uint8_t len;
      file->read(&len,1);
      if(!hasText&&len>0&&len<sizeof(groupName)){
        file->read((uint8_t*)groupName,len);
        groupName[len]='\0';
        hasText=true;
      }else{
        file->seek(file->getPos()+len,SEEK_SET);
      }
      state=STATE_GET_ID;
      break;
    }
    case ID22:
      state=STATE_GET_ID;
      break;
    case ID30:{
      uint8_t len;
      file->read(&len,1);
      if(!hasText&&len>0&&len<sizeof(textDescription)){
        file->read((uint8_t*)textDescription,len);
        textDescription[len]='\0';
        hasText=true;
      }else{
        file->seek(file->getPos()+len,SEEK_SET);
      }
      state=STATE_GET_ID;
      break;
    }
    default:{
      if(id==0x31){
        file->seek(file->getPos()+1,SEEK_SET);
        state=STATE_GET_ID;
      }else if(id==0x32){
        uint16_t len=readWord();
        file->seek(file->getPos()+len,SEEK_SET);
        state=STATE_GET_ID;
      }else if(id==0x33){
        uint8_t count;
        file->read(&count,1);
        file->seek(file->getPos()+count*3,SEEK_SET);
        state=STATE_GET_ID;
      }else{
        uint32_t startPos=file->getPos()-1;
        uint32_t fileSize=file->getSize();
        bool found=false;
        for(uint32_t searchPos=startPos+1;searchPos<fileSize-5&&searchPos<startPos+50000;searchPos++){
          file->seek(searchPos,SEEK_SET);
          uint8_t testId;
          if(file->read(&testId,1)==1){
            if(testId==0x10||testId==0x11||testId==0x20){
              uint8_t testBuf[4];
              if(file->read(testBuf,4)==4){
                uint16_t testLen=testBuf[2]|(testBuf[3]<<8);
                if(testLen>0&&testLen<10000){
                  file->seek(searchPos,SEEK_SET);
                  found=true;
                  state=STATE_GET_ID;
                  break;
                }
              }
            }
          }
        }
        if(!found){
          state=STATE_DONE;
          return false;
        }
      }
      break;
    }
  }
  
  return true;
}

signed long AudioGeneratorTZX::getPlaybackTime(bool oneFiftieth){
  if(!file) return -1;
  uint32_t currentPos=file->getPos();
  file->seek(10,SEEK_SET);
  uint64_t sampleCount=0;
  uint8_t id;
  uint32_t pilotPulseSamples=(sampleRate*TZX_PILOT_PULSE)/1000000;
  uint32_t sync1Samples=(sampleRate*TZX_SYNC1_PULSE)/1000000;
  uint32_t sync2Samples=(sampleRate*TZX_SYNC2_PULSE)/1000000;
  uint32_t zeroPulseSamples=(sampleRate*TZX_ZERO_PULSE)/1000000;
  uint32_t onePulseSamples=(sampleRate*TZX_ONE_PULSE)/1000000;
  if(pilotPulseSamples==0) pilotPulseSamples=1;
  if(sync1Samples==0) sync1Samples=1;
  if(sync2Samples==0) sync2Samples=1;
  if(zeroPulseSamples==0) zeroPulseSamples=1;
  if(onePulseSamples==0) onePulseSamples=1;
  while(file->read(&id,1)==1){
    if(id==0x10){
      uint16_t pause=readWord();
      uint16_t len=readWord();
      uint8_t flag;
      file->read(&flag,1);
      sampleCount++;
      uint16_t pilot=(flag==0)?TZX_PILOT_HEADER:TZX_PILOT_DATA;
      sampleCount+=(uint64_t)pilot*pilotPulseSamples+pilot;
      sampleCount+=sync1Samples+1;
      sampleCount+=sync2Samples+1;
      for(uint8_t bit=0;bit<8;bit++){
        uint32_t pulseSamples=(flag&0x80)?onePulseSamples:zeroPulseSamples;
        sampleCount+=pulseSamples*2+2;
        flag<<=1;
      }
      for(uint16_t i=1;i<len;i++){
        uint8_t byte;
        if(file->read(&byte,1)!=1) break;
        for(uint8_t bit=0;bit<8;bit++){
          uint32_t pulseSamples=(byte&0x80)?onePulseSamples:zeroPulseSamples;
          sampleCount+=pulseSamples*2+2;
          byte<<=1;
        }
      }
      sampleCount++;
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
    }else if(id==0x11){
      uint16_t pilotPulse=tickToUs(readWord());
      uint16_t sync1=tickToUs(readWord());
      uint16_t sync2=tickToUs(readWord());
      uint16_t zero=tickToUs(readWord());
      uint16_t one=tickToUs(readWord());
      uint16_t pilotLen=readWord();
      uint8_t usedBits;
      file->read(&usedBits,1);
      uint16_t pause=readWord();
      uint32_t dataLen=readLong();
      sampleCount++;
      uint32_t pPulseSamples=(sampleRate*pilotPulse)/1000000;
      if(pPulseSamples==0) pPulseSamples=1;
      sampleCount+=(uint64_t)pilotLen*pPulseSamples+pilotLen;
      uint32_t s1Samples=(sampleRate*sync1)/1000000;
      uint32_t s2Samples=(sampleRate*sync2)/1000000;
      if(s1Samples==0) s1Samples=1;
      if(s2Samples==0) s2Samples=1;
      sampleCount+=s1Samples+1+s2Samples+1;
      uint32_t zSamples=(sampleRate*zero)/1000000;
      uint32_t oSamples=(sampleRate*one)/1000000;
      if(zSamples==0) zSamples=1;
      if(oSamples==0) oSamples=1;
      uint32_t avgSamples=(zSamples+oSamples)/2;
      sampleCount+=(uint64_t)(dataLen-1)*8*(avgSamples*2+2)+usedBits*(avgSamples*2+2);
      sampleCount++;
      file->seek(file->getPos()+dataLen,SEEK_SET);
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
    }else if(id==0x12){
      uint16_t pulse=tickToUs(readWord());
      uint16_t count=readWord();
      uint32_t pulseSamples=(sampleRate*pulse)/1000000;
      if(pulseSamples==0) pulseSamples=1;
      sampleCount+=(uint64_t)count*pulseSamples+count+1;
    }else if(id==0x13){
      uint8_t count;
      file->read(&count,1);
      for(uint8_t i=0;i<count;i++){
        uint16_t pulse=tickToUs(readWord());
        uint32_t pulseSamples=(sampleRate*pulse)/1000000;
        if(pulseSamples==0) pulseSamples=1;
        sampleCount+=pulseSamples+1;
      }
      sampleCount++;
    }else if(id==0x14){
      uint16_t zero=tickToUs(readWord());
      uint16_t one=tickToUs(readWord());
      uint8_t usedBits;
      file->read(&usedBits,1);
      uint16_t pause=readWord();
      uint32_t dataLen=readLong();
      sampleCount++;
      uint32_t zSamples=(sampleRate*zero)/1000000;
      uint32_t oSamples=(sampleRate*one)/1000000;
      if(zSamples==0) zSamples=1;
      if(oSamples==0) oSamples=1;
      uint32_t avgSamples=(zSamples+oSamples)/2;
      sampleCount+=(uint64_t)(dataLen-1)*8*(avgSamples*2+2)+usedBits*(avgSamples*2+2);
      sampleCount++;
      file->seek(file->getPos()+dataLen,SEEK_SET);
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
    }else if(id==0x20){
      uint16_t pause=readWord();
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
    }else if(id==0x21||id==0x30){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else if(id==0x31){
      file->seek(file->getPos()+1,SEEK_SET);
    }else if(id==0x32){
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x33){
      file->seek(file->getPos()+3*readWord(),SEEK_SET);
    }else{
      uint32_t startPos=file->getPos()-1;
      uint32_t fileSize=file->getSize();
      bool found=false;
      for(uint32_t searchPos=startPos+1;searchPos<fileSize-5&&searchPos<startPos+50000;searchPos++){
        file->seek(searchPos,SEEK_SET);
        uint8_t testId;
        if(file->read(&testId,1)==1){
          if(testId==0x10||testId==0x11||testId==0x20){
            uint8_t testBuf[4];
            if(file->read(testBuf,4)==4){
              uint16_t testLen=testBuf[2]|(testBuf[3]<<8);
              if(testLen>0&&testLen<10000){
                file->seek(searchPos,SEEK_SET);
                found=true;
                break;
              }
            }
          }
        }
      }
      if(!found) break;
    }
  }
  file->seek(currentPos,SEEK_SET);
  if(oneFiftieth){
    return (sampleCount*50)/sampleRate;
  }
  return (sampleCount*1000)/sampleRate;
}

void AudioGeneratorTZX::getTitle(char* title,size_t maxLen){
  if(!file||maxLen==0){
    if(maxLen>0) title[0]='\0';
    return;
  }
  uint32_t currentPos=file->getPos();
  file->seek(10,SEEK_SET);
  char result[256];
  result[0]='\0';
  int resultLen=0;
  uint8_t id;
  while(file->read(&id,1)==1&&resultLen<250){
    if(id==0x30||id==0x31||id==0x32){
      uint8_t len;
      if(id==0x32){
        uint16_t archiveLen=readWord();
        if(archiveLen>1&&archiveLen<200){
          char archiveBuf[200];
          if(file->read((uint8_t*)archiveBuf,archiveLen)==archiveLen){
            int i=1;
            while(i<archiveLen&&resultLen<248){
              if(i+1<archiveLen){
                uint8_t fieldLen=archiveBuf[i+1];
                i+=2;
                if(i+fieldLen<=archiveLen){
                  for(int j=0;j<fieldLen&&resultLen<248;j++){
                    if(archiveBuf[i+j]>=32&&archiveBuf[i+j]<127){
                      result[resultLen++]=archiveBuf[i+j];
                    }
                  }
                  result[resultLen++]=' ';
                  i+=fieldLen;
                }else{
                  break;
                }
              }else{
                break;
              }
            }
          }
        }
      }else if(id==0x31){
        file->read(&len,1);
        if(len>0&&len<200){
          char msgBuf[200];
          if(file->read((uint8_t*)msgBuf,len)==len){
            for(int i=0;i<len&&resultLen<250;i++){
              if(msgBuf[i]>=32&&msgBuf[i]<127){
                result[resultLen++]=msgBuf[i];
              }
            }
          }
        }
      }else{
        file->read(&len,1);
        if(len>0&&len<200){
          char textBuf[200];
          if(file->read((uint8_t*)textBuf,len)==len){
            for(int i=0;i<len&&resultLen<250;i++){
              if(textBuf[i]>=32&&textBuf[i]<127){
                result[resultLen++]=textBuf[i];
              }
            }
          }
        }
      }
    }else if(id==0x10){
      break;
    }else if(id==0x11){
      break;
    }else if(id==0x20||id==0x12){
      file->seek(file->getPos()+2,SEEK_SET);
    }else if(id==0x13){
      uint8_t sp;
      file->read(&sp,1);
      file->seek(file->getPos()+sp*2,SEEK_SET);
    }else if(id==0x21){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else{
      break;
    }
  }
  result[resultLen]='\0';
  char cleaned[256];
  int cleanedLen=0;
  bool lastWasSpace=false;
  for(int i=0;i<resultLen&&cleanedLen<250;i++){
    if(result[i]==' '){
      if(!lastWasSpace){
        cleaned[cleanedLen++]=' ';
        lastWasSpace=true;
      }
    }else{
      cleaned[cleanedLen++]=result[i];
      lastWasSpace=false;
    }
  }
  cleaned[cleanedLen]='\0';
  int start=0,end=cleanedLen-1;
  while(start<=end&&cleaned[start]==' ') start++;
  while(end>=start&&cleaned[end]==' ') end--;
  int finalLen=end-start+1;
  if(finalLen>0){
    memmove(cleaned,&cleaned[start],finalLen);
    cleaned[finalLen]='\0';
    strncpy(title,cleaned,maxLen-1);
    title[maxLen-1]='\0';
  }else{
    title[0]='\0';
  }
  file->seek(currentPos,SEEK_SET);
}

void AudioGeneratorTZX::getDescription(char* description,size_t maxLen){
  if(!file||maxLen==0){
    if(maxLen>0) description[0]='\0';
    return;
  }
  snprintf(description,maxLen,"%u blocks",totalBlocks);
}

void AudioGeneratorTZX::initEQBuffers(uint8_t* eqBuf,uint8_t* channelEQBuf){
  eqBuffer=eqBuf;
  channelEQBuffer=channelEQBuf;
  if(eqBuffer) memset(eqBuffer,0,96);
  if(channelEQBuffer) memset(channelEQBuffer,0,8);
}

bool AudioGeneratorTZX::loop(){
  if(stopping||!running) goto done;
  if(!file||!output){
    running=false;
    goto done;
  }
  if(!output->ConsumeSample(lastSample)) goto done;
  do{
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
      if(eqBuffer&&channelEQBuffer){
        int freq_idx=48;
        if(pulseDuration>0&&pulseDuration<2000){
          freq_idx=95-((pulseDuration*96)/2000);
          if(freq_idx>=96) freq_idx=95;
          if(freq_idx<0) freq_idx=0;
        }
        if(eqBuffer[freq_idx]<8) eqBuffer[freq_idx]+=2;
        // Set volume based on pulse type: Pilot=48, Sync1=64, Sync2=56, One=64, Zero=56
        uint8_t volume=64;
        if(pulseDuration>=pilotPulse-10&&pulseDuration<=pilotPulse+10){
          volume=48;  // Pilot tone
        }else if(pulseDuration>=sync1Pulse-10&&pulseDuration<=sync1Pulse+10){
          volume=64;  // Sync1
        }else if(pulseDuration>=sync2Pulse-10&&pulseDuration<=sync2Pulse+10){
          volume=56;  // Sync2
        }else if(pulseDuration>=onePulse-10&&pulseDuration<=onePulse+10){
          volume=64;  // One bit
        }else if(pulseDuration>=zeroPulse-10&&pulseDuration<=zeroPulse+10){
          volume=56;  // Zero bit
        }
        channelEQBuffer[0]=volume;
        channelEQBuffer[1]=volume;
      }
      pulseRemain--;
      if(pulseRemain==0){
        level=-level;
      }
      continue;
    }
    switch(state){
      case STATE_HEADER:
        if(!readHeader()){
          state=STATE_DONE;
          running=false;
          goto done;
        }
        break;
      case STATE_GET_ID:
        if(!processBlock()){
          // finalPauseRemain=(sampleRate*5)/speedMultiplier;
          // state=STATE_FINAL_PAUSE;
          state=STATE_DONE;
        }else{
          bool isDataBlock=(currentID==ID10||currentID==ID11||currentID==ID14);
          if(blockAlreadyRead&&isDataBlock){
            blockAlreadyRead=false;
          }
        }
        break;
      case STATE_PILOT:
        generatePulse(pilotPulse);
        pilotRemain--;
        if(pilotRemain==0){
          state=STATE_SYNC1;
        }
        break;
      case STATE_SYNC1:
        generatePulse(sync1Pulse);
        state=STATE_SYNC2;
        break;
      case STATE_SYNC2:
        generatePulse(sync2Pulse);
        state=STATE_DATA;
        currentBit=0;
        pass=0;
        break;
      case STATE_DATA:
        if(currentBit==0){
          if(bytesToRead==0){
            if(pauseLength>0){
              pauseRemain=(sampleRate*pauseLength)/(1000*speedMultiplier);
              state=STATE_PAUSE;
            }else{
              state=STATE_GET_ID;
            }
            break;
          }
          if(!file){
            state=STATE_DONE;
            running=false;
            goto done;
          }
          if(file->read(&currentByte,1)!=1){
            state=STATE_DONE;
            running=false;
            goto done;
          }
          bytesToRead--;
          currentBit=(bytesToRead==0)?usedBits:8;
          pass=0;
        }
        generatePulse((currentByte&0x80)?onePulse:zeroPulse);
        pass++;
        if(pass==2){
          currentByte<<=1;
          currentBit--;
          pass=0;
        }
        break;
      case STATE_PURE_TONE:
        generatePulse(pilotPulse);
        pilotRemain--;
        if(pilotRemain==0){
          state=STATE_GET_ID;
        }
        break;
      case STATE_PULSE_SEQ:
        if(seqRemain>0){
          uint16_t pulse=tickToUs(readWord());
          generatePulse(pulse);
          seqRemain--;
        }
        if(seqRemain==0){
          state=STATE_GET_ID;
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
          state=STATE_GET_ID;
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

uint8_t AudioGeneratorTZX::getTotalBlocks(){
  if(!file) return 0;
  uint32_t currentPos=file->getPos();
  file->seek(10,SEEK_SET);
  uint8_t count=0;
  uint8_t id;
  while(file->read(&id,1)==1&&count<255){
    if(id==0x10){
      readWord();
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x11){
      file->seek(file->getPos()+15,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x14){
      file->seek(file->getPos()+7,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x20||id==0x12){
      file->seek(file->getPos()+2,SEEK_SET);
    }else if(id==0x13){
      uint8_t sp;
      file->read(&sp,1);
      file->seek(file->getPos()+sp*2,SEEK_SET);
    }else if(id==0x21||id==0x30){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else if(id==0x31){
      file->seek(file->getPos()+1,SEEK_SET);
    }else if(id==0x32){
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
    }else{
      break;
    }
  }
  file->seek(currentPos,SEEK_SET);
  totalBlocks=count;
  return count;
}

void AudioGeneratorTZX::setCurrentBlock(uint8_t block){
  if(!file||block>=totalBlocks) return;
  currentBlock=block;
  baseSample=getBlockStartSample(block);
  if(trackFrame){
    *trackFrame=(baseSample*50)/sampleRate;
    sampleAccumulator=0;
  }
  file->seek(10,SEEK_SET);
  uint8_t id;
  uint8_t dataBlockCount=0;
  while(dataBlockCount<=block){
    uint32_t blockStartPos=file->getPos();
    if(file->read(&id,1)!=1){
      state=STATE_GET_ID;
      blockAlreadyRead=false;
      return;
    }
    if(id==0x10){
      if(dataBlockCount==block){
        file->seek(blockStartPos,SEEK_SET);
        state=STATE_GET_ID;
        blockAlreadyRead=true;
        return;
      }
      readWord();
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
      dataBlockCount++;
    }else if(id==0x11){
      if(dataBlockCount==block){
        file->seek(blockStartPos,SEEK_SET);
        state=STATE_GET_ID;
        blockAlreadyRead=true;
        return;
      }
      file->seek(file->getPos()+15,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      dataBlockCount++;
    }else if(id==0x14){
      if(dataBlockCount==block){
        file->seek(blockStartPos,SEEK_SET);
        state=STATE_GET_ID;
        blockAlreadyRead=true;
        return;
      }
      file->seek(file->getPos()+7,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      dataBlockCount++;
    }else if(id==0x20||id==0x12){
      file->seek(file->getPos()+2,SEEK_SET);
    }else if(id==0x13){
      uint8_t sp;
      file->read(&sp,1);
      file->seek(file->getPos()+sp*2,SEEK_SET);
    }else if(id==0x21||id==0x30){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else if(id==0x31){
      file->seek(file->getPos()+1,SEEK_SET);
    }else if(id==0x32){
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
    }else{
      break;
    }
  }
  state=STATE_GET_ID;
  blockAlreadyRead=false;
}

uint32_t AudioGeneratorTZX::getBlockStartSample(uint8_t block){
  if(!file||block>=totalBlocks) return 0;
  uint32_t currentPos=file->getPos();
  file->seek(10,SEEK_SET);
  uint64_t sampleCount=0;
  uint8_t id;
  uint8_t dataBlockCount=0;
  uint32_t pilotPulseSamples=(sampleRate*TZX_PILOT_PULSE)/1000000;
  uint32_t sync1Samples=(sampleRate*TZX_SYNC1_PULSE)/1000000;
  uint32_t sync2Samples=(sampleRate*TZX_SYNC2_PULSE)/1000000;
  uint32_t zeroPulseSamples=(sampleRate*TZX_ZERO_PULSE)/1000000;
  uint32_t onePulseSamples=(sampleRate*TZX_ONE_PULSE)/1000000;
  if(pilotPulseSamples==0) pilotPulseSamples=1;
  if(sync1Samples==0) sync1Samples=1;
  if(sync2Samples==0) sync2Samples=1;
  if(zeroPulseSamples==0) zeroPulseSamples=1;
  if(onePulseSamples==0) onePulseSamples=1;
  while(dataBlockCount<block){
    if(file->read(&id,1)!=1){
      file->seek(currentPos,SEEK_SET);
      return 0;
    }
    if(id==0x10){
      uint16_t pause=readWord();
      uint16_t len=readWord();
      uint8_t flag;
      file->read(&flag,1);
      sampleCount++;
      uint16_t pilot=(flag==0)?TZX_PILOT_HEADER:TZX_PILOT_DATA;
      sampleCount+=(uint64_t)pilot*pilotPulseSamples+pilot;
      sampleCount+=sync1Samples+1;
      sampleCount+=sync2Samples+1;
      for(uint8_t bit=0;bit<8;bit++){
        uint32_t pulseSamples=(flag&0x80)?onePulseSamples:zeroPulseSamples;
        sampleCount+=pulseSamples*2+2;
        flag<<=1;
      }
      for(uint16_t i=1;i<len;i++){
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
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
      dataBlockCount++;
    }else if(id==0x11){
      uint16_t pilotPulse=tickToUs(readWord());
      uint16_t sync1=tickToUs(readWord());
      uint16_t sync2=tickToUs(readWord());
      uint16_t zero=tickToUs(readWord());
      uint16_t one=tickToUs(readWord());
      uint16_t pilotLen=readWord();
      uint8_t usedBits;
      file->read(&usedBits,1);
      uint16_t pause=readWord();
      uint32_t dataLen=readLong();
      sampleCount++;
      uint32_t pPulseSamples=(sampleRate*pilotPulse)/1000000;
      if(pPulseSamples==0) pPulseSamples=1;
      sampleCount+=(uint64_t)pilotLen*pPulseSamples+pilotLen;
      uint32_t s1Samples=(sampleRate*sync1)/1000000;
      uint32_t s2Samples=(sampleRate*sync2)/1000000;
      if(s1Samples==0) s1Samples=1;
      if(s2Samples==0) s2Samples=1;
      sampleCount+=s1Samples+1+s2Samples+1;
      uint32_t zSamples=(sampleRate*zero)/1000000;
      uint32_t oSamples=(sampleRate*one)/1000000;
      if(zSamples==0) zSamples=1;
      if(oSamples==0) oSamples=1;
      uint32_t avgSamples=(zSamples+oSamples)/2;
      sampleCount+=(uint64_t)(dataLen-1)*8*(avgSamples*2+2)+usedBits*(avgSamples*2+2);
      sampleCount++;
      file->seek(file->getPos()+dataLen,SEEK_SET);
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
      dataBlockCount++;
    }else if(id==0x14){
      uint16_t zero=tickToUs(readWord());
      uint16_t one=tickToUs(readWord());
      uint8_t usedBits;
      file->read(&usedBits,1);
      uint16_t pause=readWord();
      uint32_t dataLen=readLong();
      sampleCount++;
      uint32_t zSamples=(sampleRate*zero)/1000000;
      uint32_t oSamples=(sampleRate*one)/1000000;
      if(zSamples==0) zSamples=1;
      if(oSamples==0) oSamples=1;
      uint32_t avgSamples=(zSamples+oSamples)/2;
      sampleCount+=(uint64_t)(dataLen-1)*8*(avgSamples*2+2)+usedBits*(avgSamples*2+2);
      sampleCount++;
      file->seek(file->getPos()+dataLen,SEEK_SET);
      if(pause>0){
        uint32_t pauseSamples=(sampleRate*pause)/1000;
        sampleCount+=pauseSamples+1;
      }
      dataBlockCount++;
    }else if(id==0x20||id==0x12){
      file->seek(file->getPos()+2,SEEK_SET);
    }else if(id==0x13){
      uint8_t sp;
      file->read(&sp,1);
      file->seek(file->getPos()+sp*2,SEEK_SET);
    }else if(id==0x21||id==0x30){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else if(id==0x31){
      file->seek(file->getPos()+1,SEEK_SET);
    }else if(id==0x32){
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
    }else{
      file->seek(currentPos,SEEK_SET);
      return 0;
    }
  }
  file->seek(currentPos,SEEK_SET);
  return (uint32_t)sampleCount;
}

unsigned long AudioGeneratorTZX::setSpeed(uint8_t speed){
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

void AudioGeneratorTZX::getBlockName(uint8_t block,char* name,size_t maxLen){
  if(!file||maxLen==0||block>=totalBlocks){
    if(maxLen>0) name[0]='\0';
    return;
  }
  uint32_t currentPos=file->getPos();
  file->seek(10,SEEK_SET);
  uint8_t count=0;
  while(true){
    uint8_t id;
    if(file->read(&id,1)!=1){
      name[0]='\0';
      file->seek(currentPos,SEEK_SET);
      return;
    }
    if(id==0x10){
      if(count==block){
        uint16_t pause=readWord();
        uint16_t len=readWord();
        uint8_t flag;
        if(file->read(&flag,1)!=1){
          name[0]='\0';
          file->seek(currentPos,SEEK_SET);
          return;
        }
        if(flag==0x00&&len==19){
          uint8_t allData[18];
          if(file->read(allData,18)==18){
            uint8_t type=allData[0];
            char blockName[11];
            bool hasParams=(allData[1]<0x20&&allData[2]<0x20&&allData[3]<0x20);
            int nameOffset=hasParams?4:1;
            memcpy(blockName,&allData[nameOffset],10);
            blockName[10]='\0';
            int validLen=0;
            for(int j=0;j<10;j++){
              unsigned char c=allData[nameOffset+j];
              if(c<32||c>=127) break;
              blockName[validLen++]=c;
            }
            blockName[validLen]='\0';
            int firstNonSpace=-1,lastNonSpace=-1;
            for(int j=0;j<validLen;j++){
              if(blockName[j]!=' '){
                if(firstNonSpace==-1) firstNonSpace=j;
                lastNonSpace=j;
              }
            }
            if(firstNonSpace>=0&&lastNonSpace>=firstNonSpace){
              int len=lastNonSpace-firstNonSpace+1;
              memmove(blockName,&blockName[firstNonSpace],len);
              blockName[len]='\0';
            }else{
              blockName[0]='\0';
            }
            if(strlen(blockName)>0){
              strncpy(name,blockName,maxLen-1);
              name[maxLen-1]='\0';
            }else{
              name[0]='\0';
            }
            file->seek(currentPos,SEEK_SET);
            return;
          }
        }
        name[0]='\0';
        file->seek(currentPos,SEEK_SET);
        return;
      }
      readWord();
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x11){
      if(count==block){
        name[0]='\0';
        file->seek(currentPos,SEEK_SET);
        return;
      }
      file->seek(file->getPos()+15,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x14){
      if(count==block){
        name[0]='\0';
        file->seek(currentPos,SEEK_SET);
        return;
      }
      file->seek(file->getPos()+7,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x20||id==0x12){
      file->seek(file->getPos()+2,SEEK_SET);
    }else if(id==0x13){
      uint8_t sp;
      file->read(&sp,1);
      file->seek(file->getPos()+sp*2,SEEK_SET);
    }else if(id==0x21||id==0x30){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else if(id==0x31){
      file->seek(file->getPos()+1,SEEK_SET);
    }else if(id==0x32){
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
    }else{
      break;
    }
  }
  file->seek(currentPos,SEEK_SET);
  name[0]='\0';
}

const char* AudioGeneratorTZX::getBlockType(uint8_t block){
  if(!file||block>=totalBlocks){
    return "Block";
  }
  uint32_t currentPos=file->getPos();
  file->seek(10,SEEK_SET);
  uint8_t count=0;
  while(true){
    uint8_t id;
    if(file->read(&id,1)!=1){
      file->seek(currentPos,SEEK_SET);
      return "Block";
    }
    if(id==0x10){
      if(count==block){
        uint16_t pause=readWord();
        uint16_t len=readWord();
        uint8_t flag;
        if(file->read(&flag,1)==1){
          if(flag==0x00&&len==19){
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
      }
      readWord();
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x11){
      if(count==block){
        file->seek(currentPos,SEEK_SET);
        return "Turbo";
      }
      file->seek(file->getPos()+15,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x14){
      if(count==block){
        file->seek(currentPos,SEEK_SET);
        return "Turbo";
      }
      file->seek(file->getPos()+7,SEEK_SET);
      uint32_t len=readLong();
      file->seek(file->getPos()+len,SEEK_SET);
      count++;
    }else if(id==0x20||id==0x12){
      file->seek(file->getPos()+2,SEEK_SET);
    }else if(id==0x13){
      uint8_t sp;
      file->read(&sp,1);
      file->seek(file->getPos()+sp*2,SEEK_SET);
    }else if(id==0x21||id==0x30){
      uint8_t len;
      file->read(&len,1);
      file->seek(file->getPos()+len,SEEK_SET);
    }else if(id==0x22){
    }else if(id==0x31){
      file->seek(file->getPos()+1,SEEK_SET);
    }else if(id==0x32){
      uint16_t len=readWord();
      file->seek(file->getPos()+len,SEEK_SET);
    }else{
      break;
    }
  }
  file->seek(currentPos,SEEK_SET);
  return "Block";
}
