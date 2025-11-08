/*
  AudioGeneratorMP3
  Wrap libmad MP3 library to play audio

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


#include "AudioGeneratorMP3.h"

AudioGeneratorMP3::AudioGeneratorMP3()
{
  running = false;
  file = NULL;
  output = NULL;
  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;
  nsCountMax = 1152/32;
  madInitted = false;
  FFTL = new ArduinoFFT<double>(vRealL, vImagL, 128, 44100);
  FFTR = new ArduinoFFT<double>(vRealR, vImagR, 128, 44100);
}

AudioGeneratorMP3::AudioGeneratorMP3(void *space, int size): preallocateSpace(space), preallocateSize(size)
{
  running = false;
  file = NULL;
  output = NULL;
  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;
  nsCountMax = 1152/32;
  madInitted = false;
}

AudioGeneratorMP3::AudioGeneratorMP3(void *buff, int buffSize, void *stream, int streamSize, void *frame, int frameSize, void *synth, int synthSize):
    preallocateSpace(buff), preallocateSize(buffSize),
    preallocateStreamSpace(stream), preallocateStreamSize(streamSize),
    preallocateFrameSpace(frame), preallocateFrameSize(frameSize),
    preallocateSynthSpace(synth), preallocateSynthSize(synthSize)
{
  running = false;
  file = NULL;
  output = NULL;
  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;
  nsCountMax = 1152/32;
  madInitted = false;
}

AudioGeneratorMP3::~AudioGeneratorMP3()
{
  if (!preallocateSpace) {
    free(buff);
    free(synth);
    free(frame);
    free(stream);
  }
  if(FFTL) delete FFTL;
  if(FFTR) delete FFTR;
}


bool AudioGeneratorMP3::stop()
{
  if (madInitted) {
    mad_synth_finish(synth);
    mad_frame_finish(frame);
    mad_stream_finish(stream);
    madInitted = false;
  }

  if (!preallocateSpace) {
    free(buff);
    free(synth);
    free(frame);
    free(stream);
  }

  buff = NULL;
  synth = NULL;
  frame = NULL;
  stream = NULL;

  running = false;
  output->stop();
  return file->close();
}

bool AudioGeneratorMP3::isRunning()
{
  return running;
}

enum mad_flow AudioGeneratorMP3::ErrorToFlow()
{
  char err[64];
  char errLine[128];

  // Special case - eat "lost sync @ byte 0" as it always occurs and is not really correct....it never had sync!
  if ((lastReadPos==0) && (stream->error==MAD_ERROR_LOSTSYNC)) return MAD_FLOW_CONTINUE;

  strcpy_P(err, mad_stream_errorstr(stream));
  snprintf_P(errLine, sizeof(errLine), PSTR("Decoding error '%s' at byte offset %d"),
           err, (stream->this_frame - buff) + lastReadPos);
  yield(); // Something bad happened anyway, ensure WiFi gets some time, too
  cb.st(stream->error, errLine);
  return MAD_FLOW_CONTINUE;
}

enum mad_flow AudioGeneratorMP3::Input()
{
  int unused = 0;

  if (stream->next_frame) {
    unused = lastBuffLen - (stream->next_frame - buff);
    if (unused < 0) {
      desync();
      unused = 0;
    } else {
      memmove(buff, stream->next_frame, unused);
    }
    stream->next_frame = NULL;
  }

  if (unused == lastBuffLen) {
    // Something wicked this way came, throw it all out and try again
    unused = 0;
  }

  lastReadPos = file->getPos() - unused;
  int len = buffLen - unused;
  len = file->read(buff + unused, len);
  if ((len == 0)  && (unused == 0)) {
    // Can't read any from the file, and we don't have anything left.  It's done....
    return MAD_FLOW_STOP;
  }
  if (len < 0) {
    desync();
    unused = 0;
  }

  lastBuffLen = len + unused;
  mad_stream_buffer(stream, buff, lastBuffLen);

  return MAD_FLOW_CONTINUE;
}

void AudioGeneratorMP3::desync ()
{
    audioLogger->printf_P(PSTR("MP3:desync\n"));
    if (stream) {
        stream->next_frame = nullptr;
        stream->this_frame = nullptr;
        stream->sync = 0;
    }
    lastBuffLen = 0;
}

bool AudioGeneratorMP3::DecodeNextFrame()
{
  if (mad_frame_decode(frame, stream) == -1) {
    ErrorToFlow(); // Always returns CONTINUE
    return false;
  }
  nsCountMax  = MAD_NSBSAMPLES(&frame->header);
  return true;
}

bool AudioGeneratorMP3::GetOneSample(int16_t sample[2])
{
  if (synth->pcm.samplerate != lastRate) {
    output->SetRate(synth->pcm.samplerate);
    lastRate = synth->pcm.samplerate;
  }
  if (synth->pcm.channels != lastChannels) {
    output->SetChannels(synth->pcm.channels);
    lastChannels = synth->pcm.channels;
  }

  // If we're here, we have one decoded frame and sent 0 or more samples out
  if (samplePtr < synth->pcm.length) {
    sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr];
    sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr];
    
    // Speed control: 0=slow(0.5x), 1=normal(1x), 2=fast(2x)
    if(playbackSpeed == 2) {
      samplePtr += 2; // Skip every other sample for 2x speed
    } else if(playbackSpeed == 0) {
      // For 0.5x speed, repeat samples
      if(++speedCounter >= 2) {
        speedCounter = 0;
        samplePtr++;
      }
    } else {
      samplePtr++; // Normal speed
    }
  } else {
    samplePtr = 0;

    switch ( mad_synth_frame_onens(synth, frame, nsCount++) ) {
        case MAD_FLOW_STOP:
        case MAD_FLOW_BREAK: audioLogger->printf_P(PSTR("msf1ns failed\n"));
          return false; // Either way we're done
        default:
          break; // Do nothing
    }
    // for IGNORE and CONTINUE, just play what we have now
    sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr];
    sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr];
    samplePtr++;
  }
  
  // Update EQ buffers
  if(channelEQBuffer) {
    int16_t absL = abs(sample[AudioOutput::LEFTCHANNEL]);
    int16_t absR = abs(sample[AudioOutput::RIGHTCHANNEL]);
    channelEQBuffer[0] = (absL >> 9) & 0x3F;
    channelEQBuffer[1] = (absR >> 9) & 0x3F;
  }
  
  // Update spectrum analyzer (96 bands) - separate L/R channels
  if(eqBuffer && FFTL && FFTR) {
    // Collect samples (skip every 32nd sample to reduce load)
    if(++sampleSkip >= 32) {
      sampleSkip = 0;
      vRealL[fftPos] = sample[AudioOutput::LEFTCHANNEL];
      vImagL[fftPos] = 0;
      vRealR[fftPos] = sample[AudioOutput::RIGHTCHANNEL];
      vImagR[fftPos] = 0;
      fftPos++;
      
      if(fftPos >= 128) {
        fftPos = 0;
        
        // Process LEFT channel (even indices: 0,2,4...)
        FFTL->windowing(FFTWindow::Hamming, FFTDirection::Forward);
        FFTL->compute(FFTDirection::Forward);
        FFTL->complexToMagnitude();
        
        float sampleRate = lastRate > 0 ? lastRate : 44100;
        float freqResolution = sampleRate / 128.0;
        
        for(int band = 0; band < 48; band++) {
          float freqMin, freqMax;
          // Non-linear distribution: 8 bass, 20 mids, 20 highs
          if(band < 8) {
            // Bass: 20-250Hz (8 bands) - logarithmic for better separation
            freqMin = 20.0 * pow(1.35, band);
            freqMax = 20.0 * pow(1.35, band + 1);
          } else if(band < 28) {
            // Mids: 250-2000Hz (20 bands)
            freqMin = 250.0 + ((band - 8) * 87.5);
            freqMax = 250.0 + ((band - 7) * 87.5);
          } else {
            // Highs: 2000-20000Hz (20 bands)
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
      }
    }
  }
  
  return true;
}


bool AudioGeneratorMP3::loop()
{
  if (!running) goto done; // Nothing to do here!

  // First, try and push in the stored sample.  If we can't, then punt and try later
  if (!output->ConsumeSample(lastSample)) goto done; // Can't send, but no error detected

  // Try and stuff the buffer one sample at a time
  do
  {
    // Decode next frame if we're beyond the existing generated data
    if ( (samplePtr >= synth->pcm.length) && (nsCount >= nsCountMax) ) {
retry:
      if (Input() == MAD_FLOW_STOP) {
        return false;
      }

      if (!DecodeNextFrame()) {
        if (stream->error == MAD_ERROR_BUFLEN) {
          // randomly seeking can lead to endless
          // and unrecoverable "MAD_ERROR_BUFLEN" loop
          audioLogger->printf_P(PSTR("MP3:ERROR_BUFLEN %d\n"), unrecoverable);
          if (++unrecoverable >= 3) {
            unrecoverable = 0;
            stop();
            return running;
          }
        } else {
          unrecoverable = 0;
        }
        goto retry;
      }
      samplePtr = 9999;
      nsCount = 0;
    }

    if (!GetOneSample(lastSample)) {
      audioLogger->printf_P(PSTR("G1S failed\n"));
      running = false;
      goto done;
    }
    
    // Update track frame (current position in 1/50th seconds)
    if(trackFrameInitialized && lastRate > 0) {
      static float frameAccumulator = 0.0f;
      float speedMultiplier = (playbackSpeed == 2) ? 2.0f : (playbackSpeed == 0) ? 0.5f : 1.0f;
      frameAccumulator += (50.0f / (float)lastRate) * speedMultiplier;
      if(frameAccumulator >= 1.0f) {
        int framesToAdd = (int)frameAccumulator;
        (*trackFrame) += framesToAdd;
        frameAccumulator -= framesToAdd;
      }
    }
    if (lastChannels == 1)
    {
      lastSample[1] = lastSample[0];
    }
  } while (running && output->ConsumeSample(lastSample));

done:
  file->loop();
  output->loop();

  return running;
}

bool AudioGeneratorMP3::initializeFile(AudioFileSource *source){
	if(!source) return false;
	file = source;
	return true;
}

bool AudioGeneratorMP3::begin(AudioFileSource *source, AudioOutput *output)
{
  if (!source)  return false;
  file = source;
  if (!output) return false;
  this->output = output;
  if (!file->isOpen()) {
    audioLogger->printf_P(PSTR("MP3 source file not open\n"));
    return false; // Error
  }

  // Reset error count from previous file
  unrecoverable = 0;

  output->SetBitsPerSample(16); // Constant for MP3 decoder
  output->SetChannels(2);

  if (!output->begin()) return false;

  // Where we are in generating one frame's data, set to invalid so we will run loop on first getsample()
  samplePtr = 9999;
  nsCount = 9999;
  lastRate = 0;
  lastChannels = 0;
  lastReadPos = 0;
  lastBuffLen = 0;

  // Allocate all large memory chunks
  if (preallocateStreamSize + preallocateFrameSize + preallocateSynthSize) {
    if (preallocateSize >= preAllocBuffSize() &&
        preallocateStreamSize >= preAllocStreamSize() &&
        preallocateFrameSize >= preAllocFrameSize() &&
        preallocateSynthSize >= preAllocSynthSize()) {
      buff = reinterpret_cast<unsigned char *>(preallocateSpace);
      stream = reinterpret_cast<struct mad_stream *>(preallocateStreamSpace);
      frame = reinterpret_cast<struct mad_frame *>(preallocateFrameSpace);
      synth = reinterpret_cast<struct mad_synth *>(preallocateSynthSpace);
    }
    else {
      output->stop();
      audioLogger->printf_P("OOM error in MP3:  Want %d/%d/%d/%d bytes, have %d/%d/%d/%d bytes preallocated.\n",
          preAllocBuffSize(), preAllocStreamSize(), preAllocFrameSize(), preAllocSynthSize(),
          preallocateSize, preallocateStreamSize, preallocateFrameSize, preallocateSynthSize);
      return false;
    }
  } else if (preallocateSpace) {
    uint8_t *p = reinterpret_cast<uint8_t *>(preallocateSpace);
    buff = reinterpret_cast<unsigned char *>(p);
    p += preAllocBuffSize();
    stream = reinterpret_cast<struct mad_stream *>(p);
    p += preAllocStreamSize();
    frame = reinterpret_cast<struct mad_frame *>(p);
    p += preAllocFrameSize();
    synth = reinterpret_cast<struct mad_synth *>(p);
    p += preAllocSynthSize();
    int neededBytes = p - reinterpret_cast<uint8_t *>(preallocateSpace);
    if (neededBytes > preallocateSize) {
      output->stop();
      audioLogger->printf_P("OOM error in MP3:  Want %d bytes, have %d bytes preallocated.\n", neededBytes, preallocateSize);
      return false;
    }
  } else {
    buff = reinterpret_cast<unsigned char *>(malloc(buffLen));
    stream = reinterpret_cast<struct mad_stream *>(malloc(sizeof(struct mad_stream)));
    frame = reinterpret_cast<struct mad_frame *>(malloc(sizeof(struct mad_frame)));
    synth = reinterpret_cast<struct mad_synth *>(malloc(sizeof(struct mad_synth)));
    if (!buff || !stream || !frame || !synth) {
      free(buff);
      free(stream);
      free(frame);
      free(synth);
      buff = NULL;
      stream = NULL;
      frame = NULL;
      synth = NULL;

      output->stop();
      audioLogger->printf_P("OOM error in MP3\n");
      return false;
    }
  }

  mad_stream_init(stream);
  mad_frame_init(frame);
  mad_synth_init(synth);
  synth->pcm.length = 0;
  mad_stream_options(stream, 0); // TODO - add options support
  madInitted = true;

  running = true;
  return true;
}

signed long AudioGeneratorMP3::getPlaybackTime(bool oneFiftieth) {
  if(!file || !file->isOpen()) return -1;
  uint32_t currentPos = file->getPos();
  file->seek(0, SEEK_SET);
  uint8_t h[10];
  uint32_t start = 0;
  if(file->read(h, 10) == 10 && h[0]=='I' && h[1]=='D' && h[2]=='3') {
    start = ((h[6]&0x7F)<<21)|((h[7]&0x7F)<<14)|((h[8]&0x7F)<<7)|(h[9]&0x7F) + 10;
  }
  file->seek(start, SEEK_SET);
  uint8_t d[200];
  if(file->read(d, 200) < 200) { file->seek(currentPos, SEEK_SET); return -1; }
  int s = -1;
  for(int i = 0; i < 190; i++) if(d[i] == 0xFF && (d[i+1] & 0xE0) == 0xE0) { s = i; break; }
  if(s < 0) { file->seek(currentPos, SEEK_SET); return -1; }
  uint8_t *f = &d[s];
  int v = (f[1] >> 3) & 3, bi = (f[2] >> 4) & 0xF, si = (f[2] >> 2) & 3;
  const int br[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
  const int sr[3] = {44100,48000,32000};
  if(bi == 0 || bi == 15 || si == 3) { file->seek(currentPos, SEEK_SET); return -1; }
  int xo = (v == 3) ? ((f[3] & 0xC0) == 0xC0 ? 36 : 21) : ((f[3] & 0xC0) == 0xC0 ? 21 : 13);
  uint8_t *x = &f[xo];
  if((x[0]=='X'&&x[1]=='i'&&x[2]=='n'&&x[3]=='g')||(x[0]=='I'&&x[1]=='n'&&x[2]=='f'&&x[3]=='o')) {
    uint32_t fl = x[4]<<24|x[5]<<16|x[6]<<8|x[7];
    if(fl & 1) {
      uint32_t fc = x[8]<<24|x[9]<<16|x[10]<<8|x[11];
      float sec = (float)fc * 1152.0f / (float)sr[si];
      file->seek(currentPos, SEEK_SET);
      return (signed long)(sec * (oneFiftieth ? 50 : 1000));
    }
  }
  float sec = (float)(file->getSize() - start) * 8.0f / (float)(br[bi] * 1000);
  file->seek(currentPos, SEEK_SET);
  return (signed long)(sec * (oneFiftieth ? 50 : 1000));
}

// The following are helper routines for use in libmad to check stack/heap free
// and to determine if there's enough stack space to allocate some blocks there
// instead of precious heap.

#undef stack
extern "C" {
#ifdef ESP32
  //TODO - add ESP32 checks
  void stack(const char *s, const char *t, int i)
  {
  }
  int stackfree()
  {
    return 8192;
  }
#elif defined(ESP8266) && !defined(CORE_MOCK)
  #include <cont.h>
  extern cont_t g_cont;

  void stack(const char *s, const char *t, int i)
  {
    (void) t;
    (void) i;
    register uint32_t *sp asm("a1");
    int freestack = 4 * (sp - g_cont.stack);
    int freeheap = ESP.getFreeHeap();
    if ((freestack < 512) || (freeheap < 5120)) {
      static int laststack, lastheap;
      if (laststack!=freestack|| lastheap !=freeheap) {
        audioLogger->printf_P(PSTR("%s: FREESTACK=%d, FREEHEAP=%d\n"), s, /*t, i,*/ freestack, /*cont_get_free_stack(&g_cont),*/ freeheap);
      }
      if (freestack < 256) {
        audioLogger->printf_P(PSTR("out of stack!\n"));
      }
      if (freeheap < 1024) {
        audioLogger->printf_P(PSTR("out of heap!\n"));
      }
      Serial.flush();
      laststack = freestack;
      lastheap = freeheap;
    }
  }

  int stackfree()
  {
    register uint32_t *sp asm("a1");
    int freestack = 4 * (sp - g_cont.stack);
    return freestack;
  }
#else
  void stack(const char *s, const char *t, int i)
  {
    (void) s;
    (void) t;
    (void) i;
  }
  int stackfree()
  {
    return 8192;
  }
#endif
}


void AudioGeneratorMP3::initTrackFrame(unsigned long* tF) {
  if(tF != nullptr) {
    trackFrame = tF;
    *trackFrame = 0;
    trackFrameInitialized = true;
  }
}

int AudioGeneratorMP3::getBitrate(){
  if(!file||!file->isOpen())return 0;
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint8_t h[10];
  uint32_t start=0;
  if(file->read(h,10)==10&&h[0]=='I'&&h[1]=='D'&&h[2]=='3'){
    start=((h[6]&0x7F)<<21)|((h[7]&0x7F)<<14)|((h[8]&0x7F)<<7)|(h[9]&0x7F)+10;
  }
  file->seek(start,SEEK_SET);
  uint8_t d[4];
  if(file->read(d,4)<4){file->seek(currentPos,SEEK_SET);return 0;}
  if(d[0]!=0xFF||(d[1]&0xE0)!=0xE0){file->seek(currentPos,SEEK_SET);return 0;}
  int bi=(d[2]>>4)&0xF;
  const int br[16]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
  file->seek(currentPos,SEEK_SET);
  return br[bi];
}

int AudioGeneratorMP3::getChannelMode(){
  if(!file||!file->isOpen())return 0;
  uint32_t currentPos=file->getPos();
  file->seek(0,SEEK_SET);
  uint8_t h[10];
  uint32_t start=0;
  if(file->read(h,10)==10&&h[0]=='I'&&h[1]=='D'&&h[2]=='3'){
    start=((h[6]&0x7F)<<21)|((h[7]&0x7F)<<14)|((h[8]&0x7F)<<7)|(h[9]&0x7F)+10;
  }
  file->seek(start,SEEK_SET);
  uint8_t d[4];
  if(file->read(d,4)<4){file->seek(currentPos,SEEK_SET);return 0;}
  if(d[0]!=0xFF||(d[1]&0xE0)!=0xE0){file->seek(currentPos,SEEK_SET);return 0;}
  int mode=(d[3]>>6)&0x3;
  file->seek(currentPos,SEEK_SET);
  return mode;
}

void AudioGeneratorMP3::initEQBuffers(uint8_t* eqBuf, uint8_t* channelEQBuf){
  eqBuffer=eqBuf;
  channelEQBuffer=channelEQBuf;
  if(eqBuffer) memset(eqBuffer,0,96);
  if(channelEQBuffer) memset(channelEQBuffer,0,8);
}

void AudioGeneratorMP3::getTitle(char* title,size_t maxLen){
  if(!file||!file->isOpen()){title[0]='\0';return;}
  uint32_t currentPos=file->getPos();
  title[0]='\0';
  file->seek(0,SEEK_SET);
  uint8_t header[10];
  if(file->read(header,10)==10&&header[0]=='I'&&header[1]=='D'&&header[2]=='3'){
    uint8_t version=header[3];
    uint32_t tagSize=((header[6]&0x7F)<<21)|((header[7]&0x7F)<<14)|((header[8]&0x7F)<<7)|(header[9]&0x7F);
    uint8_t*tagData=(uint8_t*)malloc(tagSize);
    if(tagData&&file->read(tagData,tagSize)==tagSize){
      uint32_t pos=0;
      while(pos<tagSize-10){
        if(tagData[pos]==0)break;
        char frameId[5]={tagData[pos],tagData[pos+1],tagData[pos+2],tagData[pos+3],0};
        uint32_t frameSize;
        if(version==4)frameSize=((tagData[pos+4]&0x7F)<<21)|((tagData[pos+5]&0x7F)<<14)|((tagData[pos+6]&0x7F)<<7)|(tagData[pos+7]&0x7F);
        else frameSize=(tagData[pos+4]<<24)|(tagData[pos+5]<<16)|(tagData[pos+6]<<8)|tagData[pos+7];
        pos+=10;
        if((strcmp(frameId,"TIT2")==0||(version==2&&strcmp(frameId,"TT2")==0))&&frameSize>1){
          uint8_t encoding=tagData[pos];
          if(encoding==1||encoding==2){
            uint32_t textStart=pos+1;
            if(tagData[textStart]==0xFF&&tagData[textStart+1]==0xFE)textStart+=2;
            else if(tagData[textStart]==0xFE&&tagData[textStart+1]==0xFF){textStart+=2;}
            uint32_t outPos=0;
            for(uint32_t i=textStart;i<pos+frameSize&&outPos<maxLen-3;i+=2){
              uint16_t c;
              if(encoding==1)c=tagData[i]|(tagData[i+1]<<8);
              else c=(tagData[i]<<8)|tagData[i+1];
              if(c==0)break;
              if(c>=0x400&&c<=0x43F){title[outPos++]=0xD0;title[outPos++]=0x80+c-0x400;}
              else if(c>=0x440&&c<=0x44F){title[outPos++]=0xD1;title[outPos++]=0x80+c-0x440;}
              else if(c<0x80)title[outPos++]=c;
              else if(c<0x800){title[outPos++]=0xC0|(c>>6);title[outPos++]=0x80|(c&0x3F);}
              else{title[outPos++]=0xE0|(c>>12);title[outPos++]=0x80|((c>>6)&0x3F);title[outPos++]=0x80|(c&0x3F);}
            }
            title[outPos]='\0';
          }else{
            uint32_t len=min(frameSize-1,maxLen-1);
            memcpy(title,&tagData[pos+1],len);
            title[len]='\0';
          }
          break;
        }
        pos+=frameSize;
      }
      free(tagData);
    }
  }
  if(!title[0]){
    uint32_t fileSize=file->getSize();
    if(fileSize>128){
      file->seek(fileSize-128,SEEK_SET);
      uint8_t id3v1[128];
      if(file->read(id3v1,128)==128&&id3v1[0]=='T'&&id3v1[1]=='A'&&id3v1[2]=='G'){
        uint32_t outPos=0;
        for(int i=0;i<30&&id3v1[3+i]&&outPos<maxLen-3;i++){
          uint8_t c=id3v1[3+i];
          if(c>=0xC0&&c<=0xEF){title[outPos++]=0xD0;title[outPos++]=c-0x30;}
          else if(c>=0xF0){title[outPos++]=0xD1;title[outPos++]=c-0x70;}
          else if(c==0xA8){title[outPos++]=0xD0;title[outPos++]=0x81;}
          else if(c==0xB8){title[outPos++]=0xD1;title[outPos++]=0x91;}
          else title[outPos++]=c;
        }
        while(outPos>0&&title[outPos-1]==' ')outPos--;
        title[outPos]='\0';
      }
    }
  }
  file->seek(currentPos,SEEK_SET);
}

void AudioGeneratorMP3::setSpeed(int speed) {
  if(speed >= 0 && speed <= 2) {
    playbackSpeed = speed;
    speedCounter = 0;
  }
}

void AudioGeneratorMP3::getDescription(char* description,size_t maxLen){
  if(!file||!file->isOpen()){description[0]='\0';return;}
  uint32_t currentPos=file->getPos();
  description[0]='\0';
  file->seek(0,SEEK_SET);
  uint8_t header[10];
  char artist[256]={0},album[256]={0},year[16]={0};
  if(file->read(header,10)==10&&header[0]=='I'&&header[1]=='D'&&header[2]=='3'){
    uint8_t version=header[3];
    uint32_t tagSize=((header[6]&0x7F)<<21)|((header[7]&0x7F)<<14)|((header[8]&0x7F)<<7)|(header[9]&0x7F);
    uint8_t*tagData=(uint8_t*)malloc(tagSize);
    if(tagData&&file->read(tagData,tagSize)==tagSize){
      uint32_t pos=0;
      while(pos<tagSize-10){
        if(tagData[pos]==0)break;
        char frameId[5]={tagData[pos],tagData[pos+1],tagData[pos+2],tagData[pos+3],0};
        uint32_t frameSize;
        if(version==4)frameSize=((tagData[pos+4]&0x7F)<<21)|((tagData[pos+5]&0x7F)<<14)|((tagData[pos+6]&0x7F)<<7)|(tagData[pos+7]&0x7F);
        else frameSize=(tagData[pos+4]<<24)|(tagData[pos+5]<<16)|(tagData[pos+6]<<8)|tagData[pos+7];
        pos+=10;
        if(frameSize>1&&frameSize<512){
          uint8_t encoding=tagData[pos];
          char*target=nullptr;
          size_t targetLen=0;
          if(strcmp(frameId,"TPE1")==0||(version==2&&strcmp(frameId,"TP1")==0)){target=artist;targetLen=sizeof(artist);}
          else if(strcmp(frameId,"TALB")==0||(version==2&&strcmp(frameId,"TAL")==0)){target=album;targetLen=sizeof(album);}
          else if(strcmp(frameId,"TYER")==0||strcmp(frameId,"TDRC")==0||(version==2&&strcmp(frameId,"TYE")==0)){target=year;targetLen=sizeof(year);}
          if(target){
            if(encoding==1||encoding==2){
              uint32_t textStart=pos+1;
              if(tagData[textStart]==0xFF&&tagData[textStart+1]==0xFE)textStart+=2;
              else if(tagData[textStart]==0xFE&&tagData[textStart+1]==0xFF){textStart+=2;}
              uint32_t outPos=0;
              for(uint32_t i=textStart;i<pos+frameSize&&outPos<targetLen-3;i+=2){
                uint16_t c;
                if(encoding==1)c=tagData[i]|(tagData[i+1]<<8);
                else c=(tagData[i]<<8)|tagData[i+1];
                if(c==0)break;
                if(c>=0x400&&c<=0x43F){target[outPos++]=0xD0;target[outPos++]=0x80+c-0x400;}
                else if(c>=0x440&&c<=0x44F){target[outPos++]=0xD1;target[outPos++]=0x80+c-0x440;}
                else if(c<0x80)target[outPos++]=c;
                else if(c<0x800){target[outPos++]=0xC0|(c>>6);target[outPos++]=0x80|(c&0x3F);}
                else{target[outPos++]=0xE0|(c>>12);target[outPos++]=0x80|((c>>6)&0x3F);target[outPos++]=0x80|(c&0x3F);}
              }
              target[outPos]='\0';
            }else{
              uint32_t len=min(frameSize-1,targetLen-1);
              memcpy(target,&tagData[pos+1],len);
              target[len]='\0';
            }
          }
        }
        pos+=frameSize;
      }
      free(tagData);
    }
  }
  if(!artist[0]&&!album[0]){
    uint32_t fileSize=file->getSize();
    if(fileSize>128){
      file->seek(fileSize-128,SEEK_SET);
      uint8_t id3v1[128];
      if(file->read(id3v1,128)==128&&id3v1[0]=='T'&&id3v1[1]=='A'&&id3v1[2]=='G'){
        uint32_t outPos=0;
        for(int i=0;i<30&&id3v1[33+i]&&outPos<sizeof(artist)-3;i++){
          uint8_t c=id3v1[33+i];
          if(c>=0xC0&&c<=0xEF){artist[outPos++]=0xD0;artist[outPos++]=c-0x30;}
          else if(c>=0xF0){artist[outPos++]=0xD1;artist[outPos++]=c-0x70;}
          else if(c==0xA8){artist[outPos++]=0xD0;artist[outPos++]=0x81;}
          else if(c==0xB8){artist[outPos++]=0xD1;artist[outPos++]=0x91;}
          else artist[outPos++]=c;
        }
        while(outPos>0&&artist[outPos-1]==' ')outPos--;
        artist[outPos]='\0';
        outPos=0;
        for(int i=0;i<30&&id3v1[63+i]&&outPos<sizeof(album)-3;i++){
          uint8_t c=id3v1[63+i];
          if(c>=0xC0&&c<=0xEF){album[outPos++]=0xD0;album[outPos++]=c-0x30;}
          else if(c>=0xF0){album[outPos++]=0xD1;album[outPos++]=c-0x70;}
          else if(c==0xA8){album[outPos++]=0xD0;album[outPos++]=0x81;}
          else if(c==0xB8){album[outPos++]=0xD1;album[outPos++]=0x91;}
          else album[outPos++]=c;
        }
        while(outPos>0&&album[outPos-1]==' ')outPos--;
        album[outPos]='\0';
        strncpy(year,(char*)&id3v1[93],4);year[4]='\0';
        for(int i=strlen(year)-1;i>=0&&year[i]==' ';i--)year[i]='\0';
      }
    }
  }
  if(artist[0])strncat(description,artist,maxLen-strlen(description)-1);
  if(album[0]){
    if(description[0])strncat(description," - ",maxLen-strlen(description)-1);
    strncat(description,album,maxLen-strlen(description)-1);
  }
  if(year[0]){
    if(description[0])strncat(description," (",maxLen-strlen(description)-1);
    strncat(description,year,maxLen-strlen(description)-1);
    if(description[0])strncat(description,")",maxLen-strlen(description)-1);
  }
  file->seek(currentPos,SEEK_SET);
}
