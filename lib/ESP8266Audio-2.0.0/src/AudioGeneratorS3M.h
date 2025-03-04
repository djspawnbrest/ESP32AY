#ifndef _AUDIOGENERATORS3M_H
#define _AUDIOGENERATORS3M_H

#include "AudioGenerator.h"

#define FATBUFFERSIZE 3*1024
#define FIXED_DIVIDER 10				// Fixed-point mantissa used for integer arithmetic
#define SAMPLERATE 44100
#define AMIGA (14317056L/sampleRate<<FIXED_DIVIDER) 	//s3m player

// Effects
#define SETSPEED                 0x1  // Axx
#define JUMPTOORDER              0x2  // Bxx
#define BREAKPATTERNTOROW        0x3  // Cxx
#define VOLUMESLIDE              0x4  // Dxx
#define PORTAMENTODOWN           0x5  // Exx
#define PORTAMENTOUP             0x6  // Fxx
#define TONEPORTAMENTO           0x7  // Gxx
#define VIBRATO                  0x8  // Hxy
#define TREMOR                   0x9  // Ixy
#define ARPEGGIO                 0xA  // Jxy
#define VIBRATOVOLUMESLIDE       0xB  // Kxy
#define PORTAMENTOVOLUMESLIDE    0xC  // Lxy
#define SETSAMPLEOFFSET          0xF  // Oxy
#define RETRIGGERNOTEVOLUMESLIDE 0x11 // Qxy
#define TREMOLO                  0x12 // Rxy
#define SETTEMPO                 0x14 // Txx
#define FINEVIBRATO              0x15 // Uxy
#define SETGLOBALVOLUME          0x16 // Vxx

// 0x13 subset effects
#define SETFILTER                0x0
#define SETGLISSANDOCONTROL      0x1
#define SETFINETUNE              0x2
#define SETVIBRATOWAVEFORM       0x3
#define SETTREMOLOWAVEFORM       0x4
#define SETCHANNELPANNING        0x8
#define STEREOCONTROL            0xA
#define PATTERNLOOP              0xB
#define NOTECUT                  0xC
#define NOTEDELAY                0xD
#define PATTERNDELAY             0xE
#define FUNKREPEAT               0xF

class AudioGeneratorS3M:public AudioGenerator{
	public:
		AudioGeneratorS3M();
		virtual ~AudioGeneratorS3M() override;
		virtual bool begin(AudioFileSource *source,AudioOutput *output) override;
		virtual bool loop() override;
		virtual bool stop() override;
		virtual bool isRunning() override { return running; };
		bool SetSampleRate(int hz){if(running||(hz<20000)||(hz>96000)) return false; sampleRate=samplerateOriginal=hz; return true;}
		bool SetBufferSize(int sz){if(running||(sz<1)) return false; fatBufferSize=sz; return true;}
		bool SetStereoSeparation(int sep){if(running||(sep<0)||(sep>64)) return false; stereoSeparation=sep; return true;}
		// Aditional methods
		void setPause(bool pause);
		bool initializeFile(AudioFileSource *source);
		uint8_t getNumberOfChannels();
		void getTitle(char* lfn,size_t maxLen);
		void getDescription(char* description,size_t maxLen);
		signed long getPlaybackTime(bool oneFiftieth=true); // For calculate time
		void initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer); // EQ buffer is 96 elements max, channel EQ buffer is 8 elements max
		void initTrackFrame(unsigned long* tF);
		void SetSeparation(int sep);
		void setSpeed(uint8_t speed);

	public:
		bool isPaused;

  protected:
		bool LoadS3M();
		bool LoadHeader();
		void GetSample(int16_t sample[2]);
		bool RunPlayer();
		void LoadSamples();
		bool LoadPattern(uint8_t pattern);
		bool ProcessTick();
		bool ProcessRow();
		void Tremolo(uint8_t channel);
		void Portamento(uint8_t channel);
		void Vibrato(uint8_t channel, bool fine);
		void Tremor(uint8_t channel);
		void VolumeSlide(uint8_t channel);
		// Helper additional methods
		uint16_t CalculateAmigaPeriod(uint8_t note,uint8_t instrument);
		void removeExtraSpaces(char* str);
		void freeFatBuffer();
		
	protected:
		int mixerTick;
		int sampleRate;
		int samplerateOriginal; // Original sample rate (need for speed change)
		int fatBufferSize; // File system buffers per-CHANNEL (i.e. total mem required is 6 * FATBUFFERSIZE)
		int stereoSeparation; // 0 (max), 64 (half stereo), 128 (mono)
		uint8_t* eqBuffer=nullptr; // Pointer to the equalizer buffer
		uint8_t* channelEQBuffer=nullptr; // Pointer to the equalizer channel buffer
		bool buffersInitialized=false; // Equalizer buffers initialization flag
		bool trackFrameInitialized=false; // Track frame initialization flag
		unsigned long* trackFrame; // Track frame pointer
		float totalSeconds;  // For storing total playback duration
		volatile bool stopping=false;
    volatile bool bufferFreed=false;

		// Hz = 7093789 / (amigaPeriod * 2) for PAL
		// Hz = 7159091 / (amigaPeriod * 2) for NTSC
 
#ifdef ESP8266 // Not sure if C3/C2 have RAM constraints, maybe add them here?
		// support max 4 channels
		enum{ROWS=64,INSTRUMENTS=99,CHANNELS=4,NONOTE=108,KEYOFF=109,NOVOLUME=255};
#else
		// support max 8 channels
		enum{ROWS=64,INSTRUMENTS=99,CHANNELS=16,NONOTE=132,KEYOFF=133,NOVOLUME=255};
#endif

	typedef struct S3M_Instrument{
		uint8_t name[28];
		uint16_t sampleParapointer;
		uint16_t length;
		uint16_t loopBegin;
		uint16_t loopEnd;
		uint8_t volume;
		uint16_t middleC;
		bool loop;
	}S3M_Instrument;

	typedef struct S3M_Pattern{
		uint16_t note[ROWS][CHANNELS];
		uint8_t instrumentNumber[ROWS][CHANNELS];
		uint8_t volume[ROWS][CHANNELS];
		uint8_t effectNumber[ROWS][CHANNELS];
		uint8_t effectParameter[ROWS][CHANNELS];
	}S3M_Pattern;

	typedef struct s3m{
		uint8_t name[28];
		S3M_Instrument instruments[INSTRUMENTS];
		uint16_t songLength;
		uint16_t numberOfInstruments;
		uint16_t numberOfPatterns;
		bool fastVolumeSlides;
		uint8_t globalVolume;
		uint8_t order[256];
		uint8_t numberOfChannels;
		uint8_t channelRemapping[CHANNELS];
		uint16_t instrumentParapointers[INSTRUMENTS];
		uint16_t patternParapointers[256];
	}s3m;

	typedef struct s3m_player{
		S3M_Pattern currentPattern;
		uint32_t amiga;
		uint16_t samplesPerTick;
		uint8_t speed;
		int bpmOriginal;
		uint8_t tick;
		uint8_t row;
		uint8_t lastRow;
		uint8_t orderIndex;
		uint8_t oldOrderIndex;
		uint8_t patternDelay;
		uint8_t patternLoopCount[CHANNELS];
		uint8_t patternLoopRow[CHANNELS];
		uint8_t lastInstrumentNumber[CHANNELS];
		int8_t volume[CHANNELS];
		uint16_t lastNote[CHANNELS];
		uint16_t amigaPeriod[CHANNELS];
		int16_t lastAmigaPeriod[CHANNELS];
		uint8_t lastVolumeSlide[CHANNELS];
		uint8_t lastPortamento[CHANNELS];
		uint16_t portamentoNote[CHANNELS];
		uint8_t portamentoSpeed[CHANNELS];
		uint8_t waveControl[CHANNELS];
		uint8_t vibratoSpeed[CHANNELS];
		uint8_t vibratoDepth[CHANNELS];
		int8_t vibratoPos[CHANNELS];
		uint8_t tremoloSpeed[CHANNELS];
		uint8_t tremoloDepth[CHANNELS];
		int8_t tremoloPos[CHANNELS];
		uint8_t tremorOnOff[CHANNELS];
		uint8_t tremorCount[CHANNELS];
		uint8_t retriggerVolumeSlide[CHANNELS];
		uint8_t retriggerSpeed[CHANNELS];
	}s3m_player;

	typedef struct mixer{
		uint32_t sampleBegin[INSTRUMENTS];
		uint32_t sampleEnd[INSTRUMENTS];
		uint32_t sampleLoopBegin[INSTRUMENTS];
		uint16_t sampleLoopLength[INSTRUMENTS];
		uint32_t sampleLoopEnd[INSTRUMENTS];
		uint8_t channelSampleNumber[CHANNELS];
		uint32_t channelSampleOffset[CHANNELS];
		uint16_t channelFrequency[CHANNELS];
		uint8_t channelVolume[CHANNELS];
		uint8_t channelPanning[CHANNELS];
		int32_t channelLastVolume[CHANNELS];  // For volume ramping
	}mixer;

	typedef struct fatBuffer{
		uint8_t *channels[CHANNELS]; // Make dynamically allocated [FATBUFFERSIZE];
		uint32_t samplePointer[CHANNELS];
		uint8_t channelSampleNumber[CHANNELS];
	}fatBuffer;

	// Our state lives here...
	s3m_player Player;
	s3m S3m;
	mixer Mixer;
	fatBuffer FatBuffer;
};

#endif
