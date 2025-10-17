/*
  AudioGeneratorTAP - ZX Spectrum TAP file to audio converter
  Converts TAP cassette images to audio waveform for DAC playback

  Copyright (C) 2025 by Spawn
*/

#ifndef _AUDIOGENERATORTAP_H
#define _AUDIOGENERATORTAP_H

#include "AudioGenerator.h"

// ZX Spectrum timing constants (in microseconds)
#define TAP_PILOT_PULSE     619
#define TAP_SYNC1_PULSE     191
#define TAP_SYNC2_PULSE     210
#define TAP_ZERO_PULSE      244
#define TAP_ONE_PULSE       489
#define TAP_PILOT_HEADER    8063
#define TAP_PILOT_DATA      3223
#define TAP_PAUSE_MS        1000

class AudioGeneratorTAP:public AudioGenerator{
  public:
    AudioGeneratorTAP();
    virtual ~AudioGeneratorTAP() override;
    virtual bool begin(AudioFileSource *source,AudioOutput *out) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override;
    signed long getPlaybackTime(bool oneFiftieth=true);
    void initTrackFrame(unsigned long* tF,signed long* totalLength=nullptr,char* authorStr=nullptr,char* typeLabel=nullptr){ 
      trackFrame=tF; 
      trackLength=totalLength;
      authorString=authorStr;
      typeString=typeLabel;
    }
    void getTitle(char* title,size_t maxLen);
    void getDescription(char* description,size_t maxLen);
    void initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer);
    bool initializeFile(AudioFileSource* source,const char** message=nullptr);
    uint8_t getTotalBlocks();
    void setCurrentBlock(uint8_t block);
    uint8_t getCurrentBlock(){return currentBlock;}
    void getBlockName(uint8_t block,char* name,size_t maxLen);
    const char* getBlockType(uint8_t block);
    unsigned long setSpeed(uint8_t speed);  // Change speed dynamically during playback, returns new total time
    uint32_t getBlockStartSample(uint8_t block);  // Get start sample of block
    
  private:
    unsigned long* trackFrame=nullptr;
    signed long* trackLength=nullptr;
    char* authorString=nullptr;
    char* typeString=nullptr;
    
  private:
    enum State{
      STATE_READ_BLOCK,
      STATE_PILOT,
      STATE_SYNC1,
      STATE_SYNC2,
      STATE_DATA,
      STATE_PAUSE,
      STATE_FINAL_PAUSE,
      STATE_DONE
    };
    
    void generatePulse(uint16_t durationUs);
    bool readNextBlock();
    
    State state;
    uint16_t blockLen;
    uint16_t pilotCount;
    uint16_t pilotRemain;
    uint8_t currentByte;
    uint8_t currentBit;
    uint8_t pass;
    uint16_t bytesRemain;
    uint32_t pauseRemain;
    uint32_t finalPauseRemain;
    
    uint32_t sampleRate;
    int16_t level;
    uint32_t pulseRemain;
    uint16_t pulseDuration;
    uint32_t currentSample;
    uint32_t baseSample;
    uint32_t sampleAccumulator;
    
    char programName[11];
    uint8_t totalBlocks;
    uint8_t currentBlock;
    bool headerParsed;
    
    uint8_t* eqBuffer;
    uint8_t* channelEQBuffer;
    uint8_t speedMultiplier=1;
    volatile bool stopping=false;
    bool blockAlreadyRead=false;
    int16_t lastSample[2];
};

#endif
