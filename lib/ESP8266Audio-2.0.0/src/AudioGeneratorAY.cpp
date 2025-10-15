/*
  AudioGeneratorAY - AY-3-8910/YM-2149 Emulator by Spawn
  based on emu2149 library
  
  Copyright (C) 2025 by Spawn
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#include "AudioGeneratorAY.h"
#include <string.h>
#include <Arduino.h>

#define GETA_BITS 24

AudioGeneratorAY *AudioGeneratorAY::instance = NULL;

static const uint8_t regmsk[16] = {
  0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0x3f, 
  0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff
};

AudioGeneratorAY::AudioGeneratorAY()
{
  sampleRate = 44100;
  numChips = 0;
  output = NULL;
  running = false;
  ayTimer = NULL;
  timerMode = false;
  instance = this;
  beeperSample = 0;
  beeperActive = false;
  
#ifdef USE_HQ_RESAMPLER
  resampleBuf[0] = resampleBuf[1] = 0;
  resampleAccum = 0;
#endif
  
  voltbl[0][0] = 0x00; voltbl[0][1] = 0x01; voltbl[0][2] = 0x01; voltbl[0][3] = 0x02;
  voltbl[0][4] = 0x02; voltbl[0][5] = 0x03; voltbl[0][6] = 0x03; voltbl[0][7] = 0x04;
  voltbl[0][8] = 0x05; voltbl[0][9] = 0x06; voltbl[0][10] = 0x07; voltbl[0][11] = 0x09;
  voltbl[0][12] = 0x0B; voltbl[0][13] = 0x0D; voltbl[0][14] = 0x0F; voltbl[0][15] = 0x12;
  voltbl[0][16] = 0x16; voltbl[0][17] = 0x1A; voltbl[0][18] = 0x1F; voltbl[0][19] = 0x25;
  voltbl[0][20] = 0x2D; voltbl[0][21] = 0x35; voltbl[0][22] = 0x3F; voltbl[0][23] = 0x4C;
  voltbl[0][24] = 0x5A; voltbl[0][25] = 0x6A; voltbl[0][26] = 0x7F; voltbl[0][27] = 0x97;
  voltbl[0][28] = 0xB4; voltbl[0][29] = 0xD6; voltbl[0][30] = 0xFF; voltbl[0][31] = 0xFF;
  
  voltbl[1][0] = 0x00; voltbl[1][1] = 0x00; voltbl[1][2] = 0x03; voltbl[1][3] = 0x03;
  voltbl[1][4] = 0x04; voltbl[1][5] = 0x04; voltbl[1][6] = 0x06; voltbl[1][7] = 0x06;
  voltbl[1][8] = 0x09; voltbl[1][9] = 0x09; voltbl[1][10] = 0x0D; voltbl[1][11] = 0x0D;
  voltbl[1][12] = 0x12; voltbl[1][13] = 0x12; voltbl[1][14] = 0x1D; voltbl[1][15] = 0x1D;
  voltbl[1][16] = 0x22; voltbl[1][17] = 0x22; voltbl[1][18] = 0x37; voltbl[1][19] = 0x37;
  voltbl[1][20] = 0x4D; voltbl[1][21] = 0x4D; voltbl[1][22] = 0x62; voltbl[1][23] = 0x62;
  voltbl[1][24] = 0x82; voltbl[1][25] = 0x82; voltbl[1][26] = 0xA6; voltbl[1][27] = 0xA6;
  voltbl[1][28] = 0xD0; voltbl[1][29] = 0xD0; voltbl[1][30] = 0xFF; voltbl[1][31] = 0xFF;
}

AudioGeneratorAY::~AudioGeneratorAY()
{
  stopTimer();
  for (uint8_t i = 0; i < numChips; i++) {
    PSG_delete(&chips[i]);
  }
}

void AudioGeneratorAY::internal_refresh(PSG *psg)
{
  uint32_t f_master = psg->clk;
  if (psg->clk_div) f_master /= 2;
  
  if (psg->quality) {
    psg->base_incr = 1 << GETA_BITS;
    psg->realstep = f_master;
    psg->psgstep = psg->rate * 8;
    psg->psgtime = 0;
    psg->freq_limit = (uint32_t)(f_master / 16 / (psg->rate / 2));
  } else {
    psg->base_incr = (uint32_t)((double)f_master * (1 << GETA_BITS) / 8 / psg->rate);
    psg->freq_limit = 0;
  }
}

void AudioGeneratorAY::PSG_init(PSG *psg, uint32_t clk, uint32_t rate)
{
  memset(psg, 0, sizeof(PSG));
  psg->clk = clk;
  psg->rate = rate;
#ifdef USE_HQ_RESAMPLER
  psg->quality = 0;
#else
  psg->quality = 1;
#endif
  psg->clk_div = 0;
  internal_refresh(psg);
}

void AudioGeneratorAY::PSG_reset(PSG *psg)
{
  psg->base_count = 0;
  for (int i = 0; i < 3; i++) {
    psg->count[i] = 0;
    psg->freq[i] = 0;
    psg->edge[i] = 0;
    psg->volume[i] = 0;
    psg->ch_out[i] = 0;
  }
  psg->mask = 0;
  for (int i = 0; i < 16; i++) psg->reg[i] = 0;
  psg->adr = 0;
  psg->noise_seed = 0xffff;
  psg->noise_scaler = 0;
  psg->noise_count = 0;
  psg->noise_freq = 0;
  psg->env_ptr = 0;
  psg->env_freq = 0;
  psg->env_count = 0;
  psg->env_pause = 1;
  psg->out = 0;
}

void AudioGeneratorAY::PSG_delete(PSG *psg)
{
  // Nothing to free
}

void AudioGeneratorAY::PSG_setVolumeMode(PSG *psg, int type)
{
  psg->voltbl = (type == 2) ? voltbl[1] : voltbl[0];
}

void AudioGeneratorAY::PSG_setClock(PSG *psg, uint32_t clk)
{
  if (psg->clk != clk) {
    psg->clk = clk;
    internal_refresh(psg);
  }
}

void AudioGeneratorAY::PSG_setRate(PSG *psg, uint32_t rate)
{
  if (psg->rate != rate) {
    psg->rate = rate;
    internal_refresh(psg);
  }
}

void AudioGeneratorAY::update_output(PSG *psg)
{
  int i, noise;
  uint8_t incr;
  
  psg->base_count += psg->base_incr;
  incr = (psg->base_count >> GETA_BITS);
  psg->base_count &= (1 << GETA_BITS) - 1;
  
  psg->env_count += incr;
  if (psg->env_count >= psg->env_freq) {
    if (!psg->env_pause) {
      if (psg->env_face)
        psg->env_ptr = (psg->env_ptr + 1) & 0x3f;
      else
        psg->env_ptr = (psg->env_ptr + 0x3f) & 0x3f;
    }
    
    if (psg->env_ptr & 0x20) {
      if (psg->env_continue) {
        if (psg->env_alternate ^ psg->env_hold) psg->env_face ^= 1;
        if (psg->env_hold) psg->env_pause = 1;
        psg->env_ptr = psg->env_face ? 0 : 0x1f;
      } else {
        psg->env_pause = 1;
        psg->env_ptr = 0;
      }
    }
    
    if (psg->env_freq >= incr)
      psg->env_count -= psg->env_freq;
    else
      psg->env_count = 0;
  }
  
  psg->noise_count += incr;
  if (psg->noise_count >= psg->noise_freq) {
    psg->noise_scaler ^= 1;
    if (psg->noise_scaler) {
      if (psg->noise_seed & 1)
        psg->noise_seed ^= 0x24000;
      psg->noise_seed >>= 1;
    }
    if (psg->noise_freq >= incr)
      psg->noise_count -= psg->noise_freq;
    else
      psg->noise_count = 0;
  }
  noise = psg->noise_seed & 1;
  
  for (i = 0; i < 3; i++) {
    psg->count[i] += incr;
    if (psg->count[i] >= psg->freq[i]) {
      psg->edge[i] = !psg->edge[i];
      if (psg->freq[i] >= incr)
        psg->count[i] -= psg->freq[i];
      else
        psg->count[i] = 0;
    }
    
    if (0 < psg->freq_limit && psg->freq[i] <= psg->freq_limit && psg->nmask[i])
      continue;
    
    if (psg->mask & (1 << i)) {
      psg->ch_out[i] = 0;
      continue;
    }
    
    if ((psg->tmask[i] || psg->edge[i]) && (psg->nmask[i] || noise)) {
      if (!(psg->volume[i] & 32))
        psg->ch_out[i] = (psg->voltbl[psg->volume[i] & 31] << GAIN);
      else
        psg->ch_out[i] = (psg->voltbl[psg->env_ptr] << GAIN);
    } else {
      psg->ch_out[i] = 0;
    }
  }
}

int16_t AudioGeneratorAY::PSG_calc(PSG *psg)
{
  if (!psg->quality) {
    update_output(psg);
    psg->out = psg->ch_out[0] + psg->ch_out[1] + psg->ch_out[2];
  } else {
    while (psg->realstep > psg->psgtime) {
      psg->psgtime += psg->psgstep;
      update_output(psg);
      psg->out += psg->ch_out[0] + psg->ch_out[1] + psg->ch_out[2];
      psg->out >>= 1;
    }
    psg->psgtime -= psg->realstep;
  }
  return psg->out;
}

void AudioGeneratorAY::PSG_writeReg(PSG *psg, uint32_t reg, uint32_t val)
{
  int c;
  if (reg > 15) return;
  
  val &= regmsk[reg];
  psg->reg[reg] = (uint8_t)val;
  
  switch (reg) {
    case 0: case 2: case 4:
    case 1: case 3: case 5:
      c = reg >> 1;
      psg->freq[c] = ((psg->reg[c * 2 + 1] & 15) << 8) + psg->reg[c * 2];
      break;
    case 6:
      psg->noise_freq = val & 31;
      break;
    case 7:
      psg->tmask[0] = (val & 1);
      psg->tmask[1] = (val & 2);
      psg->tmask[2] = (val & 4);
      psg->nmask[0] = (val & 8);
      psg->nmask[1] = (val & 16);
      psg->nmask[2] = (val & 32);
      break;
    case 8: case 9: case 10:
      psg->volume[reg - 8] = val << 1;
      break;
    case 11: case 12:
      psg->env_freq = (psg->reg[12] << 8) + psg->reg[11];
      break;
    case 13:
      psg->env_continue = (val >> 3) & 1;
      psg->env_attack = (val >> 2) & 1;
      psg->env_alternate = (val >> 1) & 1;
      psg->env_hold = val & 1;
      psg->env_face = psg->env_attack;
      psg->env_pause = 0;
      psg->env_ptr = psg->env_face ? 0 : 0x1f;
      break;
  }
}

void AudioGeneratorAY::ayInit(uint8_t numChips, AY_CHIP_TYPE type, AY_LAYER_MODE layer)
{
  this->numChips = (numChips > AY_MAX_CHIPS) ? AY_MAX_CHIPS : numChips;
  
  for (uint8_t i = 0; i < this->numChips; i++) {
    PSG_init(&chips[i], AY_CHIP_CLOCK, sampleRate);
    PSG_setVolumeMode(&chips[i], type);
    PSG_reset(&chips[i]);
    layerModes[i] = layer;
  }
}

bool AudioGeneratorAY::begin(AudioOutput *output)
{
  if (!output) return false;
  this->output = output;
  
  if (!output->SetRate(sampleRate)) return false;
  if (!output->SetBitsPerSample(16)) return false;
  if (!output->SetChannels(2)) return false;
  if (!output->begin()) return false;
  
  running = true;
  return true;
}

bool AudioGeneratorAY::loop()
{
  if (!running) return false;
  
  if (!output->ConsumeSample(lastSample)) goto done;
  
  do {
    int32_t mixL = 0, mixR = 0;
    
#ifdef USE_HQ_RESAMPLER
    uint32_t highRate = chips[0].clk / 8;
    uint32_t ratio = (highRate + sampleRate / 2) / sampleRate;
    int32_t sumL = 0, sumR = 0;
    
    for (uint32_t r = 0; r < ratio; r++) {
      int32_t tmpL = 0, tmpR = 0;
      for (uint8_t i = 0; i < numChips; i++) {
        PSG_calc(&chips[i]);
        
        switch (layerModes[i]) {
          case AY_LAYER_ABC:
            tmpL += chips[i].ch_out[0] + (chips[i].ch_out[1] >> 1);
            tmpR += chips[i].ch_out[2] + (chips[i].ch_out[1] >> 1);
            break;
          case AY_LAYER_ACB:
            tmpL += chips[i].ch_out[0] + (chips[i].ch_out[2] >> 1);
            tmpR += chips[i].ch_out[1] + (chips[i].ch_out[2] >> 1);
            break;
          case AY_LAYER_BAC:
            tmpL += chips[i].ch_out[1] + (chips[i].ch_out[0] >> 1);
            tmpR += chips[i].ch_out[2] + (chips[i].ch_out[0] >> 1);
            break;
          case AY_LAYER_BCA:
            tmpL += chips[i].ch_out[1] + (chips[i].ch_out[2] >> 1);
            tmpR += chips[i].ch_out[0] + (chips[i].ch_out[2] >> 1);
            break;
          case AY_LAYER_CAB:
            tmpL += chips[i].ch_out[2] + (chips[i].ch_out[0] >> 1);
            tmpR += chips[i].ch_out[1] + (chips[i].ch_out[0] >> 1);
            break;
          case AY_LAYER_CBA:
            tmpL += chips[i].ch_out[2] + (chips[i].ch_out[1] >> 1);
            tmpR += chips[i].ch_out[0] + (chips[i].ch_out[1] >> 1);
            break;
        }
      }
      sumL += tmpL;
      sumR += tmpR;
    }
    
    mixL = sumL / ratio;
    mixR = sumR / ratio;
    lastSample[0] = (int16_t)(mixL > 32767 ? 32767 : (mixL < -32768 ? -32768 : mixL));
    lastSample[1] = (int16_t)(mixR > 32767 ? 32767 : (mixR < -32768 ? -32768 : mixR));
#else
    for (uint8_t i = 0; i < numChips; i++) {
      PSG_calc(&chips[i]);
      
      switch (layerModes[i]) {
        case AY_LAYER_ABC:
          mixL += chips[i].ch_out[0] + (chips[i].ch_out[1] >> 1);
          mixR += chips[i].ch_out[2] + (chips[i].ch_out[1] >> 1);
          break;
        case AY_LAYER_ACB:
          mixL += chips[i].ch_out[0] + (chips[i].ch_out[2] >> 1);
          mixR += chips[i].ch_out[1] + (chips[i].ch_out[2] >> 1);
          break;
        case AY_LAYER_BAC:
          mixL += chips[i].ch_out[1] + (chips[i].ch_out[0] >> 1);
          mixR += chips[i].ch_out[2] + (chips[i].ch_out[0] >> 1);
          break;
        case AY_LAYER_BCA:
          mixL += chips[i].ch_out[1] + (chips[i].ch_out[2] >> 1);
          mixR += chips[i].ch_out[0] + (chips[i].ch_out[2] >> 1);
          break;
        case AY_LAYER_CAB:
          mixL += chips[i].ch_out[2] + (chips[i].ch_out[0] >> 1);
          mixR += chips[i].ch_out[1] + (chips[i].ch_out[0] >> 1);
          break;
        case AY_LAYER_CBA:
          mixL += chips[i].ch_out[2] + (chips[i].ch_out[1] >> 1);
          mixR += chips[i].ch_out[0] + (chips[i].ch_out[1] >> 1);
          break;
      }
    }
    
    lastSample[0] = (int16_t)(mixL > 32767 ? 32767 : (mixL < -32768 ? -32768 : mixL));
    lastSample[1] = (int16_t)(mixR > 32767 ? 32767 : (mixR < -32768 ? -32768 : mixR));
#endif
  } while (output->ConsumeSample(lastSample));

done:  
  output->loop();
  return running;
}

bool AudioGeneratorAY::stop()
{
  if (!output) return false;
  running = false;
  output->stop();
  return true;
}

bool AudioGeneratorAY::isRunning()
{
  return running;
}

void AudioGeneratorAY::writeReg(uint8_t chip, uint8_t reg, uint8_t val)
{
  if (chip >= numChips) return;
  PSG_writeReg(&chips[chip], reg, val);
}

void AudioGeneratorAY::setChipType(uint8_t chip, AY_CHIP_TYPE type)
{
  if (chip >= numChips) return;
  PSG_setVolumeMode(&chips[chip], type);
}

void AudioGeneratorAY::setLayer(uint8_t chip, AY_LAYER_MODE layer)
{
  if (chip >= numChips) return;
  layerModes[chip] = layer;
}

void AudioGeneratorAY::setChipClock(uint8_t chip, uint32_t clock)
{
  if (chip >= numChips) return;
  PSG_setClock(&chips[chip], clock);
}

void AudioGeneratorAY::setSampleRate(uint32_t rate)
{
  sampleRate = rate;
  for (uint8_t i = 0; i < numChips; i++) {
    PSG_setRate(&chips[i], rate);
  }
}

void IRAM_ATTR AudioGeneratorAY::timerISR()
{
  if (!instance || !instance->running || !instance->output) return;
  
  int32_t mixL = 0, mixR = 0;
  
  for (uint8_t i = 0; i < instance->numChips; i++) {
    instance->PSG_calc(&instance->chips[i]);
    
    switch (instance->layerModes[i]) {
      case AY_LAYER_ABC:
        mixL += instance->chips[i].ch_out[0] + (instance->chips[i].ch_out[1] >> 1);
        mixR += instance->chips[i].ch_out[2] + (instance->chips[i].ch_out[1] >> 1);
        break;
      case AY_LAYER_ACB:
        mixL += instance->chips[i].ch_out[0] + (instance->chips[i].ch_out[2] >> 1);
        mixR += instance->chips[i].ch_out[1] + (instance->chips[i].ch_out[2] >> 1);
        break;
      case AY_LAYER_BAC:
        mixL += instance->chips[i].ch_out[1] + (instance->chips[i].ch_out[0] >> 1);
        mixR += instance->chips[i].ch_out[2] + (instance->chips[i].ch_out[0] >> 1);
        break;
      case AY_LAYER_BCA:
        mixL += instance->chips[i].ch_out[1] + (instance->chips[i].ch_out[2] >> 1);
        mixR += instance->chips[i].ch_out[0] + (instance->chips[i].ch_out[2] >> 1);
        break;
      case AY_LAYER_CAB:
        mixL += instance->chips[i].ch_out[2] + (instance->chips[i].ch_out[0] >> 1);
        mixR += instance->chips[i].ch_out[1] + (instance->chips[i].ch_out[0] >> 1);
        break;
      case AY_LAYER_CBA:
        mixL += instance->chips[i].ch_out[2] + (instance->chips[i].ch_out[1] >> 1);
        mixR += instance->chips[i].ch_out[0] + (instance->chips[i].ch_out[1] >> 1);
        break;
    }
  }
  
  if (instance->beeperActive) {
    mixL += instance->beeperSample;
    mixR += instance->beeperSample;
  }
  
  instance->lastSample[0] = (int16_t)(mixL > 32767 ? 32767 : (mixL < -32768 ? -32768 : mixL));
  instance->lastSample[1] = (int16_t)(mixR > 32767 ? 32767 : (mixR < -32768 ? -32768 : mixR));
  instance->output->ConsumeSample(instance->lastSample);
}

bool AudioGeneratorAY::beginTimer()
{
  if (ayTimer) return true;
  if (!output || !running) return false;
  
  timerMode = true;
  ayTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(ayTimer, &timerISR, true);
  timerAlarmWrite(ayTimer, 1000000 / sampleRate, true);
  timerAlarmEnable(ayTimer);
  
  return true;
}

void AudioGeneratorAY::stopTimer()
{
  if (ayTimer) {
    timerAlarmDisable(ayTimer);
    timerDetachInterrupt(ayTimer);
    timerEnd(ayTimer);
    ayTimer = NULL;
  }
  timerMode = false;
}

void AudioGeneratorAY::renderSampleWithBeeper(int16_t beeper, int16_t* outL, int16_t* outR)
{
  if (!running) return;
  
  int32_t mixL = 0, mixR = 0;
  
  for (uint8_t i = 0; i < numChips; i++) {
    PSG_calc(&chips[i]);
    
    switch (layerModes[i]) {
      case AY_LAYER_ABC:
        mixL += chips[i].ch_out[0] + (chips[i].ch_out[1] >> 1);
        mixR += chips[i].ch_out[2] + (chips[i].ch_out[1] >> 1);
        break;
      case AY_LAYER_ACB:
        mixL += chips[i].ch_out[0] + (chips[i].ch_out[2] >> 1);
        mixR += chips[i].ch_out[1] + (chips[i].ch_out[2] >> 1);
        break;
      case AY_LAYER_BAC:
        mixL += chips[i].ch_out[1] + (chips[i].ch_out[0] >> 1);
        mixR += chips[i].ch_out[2] + (chips[i].ch_out[0] >> 1);
        break;
      case AY_LAYER_BCA:
        mixL += chips[i].ch_out[1] + (chips[i].ch_out[2] >> 1);
        mixR += chips[i].ch_out[0] + (chips[i].ch_out[2] >> 1);
        break;
      case AY_LAYER_CAB:
        mixL += chips[i].ch_out[2] + (chips[i].ch_out[0] >> 1);
        mixR += chips[i].ch_out[1] + (chips[i].ch_out[0] >> 1);
        break;
      case AY_LAYER_CBA:
        mixL += chips[i].ch_out[2] + (chips[i].ch_out[1] >> 1);
        mixR += chips[i].ch_out[0] + (chips[i].ch_out[1] >> 1);
        break;
    }
  }
  
  *outL = (int16_t)(mixL + beeper);
  *outR = (int16_t)(mixR + beeper);
}
