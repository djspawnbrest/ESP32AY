#ifndef _AUDIOGENERATORXM_H
#define _AUDIOGENERATORXM_H

#include "AudioGenerator.h"
#include "libxmize/xm_cpp.h"


class AudioGeneratorXM:public AudioGenerator{
  public:
    AudioGeneratorXM();
    virtual ~AudioGeneratorXM() override;
    bool initializeFile(AudioFileSource *source,const char** message=nullptr);
    virtual bool begin(AudioFileSource *source,AudioOutput *output) override;
    virtual bool loop() override;
    virtual bool stop() override;
    virtual bool isRunning() override {return running;}
    bool SetSampleRate(int hz){if(running||(hz<20000)||(hz>96000)) return false; sampleRate=hz; return true;}           // NOT IMPLEMENTED YET   
    bool SetStereoSeparation(int sep){if(running||(sep<0)||(sep>64)) return false; stereoSeparation=sep; return true;}  // !
    bool SetPAL(bool use){if(running) return false; usePAL=use; return true;}
    // Aditional methods
    void getTitle(char* lfn,size_t maxLen);                         // !
    void getDescription(char* description,size_t maxLen);           // !
    signed long getPlaybackTime(bool oneFiftieth=true);             // !
    void setPause(bool pause);                                      // !
    void setSpeed(uint8_t);                                         // !
    uint8_t getNumberOfChannels();                                  // !
    void initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer); //    EQ buffer is 96 elements max, channel EQ buffer is 8 elements max
    void initTrackFrame(unsigned long* tF);                         // !
    void SetSeparation(int sep);                                    // !
    void SetLoop(bool isLooped=false);                              // !
    void playerTaskEnable(bool enable=false);

  public:
    bool isPaused;

  protected:
    // Helper additional methods
	  void removeExtraSpaces(char* str);

  protected:
    int sampleRate;
    int stereoSeparation; //STEREOSEPARATION=32;    // 0 (max) to 64 (mono)
    bool usePAL;
    uint8_t* eqBuffer=nullptr;        // Pointer to the equalizer buffer
    uint8_t* channelEQBuffer=nullptr; // Pointer to the equalizer channel buffer
    unsigned long* trackFrame;        // Pointer of playing time 1/50 of second
    bool buffersInitialized=false;    // Equalizer buffers initialization flag
    bool trackFrameInitialized=false; // Track frame initialization flag
    bool running=false;               // Running flag
    bool Looped=false;                // Loop track flag
    bool playerTask=false;            // Dedicated player task flag
};

#endif