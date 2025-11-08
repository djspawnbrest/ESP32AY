/*
  AudioGeneratorWAV
  Audio output generator that reads 8 and 16-bit WAV files
    
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

#ifndef _AUDIOGENERATORWAV_H
#define _AUDIOGENERATORWAV_H

#include "AudioGenerator.h"
#include "arduinoFFT.h"

class AudioGeneratorWAV : public AudioGenerator
{
  public:
    AudioGeneratorWAV();
    virtual ~AudioGeneratorWAV() override;
    virtual bool begin(AudioFileSource *source, AudioOutput *output) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override;
    void SetBufferSize(int sz) { buffSize = sz; }

    bool initializeFile(AudioFileSource *source);
    signed long getPlaybackTime(bool oneFiftieth=true);
    void initTrackFrame(unsigned long* tF);
    void getTitle(char* title, size_t maxLen);
    void getDescription(char* description, size_t maxLen);
    int getBitrate();
    int getChannelMode();
    void initEQBuffers(uint8_t* eqBuffer, uint8_t* channelEQBuffer);
    void setSpeed(int speed); // 0=slow(0.5x), 1=normal(1x), 2=fast(2x)

  private:
    bool ReadU32(uint32_t *dest) { return file->read(reinterpret_cast<uint8_t*>(dest), 4); }
    bool ReadU16(uint16_t *dest) { return file->read(reinterpret_cast<uint8_t*>(dest), 2); }
    bool ReadU8(uint8_t *dest) { return file->read(reinterpret_cast<uint8_t*>(dest), 1); }
    bool GetBufferedData(int bytes, void *dest);
    bool ReadWAVInfo();

    
  protected:
    // WAV info
    uint16_t channels;
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    
    uint32_t availBytes;

    // We need to buffer some data in-RAM to avoid doing 1000s of small reads
    uint32_t buffSize;
    uint8_t *buff;
    uint16_t buffPtr;
    uint16_t buffLen;
    
    // Track frame for current position
    unsigned long* trackFrame = nullptr;
    bool trackFrameInitialized = false;
    uint32_t samplesPlayed = 0;
    
    // Playback speed control
    int playbackSpeed = 1; // 0=0.5x, 1=1x, 2=2x
    int speedCounter = 0;
    
    // Metadata
    char wavTitle[128] = {0};
    char wavArtist[128] = {0};
    
    // EQ buffers
    uint8_t* eqBuffer = nullptr;
    uint8_t* channelEQBuffer = nullptr;
    
    // Spectrum analyzer buffers (separate for L/R)
    double vRealL[128];
    double vImagL[128];
    double vRealR[128];
    double vImagR[128];
    uint8_t fftPos = 0;
    uint16_t sampleSkip = 0;
    ArduinoFFT<double> *FFTL = nullptr;
    ArduinoFFT<double> *FFTR = nullptr;
};


#endif

