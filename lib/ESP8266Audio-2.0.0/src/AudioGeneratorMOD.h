/*
  AudioGeneratorMOD
  Audio output generator that plays Amiga MOD tracker files
    
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _AUDIOGENERATORMOD_H
#define _AUDIOGENERATORMOD_H

#include "AudioGenerator.h"
#include <set>

class AudioGeneratorMOD:public AudioGenerator{
  public:
    AudioGeneratorMOD();
    virtual ~AudioGeneratorMOD() override;
    bool initializeFile(AudioFileSource *source);
    virtual bool begin(AudioFileSource *source,AudioOutput *output) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override {return running;}
    bool SetSampleRate(int hz){if(running||(hz<20000)||(hz>96000)) return false; sampleRate=hz; return true;}
    #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
    bool SetBufferSize(int sz){if(running||(sz<1)) return false; fatBufferSize=sz; return true;}
    #endif
    bool SetStereoSeparation(int sep){if(running||(sep<0)||(sep>64)) return false; stereoSeparation=sep; return true;}
    bool SetPAL(bool use){if(running) return false; usePAL=use; return true;}
    // Aditional methods
    void getTitle(char* lfn,size_t maxLen);
    void getDescription(char* description,size_t maxLen);
    signed long getPlaybackTime(bool oneFiftieth=true);
    void setPause(bool pause);
    void setSpeed(uint8_t);
    uint8_t getNumberOfChannels();
    void initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer); // EQ buffer is 96 elements max, channel EQ buffer is 8 elements max
    void initTrackFrame(unsigned long* tF);
    void SetSeparation(int sep);
	
  public:
    bool isPaused;

  protected:
    bool LoadMOD();
    bool LoadHeader();
    void GetSample(int16_t sample[2]);
    bool RunPlayer();
    void LoadSamples();
    bool LoadPattern(uint8_t pattern);
    bool ProcessTick();
    bool ProcessRow();
    void Tremolo(uint8_t channel);
    void Portamento(uint8_t channel);
    void Vibrato(uint8_t channel);
    // Helper additional methods
	  void removeExtraSpaces(char* str);
    int getNoteIndex(uint16_t value);
    float calcRow();
    void freeFatBuffer();

  protected:
    int mixerTick;
    enum {BITDEPTH=16};
    int sampleRate;
    int samplerateOriginal;
    int bpmOriginal;
    #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
    int fatBufferSize; //(6*1024) // File system buffers per-CHANNEL (i.e. total mem required is 4 * FATBUFFERSIZE)
    #endif
    enum {FIXED_DIVIDER=10};             // Fixed-point mantissa used for integer arithmetic
    int stereoSeparation; //STEREOSEPARATION=32;    // 0 (max) to 64 (mono)
    bool usePAL;
    uint8_t* eqBuffer=nullptr; // Pointer to the equalizer buffer
    uint8_t* channelEQBuffer=nullptr; // Pointer to the equalizer channel buffer
    unsigned long* trackFrame;
	  bool buffersInitialized=false; // Equalizer buffers initialization flag
    bool trackFrameInitialized=false; // Track frame initialization flag
    bool running=false;
    bool oldFormat=false;
    volatile bool stopping=false;
    volatile bool bufferFreed=false;
    
    // Hz=7093789 / (amigaPeriod * 2) for PAL
    // Hz=7159091 / (amigaPeriod * 2) for NTSC
    int AMIGA;
    void UpdateAmiga(){AMIGA=((usePAL?7159091:7093789)/2/sampleRate<<FIXED_DIVIDER);}
 
#ifdef ESP8266 // Not sure if C3/C2 have RAM constraints, maybe add them here?
    // support max 4 channels
    enum{ROWS=64,SAMPLES=31,CHANNELS=4,NONOTE=0xFFFF,NONOTE8=0xff};
#elif defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
    // support max 8 channels
    enum{ROWS=64,SAMPLES=31,CHANNELS=32,NONOTE=0xFFFF,NONOTE8=0xff};
#else
    // support max 8 channels
    enum{ROWS=64,SAMPLES=31,CHANNELS=16,NONOTE=0xFFFF,NONOTE8=0xff};
#endif

    typedef struct Sample{
      uint16_t length;
      int8_t fineTune;
      uint8_t volume;
      uint16_t loopBegin;
      uint16_t loopLength;
      #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
        // keys For PSRAM
        uint8_t* data=nullptr;
        bool     isAllocated=false;
      #endif
    }Sample;
    
    typedef struct mod{
      Sample samples[SAMPLES];
      uint8_t songLength;
      uint8_t numberOfPatterns;
      uint8_t order[128];
      uint8_t numberOfChannels;
    }mod;
    
    // Save 256 bytes by storing raw note values, unpack with macro NOTE
    typedef struct Pattern{
      uint8_t sampleNumber[ROWS][CHANNELS];
      uint8_t note8[ROWS][CHANNELS];
      uint8_t effectNumber[ROWS][CHANNELS];
      uint8_t effectParameter[ROWS][CHANNELS];
    }Pattern;
    
    typedef struct player{
      Pattern currentPattern;
      #if defined(CONFIG_IDF_TARGET_ESP32S3)&&defined(BOARD_HAS_PSRAM)
      Pattern* psramPattern;
      bool usingPsramPattern;
      #endif
      uint32_t amiga;
      uint16_t samplesPerTick;
      uint8_t speed;
      uint8_t tick;
      uint8_t row;
      uint8_t lastRow;
      uint8_t orderIndex;
      uint8_t oldOrderIndex;
      uint8_t patternDelay;
      uint8_t patternLoopCount[CHANNELS];
      uint8_t patternLoopRow[CHANNELS];
      uint8_t lastSampleNumber[CHANNELS];
      int8_t volume[CHANNELS];
      uint16_t lastNote[CHANNELS];
      uint16_t amigaPeriod[CHANNELS];
      int16_t lastAmigaPeriod[CHANNELS];
      uint16_t portamentoNote[CHANNELS];
      uint8_t portamentoSpeed[CHANNELS];
      uint8_t waveControl[CHANNELS];
      uint8_t vibratoSpeed[CHANNELS];
      uint8_t vibratoDepth[CHANNELS];
      int8_t vibratoPos[CHANNELS];
      uint8_t tremoloSpeed[CHANNELS];
      uint8_t tremoloDepth[CHANNELS];
      int8_t tremoloPos[CHANNELS];
    }player;
    
    typedef struct mixer{
      uint32_t sampleBegin[SAMPLES];
      uint32_t sampleEnd[SAMPLES];
      uint32_t sampleloopBegin[SAMPLES];
      uint16_t sampleLoopLength[SAMPLES];
      uint32_t sampleLoopEnd[SAMPLES];
      uint8_t channelSampleNumber[CHANNELS];
      uint32_t channelSampleOffset[CHANNELS];
      uint16_t channelFrequency[CHANNELS];
      uint8_t channelVolume[CHANNELS];
      uint8_t channelPanning[CHANNELS];
    }mixer;
    
    #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
    typedef struct fatBuffer{
      uint8_t *channels[CHANNELS]; // Make dynamically allocated [FATBUFFERSIZE];
      uint32_t samplePointer[CHANNELS];
      uint8_t channelSampleNumber[CHANNELS];
    }fatBuffer;
    #endif

    // for calculate mod playback time
    typedef struct calcMod{
      uint8_t row;
      int8_t nextRow;
      int8_t nextOrder;
      uint8_t channels;
      uint8_t prevOrder;
      uint8_t orderIndex;
      uint8_t orderTable[128];
      uint8_t songLength;
      uint8_t songRestart;
      uint8_t currentSpeed;
      uint8_t patternDelay;
      uint8_t patternNumber;
      uint32_t patternsOffset;
      uint8_t patternLoopCount[CHANNELS];
      uint8_t patternLoopRow[CHANNELS];
      float totalSeconds;
      float jumpSeconds;
      float currentBPM;
      bool patternBreak;
      bool patternJump;
      bool songEnd;
      bool oldMod;
    }calcMod;

    // Effects
    typedef enum{ARPEGGIO=0,PORTAMENTOUP,PORTAMENTODOWN,TONEPORTAMENTO,VIBRATO,PORTAMENTOVOLUMESLIDE,
                VIBRATOVOLUMESLIDE,TREMOLO,SETCHANNELPANNING,SETSAMPLEOFFSET,VOLUMESLIDE,JUMPTOORDER,
                SETVOLUME,BREAKPATTERNTOROW,ESUBSET,SETSPEED}EffectsValues;
    
    // 0xE subset
    typedef enum{SETFILTER=0,FINEPORTAMENTOUP,FINEPORTAMENTODOWN,GLISSANDOCONTROL,SETVIBRATOWAVEFORM,
                SETFINETUNE,PATTERNLOOP,SETTREMOLOWAVEFORM,SUBEFFECT8,RETRIGGERNOTE,FINEVOLUMESLIDEUP,
                FINEVOLUMESLIDEDOWN,NOTECUT,NOTEDELAY,PATTERNDELAY,INVERTLOOP}Effect08Subvalues;
    
    // Our state lives here...
    player Player;
    mod Mod;
    mixer Mixer;
    #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
    fatBuffer FatBuffer;
    #endif
    calcMod Calc;
};

#endif
