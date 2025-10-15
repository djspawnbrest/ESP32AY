/*
  AudioGeneratorTZX - ZX Spectrum TZX file to audio converter
  Supports ID10, ID11, ID12, ID13, ID14, ID20, TAP blocks
  
  Copyright (C) 2025 by Spawn
*/

#ifndef _AUDIOGENERATORTZX_H
#define _AUDIOGENERATORTZX_H

#include "AudioGenerator.h"

#define TZX_PILOT_PULSE     619
#define TZX_SYNC1_PULSE     191
#define TZX_SYNC2_PULSE     210
#define TZX_ZERO_PULSE      244
#define TZX_ONE_PULSE       489
#define TZX_PILOT_HEADER    8063
#define TZX_PILOT_DATA      3223

class AudioGeneratorTZX : public AudioGenerator
{
  public:
    AudioGeneratorTZX();
    virtual ~AudioGeneratorTZX() override;
    virtual bool begin(AudioFileSource *source, AudioOutput *out) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override;
    signed long getPlaybackTime(bool oneFiftieth=true);
    void initTrackFrame(unsigned long* tF) { trackFrame = tF; }
    void getTitle(char* title, size_t maxLen);
    void getDescription(char* description, size_t maxLen);
    void initEQBuffers(uint8_t* eqBuffer, uint8_t* channelEQBuffer);
    bool initializeFile(AudioFileSource* source, const char** message = nullptr);
    uint8_t getTotalBlocks();
    void setCurrentBlock(uint8_t block);
    uint8_t getCurrentBlock() { return currentBlock; }
    void getBlockName(uint8_t block, char* name, size_t maxLen);
    const char* getBlockType(uint8_t block);
    void setSpeed(uint8_t speed);  // Change speed dynamically during playback
    unsigned long getBlockStartTime(uint8_t block);  // Get start time of block in frames
    
  private:
    unsigned long* trackFrame = nullptr;
    
  private:
    enum State {
      STATE_HEADER,
      STATE_GET_ID,
      STATE_LEAD_IN,
      STATE_PILOT,
      STATE_SYNC1,
      STATE_SYNC2,
      STATE_DATA,
      STATE_PAUSE,
      STATE_PURE_TONE,
      STATE_PULSE_SEQ,
      STATE_FINAL_PAUSE,
      STATE_DONE
    };
    
    enum BlockID {
      ID10 = 0x10,
      ID11 = 0x11,
      ID12 = 0x12,
      ID13 = 0x13,
      ID14 = 0x14,
      ID20 = 0x20,
      ID21 = 0x21,
      ID22 = 0x22,
      ID30 = 0x30,
      TEOF = 0xFF
    };
    
    void generatePulse(uint16_t durationUs);
    bool readHeader();
    bool processBlock();
    uint16_t readWord();
    uint32_t readLong();
    uint16_t tickToUs(uint16_t ticks);
    
    State state;
    BlockID currentID;
    
    uint16_t pilotPulse;
    uint16_t sync1Pulse;
    uint16_t sync2Pulse;
    uint16_t zeroPulse;
    uint16_t onePulse;
    uint16_t pilotCount;
    uint16_t pilotRemain;
    uint16_t pauseLength;
    uint32_t bytesToRead;
    uint8_t usedBits;
    
    uint8_t currentByte;
    uint8_t currentBit;
    uint8_t pass;
    
    uint8_t seqPulses;
    uint8_t seqRemain;
    
    uint32_t sampleRate;
    int16_t level;
    uint32_t pulseRemain;
    uint16_t pulseDuration;
    uint32_t pauseRemain;
    uint32_t leadInRemain;
    uint32_t finalPauseRemain;
    uint32_t totalSamples;
    uint32_t currentSample;
    uint32_t baseSample;
    unsigned long savedTrackFrame;
    uint32_t baseTimeUs;  // Base time in microseconds (without speed multiplier)
    
    char textDescription[256];
    char groupName[256];
    uint16_t blockCount;
    uint8_t totalBlocks;
    uint8_t currentBlock;
    bool hasText;
    
    uint8_t* eqBuffer;
    uint8_t* channelEQBuffer;
    uint8_t signalLevel;
    uint8_t speedMultiplier = 1;
    volatile bool stopping = false;
    bool blockAlreadyRead = false;
    int16_t lastSample[2];
};

#endif
