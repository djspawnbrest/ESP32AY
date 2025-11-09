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


#include "AudioGeneratorWAV.h"

AudioGeneratorWAV::AudioGeneratorWAV()
{
  running = false;
  file = NULL;
  output = NULL;
  buffSize = 128;
  buff = NULL;
  buffPtr = 0;
  buffLen = 0;
}

AudioGeneratorWAV::~AudioGeneratorWAV()
{
  free(buff);
  buff = NULL;
  if(FFTL) { delete FFTL; FFTL = nullptr; }
  if(FFTR) { delete FFTR; FFTR = nullptr; }
}

bool AudioGeneratorWAV::stop()
{
  if (!running) return true;
  running = false;
  free(buff);
  buff = NULL;
  output->stop();
  return file->close();
}

bool AudioGeneratorWAV::isRunning()
{
  return running;
}


// Handle buffered reading, reload each time we run out of data
bool AudioGeneratorWAV::GetBufferedData(int bytes, void *dest)
{
  if (!running) return false; // Nothing to do here!
  uint8_t *p = reinterpret_cast<uint8_t*>(dest);
  while (bytes--) {
    // Potentially load next batch of data...
    if (buffPtr >= buffLen) {
      buffPtr = 0;
      uint32_t toRead = availBytes > buffSize ? buffSize : availBytes;
      buffLen = file->read( buff, toRead );
      availBytes -= buffLen;
    }
    if (buffPtr >= buffLen)
      return false; // No data left!
    *(p++) = buff[buffPtr++];
  }
  return true;
}

bool AudioGeneratorWAV::loop()
{
  if (!running) goto done; // Nothing to do here!

  // First, try and push in the stored sample.  If we can't, then punt and try later
  if (!output->ConsumeSample(lastSample)) goto done; // Can't send, but no error detected

  // Try and stuff the buffer one sample at a time
  do
  {
    // Speed control: read samples based on speed
    bool shouldReadSample = true;
    if(playbackSpeed == 2) {
      // Fast: skip every other sample
      shouldReadSample = true;
    } else if(playbackSpeed == 0) {
      // Slow: repeat samples
      if(speedCounter > 0) {
        shouldReadSample = false;
        speedCounter--;
      } else {
        shouldReadSample = true;
        speedCounter = 1; // Repeat once
      }
    }
    
    if(shouldReadSample) {
      if (bitsPerSample == 8) {
        uint8_t l, r;
        if (!GetBufferedData(1, &l)) stop();
        if (channels == 2) {
          if (!GetBufferedData(1, &r)) stop();
        } else {
          r = 0;
        }
        lastSample[AudioOutput::LEFTCHANNEL] = l;
        lastSample[AudioOutput::RIGHTCHANNEL] = r;
      } else if (bitsPerSample == 16) {
        if (!GetBufferedData(2, &lastSample[AudioOutput::LEFTCHANNEL])) stop();
        if (channels == 2) {
          if (!GetBufferedData(2, &lastSample[AudioOutput::RIGHTCHANNEL])) stop();
        } else {
          lastSample[AudioOutput::RIGHTCHANNEL] = 0;
        }
      }
      
      // For fast playback, skip next sample
      if(playbackSpeed == 2) {
        if (bitsPerSample == 8) {
          uint8_t dummy;
          GetBufferedData(1, &dummy);
          if (channels == 2) GetBufferedData(1, &dummy);
        } else if (bitsPerSample == 16) {
          int16_t dummy;
          GetBufferedData(2, &dummy);
          if (channels == 2) GetBufferedData(2, &dummy);
        }
      }
    }
    
    // Update track frame (current position in 1/50th seconds)
    if(trackFrameInitialized && sampleRate > 0) {
      samplesPlayed++;
      static float frameAccumulator = 0.0f;
      float speedMultiplier = (playbackSpeed == 2) ? 2.0f : (playbackSpeed == 0) ? 0.5f : 1.0f;
      frameAccumulator += (50.0f / (float)sampleRate) * speedMultiplier;
      if(frameAccumulator >= 1.0f) {
        int framesToAdd = (int)frameAccumulator;
        (*trackFrame) += framesToAdd;
        frameAccumulator -= framesToAdd;
      }
    }
    
    // Update EQ buffers
    if(channelEQBuffer) {
      int16_t absL = abs(lastSample[AudioOutput::LEFTCHANNEL]);
      int16_t absR = abs(lastSample[AudioOutput::RIGHTCHANNEL]);
      channelEQBuffer[0] = (absL >> 9) & 0x3F;
      channelEQBuffer[1] = (channels == 1) ? channelEQBuffer[0] : ((absR >> 9) & 0x3F);
    }
    
    // Update spectrum analyzer (96 bands) - separate L/R channels
    if(eqBuffer && FFTL && FFTR) {
      // Collect samples (skip every 32nd sample to reduce load)
      if(++sampleSkip >= 32) {
        sampleSkip = 0;
        vRealL[fftPos] = lastSample[AudioOutput::LEFTCHANNEL];
        vImagL[fftPos] = 0;
        vRealR[fftPos] = lastSample[AudioOutput::RIGHTCHANNEL];
        vImagR[fftPos] = 0;
        fftPos++;
        
        if(fftPos >= 128) {
          fftPos = 0;
          
          // Process LEFT channel (even indices: 0,2,4...)
          FFTL->windowing(FFTWindow::Hamming, FFTDirection::Forward);
          FFTL->compute(FFTDirection::Forward);
          FFTL->complexToMagnitude();
          
          float freqResolution = sampleRate / 128.0;
          
          for(int band = 0; band < 48; band++) {
            float freqMin, freqMax;
            if(band < 8) {
              freqMin = 20.0 * pow(1.35, band);
              freqMax = 20.0 * pow(1.35, band + 1);
            } else if(band < 28) {
              freqMin = 250.0 + ((band - 8) * 87.5);
              freqMax = 250.0 + ((band - 7) * 87.5);
            } else {
              freqMin = 2000.0 + ((band - 28) * 900.0);
              freqMax = 2000.0 + ((band - 27) * 900.0);
            }
            
            int binMin = (int)(freqMin / freqResolution);
            int binMax = (int)(freqMax / freqResolution);
            if(binMin >= 64) binMin = 63;
            if(binMax >= 64) binMax = 63;
            if(binMax < binMin) binMax = binMin;
            
            double sum = 0;
            for(int bin = binMin; bin <= binMax; bin++) sum += vRealL[bin];
            sum /= (binMax - binMin + 1);
            
            int val = (int)(sum / 2048.0) & 0x1F;
            if(val > eqBuffer[band * 2]) eqBuffer[band * 2] = val;
          }
          
          // Process RIGHT channel (odd indices: 1,3,5...)
          FFTR->windowing(FFTWindow::Hamming, FFTDirection::Forward);
          FFTR->compute(FFTDirection::Forward);
          FFTR->complexToMagnitude();
          
          for(int band = 0; band < 48; band++) {
            float freqMin, freqMax;
            if(band < 8) {
              freqMin = 20.0 * pow(1.35, band);
              freqMax = 20.0 * pow(1.35, band + 1);
            } else if(band < 28) {
              freqMin = 250.0 + ((band - 8) * 87.5);
              freqMax = 250.0 + ((band - 7) * 87.5);
            } else {
              freqMin = 2000.0 + ((band - 28) * 900.0);
              freqMax = 2000.0 + ((band - 27) * 900.0);
            }
            
            int binMin = (int)(freqMin / freqResolution);
            int binMax = (int)(freqMax / freqResolution);
            if(binMin >= 64) binMin = 63;
            if(binMax >= 64) binMax = 63;
            if(binMax < binMin) binMax = binMin;
            
            double sum = 0;
            for(int bin = binMin; bin <= binMax; bin++) sum += vRealR[bin];
            sum /= (binMax - binMin + 1);
            
            int val = (int)(sum / 2048.0) & 0x1F;
            if(val > eqBuffer[band * 2 + 1]) eqBuffer[band * 2 + 1] = val;
          }
          
          // For mono: duplicate left channel to right
          if(channels == 1) {
            for(int band = 0; band < 48; band++) {
              eqBuffer[band * 2 + 1] = eqBuffer[band * 2];
            }
          }
        }
      }
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}


bool AudioGeneratorWAV::ReadWAVInfo()
{
  uint32_t u32;
  uint16_t u16;
  int toSkip;

  // WAV specification document:
  // https://www.aelius.com/njh/wavemetatools/doc/riffmci.pdf

  // Header == "RIFF"
  if (!ReadU32(&u32)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if (u32 != 0x46464952) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, invalid RIFF header\n"));
    return false;
  }

  // Skip ChunkSize
  if (!ReadU32(&u32)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };

  // Format == "WAVE"
  if (!ReadU32(&u32)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if (u32 != 0x45564157) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, invalid WAVE header\n"));
    return false;
  }

  // there might be JUNK or PAD - ignore it by continuing reading until we get to "fmt "
  while (1) {
    if (!ReadU32(&u32)) {
      Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
      return false;
    };
    if (u32 == 0x20746d66) break; // 'fmt '
  };

  // subchunk size
  if (!ReadU32(&u32)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if (u32 == 16) { toSkip = 0; }
  else if (u32 == 18) { toSkip = 18 - 16; }
  else if (u32 == 40) { toSkip = 40 - 16; }
  else {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, appears not to be standard PCM \n"));
    return false;
  } // we only do standard PCM

  // AudioFormat
  if (!ReadU16(&u16)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if (u16 != 1) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, AudioFormat appears not to be standard PCM \n"));
    return false;
  } // we only do standard PCM

  // NumChannels
  if (!ReadU16(&channels)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if ((channels<1) || (channels>2)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, only mono and stereo are supported \n"));
    return false;
  } // Mono or stereo support only

  // SampleRate
  if (!ReadU32(&sampleRate)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if (sampleRate < 1) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, unknown sample rate \n"));
    return false;
  }  // Weird rate, punt.  Will need to check w/DAC to see if supported

  // Ignore byterate and blockalign
  if (!ReadU32(&u32)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if (!ReadU16(&u16)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };

  // Bits per sample
  if (!ReadU16(&bitsPerSample)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
    return false;
  };
  if ((bitsPerSample!=8) && (bitsPerSample != 16)) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, only 8 or 16 bits is supported \n"));
    return false;
  }  // Only 8 or 16 bits

  // Skip any extra header
  while (toSkip) {
    uint8_t ign;
    if (!ReadU8(&ign)) {
      Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: failed to read WAV data\n"));
      return false;
    };
    toSkip--;
  }

  // look for data subchunk and read metadata
  bool foundData = false;
  uint32_t dataStartPos = 0;
  
  do {
    if (!ReadU32(&u32)) break;
    
    if (u32 == 0x61746164) { // "data"
      if (!ReadU32(&availBytes)) return false;
      dataStartPos = file->getPos(); // Save position right after data size
      foundData = true;
      // Skip data chunk to continue reading metadata after it
      file->seek(availBytes, SEEK_CUR);
      if (availBytes & 1) file->seek(1, SEEK_CUR);
      continue;
    }
    
    // Read chunk size
    uint32_t chunkSize;
    if (!ReadU32(&chunkSize)) break;
    
    // Check for LIST chunk (metadata)
    if (u32 == 0x5453494C) { // "LIST"
      uint32_t listType;
      if (!ReadU32(&listType)) break;
      
      if (listType == 0x4F464E49) { // "INFO"
        uint32_t bytesRead = 4;
        while (bytesRead < chunkSize) {
          uint32_t subId, subSize;
          if (!ReadU32(&subId)) break;
          if (!ReadU32(&subSize)) break;
          bytesRead += 8;
          
          if (subId == 0x4D414E49 && subSize > 0) { // "INAM"
            uint8_t tempBuf[128];
            uint32_t readSize = (subSize < sizeof(tempBuf)-1) ? subSize : sizeof(tempBuf)-1;
            file->read(tempBuf, readSize);
            tempBuf[readSize] = '\0';
            // Convert Windows-1251 to UTF-8
            uint32_t outPos = 0;
            for(uint32_t i = 0; i < readSize && tempBuf[i] && outPos < sizeof(wavTitle)-3; i++) {
              uint8_t c = tempBuf[i];
              if(c >= 0xC0 && c <= 0xEF) { wavTitle[outPos++] = 0xD0; wavTitle[outPos++] = c - 0x30; }
              else if(c >= 0xF0) { wavTitle[outPos++] = 0xD1; wavTitle[outPos++] = c - 0x70; }
              else if(c == 0xA8) { wavTitle[outPos++] = 0xD0; wavTitle[outPos++] = 0x81; }
              else if(c == 0xB8) { wavTitle[outPos++] = 0xD1; wavTitle[outPos++] = 0x91; }
              else wavTitle[outPos++] = c;
            }
            wavTitle[outPos] = '\0';
            if (subSize > readSize) file->seek(subSize - readSize, SEEK_CUR);
          } else if (subId == 0x54524149 && subSize > 0) { // "IART"
            uint8_t tempBuf[128];
            uint32_t readSize = (subSize < sizeof(tempBuf)-1) ? subSize : sizeof(tempBuf)-1;
            file->read(tempBuf, readSize);
            tempBuf[readSize] = '\0';
            // Convert Windows-1251 to UTF-8
            uint32_t outPos = 0;
            for(uint32_t i = 0; i < readSize && tempBuf[i] && outPos < sizeof(wavArtist)-3; i++) {
              uint8_t c = tempBuf[i];
              if(c >= 0xC0 && c <= 0xEF) { wavArtist[outPos++] = 0xD0; wavArtist[outPos++] = c - 0x30; }
              else if(c >= 0xF0) { wavArtist[outPos++] = 0xD1; wavArtist[outPos++] = c - 0x70; }
              else if(c == 0xA8) { wavArtist[outPos++] = 0xD0; wavArtist[outPos++] = 0x81; }
              else if(c == 0xB8) { wavArtist[outPos++] = 0xD1; wavArtist[outPos++] = 0x91; }
              else wavArtist[outPos++] = c;
            }
            wavArtist[outPos] = '\0';
            if (subSize > readSize) file->seek(subSize - readSize, SEEK_CUR);
          } else {
            file->seek(subSize, SEEK_CUR);
          }
          bytesRead += subSize;
          if (subSize & 1) { file->seek(1, SEEK_CUR); bytesRead++; }
        }
      } else {
        file->seek(chunkSize - 4, SEEK_CUR);
      }
    } else {
      // Skip other chunks
      file->seek(chunkSize, SEEK_CUR);
    }
    if (chunkSize & 1) file->seek(1, SEEK_CUR);
  } while (1);
  
  if (!foundData) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: data chunk not found\n"));
    return false;
  }
  
  // Seek back to start of data for playback
  file->seek(dataStartPos, SEEK_SET);

  // Set up buffer
  if (!buff) {
    buff = reinterpret_cast<uint8_t *>(malloc(buffSize));
    if (!buff) {
      Serial.printf_P(PSTR("AudioGeneratorWAV::ReadWAVInfo: cannot read WAV, failed to set up buffer \n"));
      return false;
    }
  }
  buffPtr = 0;
  buffLen = 0;

  return true;
}

bool AudioGeneratorWAV::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: failed: invalid source\n"));
    return false;
  }
  file = source;
  if (!output) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: invalid output\n"));
    return false;
  }
  this->output = output;
  if (!file->isOpen()) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: file not open\n"));
    return false;
  } // Error

  // If not already initialized, read WAV info
  if (availBytes == 0) {
    if (!ReadWAVInfo()) {
      Serial.printf_P(PSTR("AudioGeneratorWAV::begin: failed during ReadWAVInfo\n"));
      return false;
    }
  } else {
    // Already initialized, just seek to data and allocate buffer
    file->seek(0, SEEK_SET);
    if (!ReadWAVInfo()) {
      Serial.printf_P(PSTR("AudioGeneratorWAV::begin: failed during ReadWAVInfo\n"));
      return false;
    }
  }

  if (!output->SetRate( sampleRate )) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: failed to SetRate in output\n"));
    return false;
  }
  if (!output->SetBitsPerSample( bitsPerSample )) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: failed to SetBitsPerSample in output\n"));
    return false;
  }
  if (!output->SetChannels( channels )) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: failed to SetChannels in output\n"));
    return false;
  }
  if (!output->begin()) {
    Serial.printf_P(PSTR("AudioGeneratorWAV::begin: output's begin did not return true\n"));
    return false;
  }

  running = true;

  return true;
}

bool AudioGeneratorWAV::initializeFile(AudioFileSource *source){
	if(!source) return false;
	file = source;
	if(!ReadWAVInfo()) return false;
	file->seek(0, SEEK_SET);
	return true;
}

signed long AudioGeneratorWAV::getPlaybackTime(bool oneFiftieth) {
  if(!file || !file->isOpen() || sampleRate == 0) return -1;
  // Calculate total duration based on data size
  uint32_t totalSamples = availBytes / (channels * (bitsPerSample / 8));
  float seconds = (float)totalSamples / (float)sampleRate;
  return (signed long)(seconds * (oneFiftieth ? 50 : 1000));
}

void AudioGeneratorWAV::initTrackFrame(unsigned long* tF) {
  if(tF != nullptr) {
    trackFrame = tF;
    *trackFrame = 0;
    samplesPlayed = 0;
    trackFrameInitialized = true;
  }
}

void AudioGeneratorWAV::getTitle(char* title, size_t maxLen) {
  if(title) {
    if(wavTitle[0] != '\0') {
      strncpy(title, wavTitle, maxLen-1);
      title[maxLen-1] = '\0';
    } else {
      title[0] = '\0';
    }
  }
}

void AudioGeneratorWAV::getDescription(char* description, size_t maxLen) {
  if(description) {
    if(wavArtist[0] != '\0') {
      strncpy(description, wavArtist, maxLen-1);
      description[maxLen-1] = '\0';
    } else {
      description[0] = '\0';
    }
  }
}

int AudioGeneratorWAV::getBitrate() {
  if(sampleRate == 0) return 0;
  return (sampleRate * channels * bitsPerSample) / 1000; // kbps
}

int AudioGeneratorWAV::getChannelMode() {
  return (channels == 2) ? 0 : 3; // 0=stereo, 3=mono
}

void AudioGeneratorWAV::initEQBuffers(uint8_t* eqBuf, uint8_t* channelEQBuf) {
  eqBuffer = eqBuf;
  channelEQBuffer = channelEQBuf;
  if(eqBuffer) memset(eqBuffer, 0, 96);
  if(channelEQBuffer) memset(channelEQBuffer, 0, 8);
  
  // Initialize FFT objects
  if(!FFTL) FFTL = new ArduinoFFT<double>(vRealL, vImagL, 128, sampleRate > 0 ? sampleRate : 44100);
  if(!FFTR) FFTR = new ArduinoFFT<double>(vRealR, vImagR, 128, sampleRate > 0 ? sampleRate : 44100);
  
  fftPos = 0;
  sampleSkip = 0;
}

void AudioGeneratorWAV::setSpeed(int speed) {
  if(speed >= 0 && speed <= 2) {
    playbackSpeed = speed;
    speedCounter = 0;
  }
}

