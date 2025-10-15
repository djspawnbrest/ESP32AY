/*
  AudioGeneratorAY - AY-3-8910/YM-2149 Emulator by Spawn
  based on emu2149 library
  
  Copyright (C) 2025 by Spawn
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef _AUDIOGENERATORAY_H
#define _AUDIOGENERATORAY_H

#include "AudioGenerator.h"

#define AY_MAX_CHIPS 4
#define AY_CHIP_CLOCK 1750000

// #define GAIN 4   // тихо (×16)
#define GAIN 5   // средне-тихо (×32)
// #define GAIN 6   // средне (×64)
// #define GAIN 7   // громко (×128)
// #define GAIN 8   // очень громко (×256)

#define USE_HQ_RESAMPLER  // Раскомментируйте для максимального качества (без алиасинга)

enum AY_CHIP_TYPE {
  AY_CHIP_AY = 2,
  AY_CHIP_YM = 1
};

enum AY_LAYER_MODE {
  AY_LAYER_ABC = 0,
  AY_LAYER_ACB = 1,
  AY_LAYER_BAC = 2,
  AY_LAYER_BCA = 3,
  AY_LAYER_CAB = 4,
  AY_LAYER_CBA = 5
};

typedef struct {
  uint32_t *voltbl;
  uint8_t reg[0x20];
  int32_t out;
  uint32_t clk, rate, base_incr;
  uint8_t quality;
  uint8_t clk_div;
  uint16_t count[3];
  uint8_t volume[3];
  uint16_t freq[3];
  uint8_t edge[3];
  uint8_t tmask[3];
  uint8_t nmask[3];
  uint32_t mask;
  uint32_t base_count;
  uint8_t env_ptr;
  uint8_t env_face;
  uint8_t env_continue;
  uint8_t env_attack;
  uint8_t env_alternate;
  uint8_t env_hold;
  uint8_t env_pause;
  uint16_t env_freq;
  uint32_t env_count;
  uint32_t noise_seed;
  uint8_t noise_scaler;
  uint8_t noise_count;
  uint8_t noise_freq;
  uint32_t realstep;
  uint32_t psgtime;
  uint32_t psgstep;
  uint32_t freq_limit;
  uint8_t adr;
  int16_t ch_out[3];
} PSG;

class AudioGeneratorAY : public AudioGenerator
{
  public:
    AudioGeneratorAY();
    virtual ~AudioGeneratorAY() override;
    virtual bool begin(AudioOutput *output) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override;
    
    void ayInit(uint8_t numChips=1, AY_CHIP_TYPE type=AY_CHIP_AY, AY_LAYER_MODE layer=AY_LAYER_ABC);
    void writeReg(uint8_t chip, uint8_t reg, uint8_t val);
    void setChipType(uint8_t chip, AY_CHIP_TYPE type);
    void setLayer(uint8_t chip, AY_LAYER_MODE layer);
    void setChipClock(uint8_t chip, uint32_t clock);
    void setSampleRate(uint32_t rate);
    uint8_t getNumChips() { return numChips; }
    
    bool beginTimer();
    void stopTimer();
    static void IRAM_ATTR timerISR();
    
    void setBeeperSample(int16_t sample) { beeperSample = sample; beeperActive = true; }
    void clearBeeper() { beeperActive = false; }
    void renderSampleWithBeeper(int16_t beeper, int16_t* outL, int16_t* outR);
    
  private:
    void PSG_init(PSG *psg, uint32_t clk, uint32_t rate);
    void PSG_reset(PSG *psg);
    void PSG_delete(PSG *psg);
    void PSG_writeReg(PSG *psg, uint32_t reg, uint32_t val);
    int16_t PSG_calc(PSG *psg);
    void PSG_setVolumeMode(PSG *psg, int type);
    void PSG_setClock(PSG *psg, uint32_t clk);
    void PSG_setRate(PSG *psg, uint32_t rate);
    void internal_refresh(PSG *psg);
    void update_output(PSG *psg);
    
    uint8_t numChips;
    PSG chips[AY_MAX_CHIPS];
    AY_LAYER_MODE layerModes[AY_MAX_CHIPS];
    uint32_t sampleRate;
    int16_t lastSample[2];
    uint32_t voltbl[2][32];
    
#ifdef USE_HQ_RESAMPLER
    int16_t resampleBuf[2];
    uint32_t resampleAccum;
#endif
    
    hw_timer_t *ayTimer;
    static AudioGeneratorAY *instance;
    bool timerMode;
    
    volatile int16_t beeperSample;
    volatile bool beeperActive;
};

#endif
