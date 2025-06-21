#define PGM_READ_UNALIGNED 0

#include "AudioGeneratorS3M.h"

#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")

// #define DEBUG_LHEADER
// #define DEBUG_LPATTERN
// #define DEBUG_SAMPLE_TIMING

#ifndef min
#define min(X,Y)((X)<(Y)?(X):(Y))
#endif

#ifndef max
#define max(X,Y)((X)>(Y)?(X):(Y))
#endif

#define MSN(x)(((x)&0xf0)>>4)
#define LSN(x)((x)&0x0f)

// Sorted Amiga periods
static const uint16_t amigaPeriods[134] PROGMEM={
	27392, 25856, 24384, 23040, 21696, 20480, 19328, 18240, 17216, 16256, 15360, 14496, // 0
	13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8608,  8128,  7680,  7248, // 1
	 6848,  6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3624, // 2
	 3424,  3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1812, // 3
	 1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906, // 4 - middle octave from C-4
		856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453, // 5
		428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226, // 6
		214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113, // 7
		107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56, // 8
		 53,		50,		 47,		45,		 42,		40,		 37,		35,		 33,		31,		 30,		28, // 9 extended
		 26,		25,		 23,		22,		 21,		20,		 18,		17,		 16,		15,		 15,		14, // 10	extended
			0,     0                                                                        // NONOTE-132, KEYOFF-133
};

#define ReadAmigaPeriods(a)(uint16_t)pgm_read_word(amigaPeriods+(a))

static const uint16_t fineTuneToHz[16] PROGMEM={
	8363, 8413, 8463, 8529, 8581, 8651, 8723, 8757,
	7895, 7941, 7985, 8046, 8107, 8169, 8232, 8280
 };

#define ReadFineTuneToHz(a)(uint16_t)pgm_read_word(fineTuneToHz+(a))
 
static const uint8_t sine[64] PROGMEM={
	0,  24,  49,  74,  97, 120, 141, 161,
	180, 197, 212, 224, 235, 244, 250, 253,
	255, 253, 250, 244, 235, 224, 212, 197,
	180, 161, 141, 120,  97,  74,  49,  24
};

#define ReadSine(a)(uint8_t)pgm_read_byte(sine+(a))

static inline uint16_t MakeWord(uint8_t h,uint8_t l){return h<<8|l;}

/* Convert signed to unsigned sample data */
static inline void convert_signal(uint8_t *p, int l, int r){
	uint16_t *w = (uint16_t *)p;
	if (r) {
		for (; l--; w++)
			*w += 0x8000;
	} else {
		for (; l--; p++)
			*p += (unsigned char)0x80;
	}
}

AudioGeneratorS3M::AudioGeneratorS3M(){
	sampleRate=44100;
	samplerateOriginal=sampleRate;
	fatBufferSize=FATBUFFERSIZE;
	stereoSeparation=32;
	mixerTick=0;
	running=false;
	isPaused=false;
	file=NULL;
	output=NULL;
	// Initialize Player structure
	memset(&Player,0,sizeof(Player));
	Player.bpmOriginal=125;  // Default BPM
	Player.speed=6;          // Default speed
	// Initialize Mixer structure
	memset(&Mixer,0,sizeof(Mixer));
	// Initialize FatBuffer
	memset(&FatBuffer,0,sizeof(FatBuffer));
}

AudioGeneratorS3M::~AudioGeneratorS3M(){
	freeFatBuffer();
}

bool AudioGeneratorS3M::stop(){
	if(!running) return true;  // Already stopped
	stopping=true;
	// Give time for any ongoing sample processing to complete
	vTaskDelay(pdMS_TO_TICKS(10));
	// Flush output
	output->flush();  //flush I2S output buffer, if the player was actually running before.
  if(file&&file->isOpen()) file->close();
  if(output) output->stop();
	running=false;
	// We may be stopping because of allocation failures, so always deallocate
	if(!bufferFreed){
		freeFatBuffer();
		bufferFreed=true;
	}
  return true;
}

bool AudioGeneratorS3M::loop(){
	if(isPaused) goto done; // Easy-peasy
  if(!running) goto done; // Easy-peasy
  // First, try and push in the stored sample.  If we can't, then punt and try later
  if(!output->ConsumeSample(lastSample)) goto done; // FIFO full, wait...
  // Now advance enough times to fill the i2s buffer
  do{
    if(mixerTick==0){
      running=RunPlayer();
      if(!running){
        stop();
        goto done;
      }
      mixerTick=Player.samplesPerTick;
    }
    GetSample(lastSample);
    mixerTick--;
  }while(output->ConsumeSample(lastSample));
done:
  file->loop();
  output->loop();
  return running;
}

bool AudioGeneratorS3M::begin(AudioFileSource *source, AudioOutput *out){
  if(running) stop();

  if(!file){
    if(!initializeFile(source)) return false;
  }
  if(!out) return false;
  output=out;
  
  if(!file->isOpen()) return false; // Can't read the file!

  // Set the output values properly
  if(!output->SetRate(sampleRate)) return false;
  if(!output->SetBitsPerSample(16)) return false;
  if(!output->SetChannels(2)) return false;
  if(!output->begin()) return false;

  if(!LoadS3M()){
    stop();
    return false;
  }

	// Initialize buffer for s3m channels
	for(uint8_t channel=0;channel<S3m.numberOfChannels;channel++){
		FatBuffer.samplePointer[channel]=0;
		FatBuffer.channelSampleNumber[channel]=0xFF;
		// Allocate buffer
		FatBuffer.channels[channel]=(uint8_t*)malloc(fatBufferSize);
		if(!FatBuffer.channels[channel]){
			// Allocation failed - clean up
			freeFatBuffer();
			return false;
		}
	}

  running=true;
  return true;
}

bool AudioGeneratorS3M::LoadHeader(){
	uint8_t i;
	uint16_t songLength;
	uint16_t numberOfPatterns;
	uint16_t temp16;
	uint8_t masterVolume;
	uint8_t defaultPanning;
	uint8_t temp8;

	file->seek(0,SEEK_SET); // reset to zero file position
	if(28!=file->read(S3m.name,28)) return false;					// Song name
	file->seek(0x20,SEEK_SET);
	if(2!=file->read(&songLength,2)) return false;				// Song length (in orders)
	if(2!=file->read(&S3m.numberOfInstruments,2)) return false;	// Instruments count
	if(2!=file->read(&numberOfPatterns,2)) return false;	// Count of orders
	S3m.fastVolumeSlides=false;
	if(2!=file->read(&temp16,2)) return false;         		// Flags
	if(temp16&64) S3m.fastVolumeSlides=true;           		// Fast volume slides flag
	if(2!=file->read(&temp16,2)) return false;	       		// Created with tracker / version
	if((temp16&0xFFF)==300) S3m.fastVolumeSlides=true; 		// Fast volume slides for Scream Tracker 3.00
	if(1!=file->read(&temp8,1)) return false; 						// File format information (signed or unsigned sample)
	S3m.signedSample=(temp8==1)?true:false;								// Flag signed/unsigned samples data type
	file->seek(0x30,SEEK_SET);
	if(1!=file->read(&S3m.globalVolume,1)) return false;	// Global volume
	if(1!=file->read(&Player.speed,1)) return false;			// Initial speed
	if(1!=file->read(&temp8,1)) return false;							// initial tempo (BPM)
	if(temp8==0) temp8=125;		  													// Default to 125 BPM if zero
	Player.bpmOriginal=temp8;															// BPM
	Player.samplesPerTick=sampleRate/(2*temp8/5);    			// Hz = samplerate / (2 * BPM / 5)
	if(1!=file->read(&masterVolume,1)) return false;			// Check bit 8 later (Master volume)
	if(1!=file->read(&temp8,1)) return false;		          // Skip ultraclick removal
	if(1!=file->read(&defaultPanning,1)) return false;		// Initial panning
	file->seek(0x40,SEEK_SET);
	// Find the number of channels and remap the used channels linearly
	S3m.numberOfChannels = 0;
	memset(S3m.channelRemapping,255,CHANNELS);
	for(i=0;i<CHANNELS;i++){
		temp8=0;
		if(1!=file->read(&temp8,1)) return false;
		if((temp8&0x80)==0){
			S3m.channelRemapping[i]=S3m.numberOfChannels;
			Mixer.channelPanning[S3m.numberOfChannels]=(temp8&0x10)?0xC:0x3;
			S3m.numberOfChannels++;
		}
	}

	file->seek(0x60,SEEK_SET);

	// Load order data
	if(songLength!=file->read(S3m.order,songLength)) return false;
	// Calculate number of physical patterns
	S3m.songLength=0;
	S3m.numberOfPatterns=0;
	for(i=0;i<songLength;i++){
		S3m.order[S3m.songLength]=S3m.order[i];
		if(S3m.order[i]<254){
			S3m.songLength++;
			if(S3m.order[i]>S3m.numberOfPatterns)	S3m.numberOfPatterns=S3m.order[i];
		}
	}
	// Load parapointers
	if(S3m.numberOfInstruments*2!=file->read(S3m.instrumentParapointers,S3m.numberOfInstruments*2)) return false;
	if(numberOfPatterns*2!=file->read(S3m.patternParapointers,numberOfPatterns*2)) return false;
	// If the panning flag is set then set default panning
	if(defaultPanning==0xFC){
		for(i=0;i<CHANNELS;i++){
			if(1!=file->read(&temp8,1)) return false;
			//R.K. added the S3m.channelRemapping[i]<CHANNELS because otherwise we would write to
			//an unknown memory
			if((temp8&0x20)&&S3m.channelRemapping[i]<CHANNELS) Mixer.channelPanning[S3m.channelRemapping[i]]=temp8&0xF;
		}
	}
	// If stereo flag is not set then make song mono
	if(!(masterVolume&128))
	for(i=0;i<CHANNELS;i++)
		Mixer.channelPanning[i]=0x8;

	// Avoid division by zero for unused instruments
	for(i=0;i<INSTRUMENTS;i++)
	S3m.instruments[i].middleC=8363;
	// Load instruments
	for(i=0;i<S3m.numberOfInstruments;i++){
		// Jump to instrument parapointer and skip filename
		file->seek((S3m.instrumentParapointers[i]<<4),SEEK_SET);				 // Set file position to instrument
		if(1!=file->read(&S3m.instruments[i].type,1)) return false; 		 // Instrument type (0=empty instrument (message only), 1=PCM instrument, 2 and higher - OPL (not supported yet:) ))
		file->seek(12,SEEK_CUR);																				 // skip char[12]	filename	Original instrument filename in DOS 8.3 format, no terminating null
		// Find parapointer to actual sample data (3 bytes)
		if(1!=file->read(&temp8,1)) return false;		               			 // High byte
		if(2!=file->read(&temp16,2)) return false;	      			    	   // Low word
		S3m.instruments[i].sampleParapointer=(temp8<<16)|temp16;				 // Sample paraptr
		if(4!=file->read(&S3m.instruments[i].length,4)) return false;		 // Sample length
		if(4!=file->read(&S3m.instruments[i].loopBegin,4)) return false; // Sample loop begin
		if(4!=file->read(&S3m.instruments[i].loopEnd,4)) return false;	 // Sample loop end
		if(1!=file->read(&S3m.instruments[i].volume,1)) return false;		 // Sample volume
		if(1!=file->read(&temp8,1)) return false;												 // Skip one byte
		if(1!=file->read(&S3m.instruments[i].pack,1)) return false;			 // Packing type
		if(1!=file->read(&temp8,1)) return false;			                	 // Instrument flags
		S3m.instruments[i].loop=temp8&0x01;	                             // Loop enable flag
		S3m.instruments[i].isStereo=temp8&0x02;													 // Stereo sample flag
    S3m.instruments[i].is16bit=temp8&0x04;	                         // 16-bit sample flag
		if(S3m.instruments[i].is16bit) S3m.instruments[i].length*=2;		 // If the sample is 16 bits, multiply its length by 2
		if(S3m.instruments[i].isStereo) S3m.instruments[i].length*=2;		 // If the sample is stereo, multiply its length by 2
		if(2!=file->read(&S3m.instruments[i].middleC,2)) return false;	 // Middle C
		if(!S3m.instruments[i].middleC) S3m.instruments[i].middleC=8363; // Avoid division by zero
		file->seek(14,SEEK_CUR);																				 // Skip 14 bytes (Always zero, used in-memory during playback)
		if(28!=file->read(S3m.instruments[i].name,28)) return false;		 // Sample name (Sample title, for display to user. Must be null-terminated.) Followed by "SCRS" (4 bytes) we skip this
		if(S3m.instruments[i].loopEnd>S3m.instruments[i].length) S3m.instruments[i].loopEnd=S3m.instruments[i].length;
	}
#ifdef DEBUG_LHEADER
	printf("S3M Header Information:\n");
	printf("------------------------\n");
	printf("Song Name: %.28s\n",S3m.name);
	printf("Song Length: %d\n",songLength);
	printf("Number of Instruments: %d\n",S3m.numberOfInstruments);
	printf("Number of Patterns: %d\n",numberOfPatterns);
	printf("Number of Channels: %d\n",S3m.numberOfChannels);
	printf("BPM: %d\n",Player.bpmOriginal);
	printf("Global Volume: %d\n",S3m.globalVolume);
	printf("Initial Speed: %d\n",Player.speed);
	printf("Samples per Tick: %d\n",Player.samplesPerTick);
	printf("Fast Volume Slides: %s\n",S3m.fastVolumeSlides?"Yes":"No");
	
	printf("\nChannel Mapping:\n");
	for(i=0;i<CHANNELS;i++){
		if(S3m.channelRemapping[i]!=255){
			printf("Channel %2d -> %2d (Panning: 0x%X)\n",i,S3m.channelRemapping[i],Mixer.channelPanning[S3m.channelRemapping[i]]);
		}
	}
	
	printf("\nInstrument Information:\n");
	for(i=0;i<S3m.numberOfInstruments;i++){
		printf("Instrument %2d: %.28s\n",i,S3m.instruments[i].name);
		printf("  Length: %d\n",S3m.instruments[i].length);
		printf("  Loop: %s (Begin: %d, End: %d)\n",S3m.instruments[i].loop?"Yes":"No",S3m.instruments[i].loopBegin,S3m.instruments[i].loopEnd);
		printf("  is 16 bit: %s\n", S3m.instruments[i].is16bit?"Yes":"No");
    printf("  Volume: %d\n", S3m.instruments[i].volume);
		printf("  Middle C: %d\n",S3m.instruments[i].middleC);
	}
	
	printf("\nOrder List:\n");
	for(i=0;i<S3m.songLength;i++){
		printf("%d ",S3m.order[i]);
	}
	printf("\n------------------------\n");
#endif

	return true;
}

void AudioGeneratorS3M::LoadSamples(){
	uint8_t i;
	uint32_t fileOffset;
  #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  uint32_t initialPos=file->getPos();
  #endif
	for(i=0;i<S3m.numberOfInstruments;i++){
		fileOffset=(S3m.instruments[i].sampleParapointer<<4)-1;
		if(S3m.instruments[i].length){
			Mixer.sampleBegin[i]=fileOffset;
			Mixer.sampleEnd[i]=fileOffset+S3m.instruments[i].length;
			if(S3m.instruments[i].loop&&S3m.instruments[i].loopEnd-S3m.instruments[i].loopBegin>2){
				Mixer.sampleLoopBegin[i]=fileOffset+(S3m.instruments[i].loopBegin);
				Mixer.sampleLoopLength[i]=(S3m.instruments[i].loopEnd-S3m.instruments[i].loopBegin);
				Mixer.sampleLoopEnd[i]=fileOffset+S3m.instruments[i].loopEnd;
			}else{
				Mixer.sampleLoopBegin[i]=0;
				Mixer.sampleLoopLength[i]=0;
				Mixer.sampleLoopEnd[i]=0;
			}
		}
	}
  #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  //read samples in PSRAM
  for(i=0;i<S3m.numberOfInstruments;i++){
    if(S3m.instruments[i].length&&S3m.instruments[i].type<2){
      S3m.instruments[i].data=(uint8_t*)ps_malloc(S3m.instruments[i].length);
      if(!S3m.instruments[i].data){
        printf("Failed to allocate PSRAM for sample [%d] data\n",i);
        free(S3m.instruments[i].data);
        file->seek(initialPos,SEEK_SET);
        return;
			}
      file->seek(Mixer.sampleBegin[i],SEEK_SET); //set position to sample begin
      if(file->read(S3m.instruments[i].data,S3m.instruments[i].length)!=S3m.instruments[i].length){
        printf("Failed to read raw sample data\n");
        free(S3m.instruments[i].data);
        file->seek(initialPos,SEEK_SET);
        return;
      }
      S3m.instruments[i].isAllocated=true;
    }
    // printf("Free PSRAM: %d bytes after sample [%d]\n",ESP.getFreePsram(),i);
  }
  // printf("Free PSRAM: %d bytes after samples load.\n",ESP.getFreePsram());
  file->seek(initialPos,SEEK_SET);
  #endif
}

bool AudioGeneratorS3M::LoadPattern(uint8_t pattern){
	// printf("Loading Pattern [%d] of [%d] - Current Position: [%d] of [%d]\n",pattern,S3m.numberOfPatterns,Player.orderIndex,S3m.songLength);
	// Serial.printf("%d ",pattern);
	uint8_t row;
	uint8_t channel;
	uint8_t temp8;
	// Clear pattern data first - properly iterate through rows and channels
	for(row=0;row<ROWS;row++)
		for(channel=0;channel<CHANNELS;channel++)
			Player.currentPattern.note[row][channel]=NONOTE;

	memset(Player.currentPattern.instrumentNumber,0,ROWS*CHANNELS);
	memset(Player.currentPattern.volume,NOVOLUME,ROWS*CHANNELS);
	memset(Player.currentPattern.effectNumber,0,ROWS*CHANNELS);
	memset(Player.currentPattern.effectParameter,0,ROWS*CHANNELS);

	file->seek((S3m.patternParapointers[pattern]<<4)+2,SEEK_SET);

	row=0;
 	while(row<ROWS){
		if(1!=file->read(&temp8,1)) return false;
		if(temp8){
			uint8_t note,instrument,volume,effectNumber,effectParameter;
			channel=S3m.channelRemapping[temp8&31];
			if(temp8&32){
				if(1!=file->read(&note,1)) return false;
				if(1!=file->read(&instrument,1)) return false;
				// Remap instrument right after reading - following libxmp's approach
				if(instrument<=S3m.numberOfInstruments&&channel<S3m.numberOfChannels){
					switch(note){
						case 255:
							Player.currentPattern.note[row][channel]=NONOTE;
							break;
						case 254:
							Player.currentPattern.note[row][channel]=KEYOFF;
							break;
						default:
							Player.currentPattern.note[row][channel]=(note>>4)*12+(note&0xF);
					}
					Player.currentPattern.instrumentNumber[row][channel]=instrument;
				}
			}
			if(temp8&64){
				uint8_t volume;
				if(1!=file->read(&volume,1)) return false;
				if(instrument<=S3m.numberOfInstruments&&channel<S3m.numberOfChannels)
					Player.currentPattern.volume[row][channel]=volume;
			}
			if(temp8&128){
				if(1!=file->read(&effectNumber,1)) return false;
				if(1!=file->read(&effectParameter,1)) return false;
				if(instrument<=S3m.numberOfInstruments&&channel<S3m.numberOfChannels){
					Player.currentPattern.effectNumber[row][channel]=effectNumber;
					Player.currentPattern.effectParameter[row][channel]=effectParameter;
				}
			}
		}else
			row++;
	}
#ifdef DEBUG_LPATTERN
	printf("\nPattern %d Data:\n", pattern);
	printf("------------------------\n");
	// Header for columns
	printf("Row  |");
	for(uint8_t c=0;c<S3m.numberOfChannels;c++){
		printf(" Ch%-2d |", c);
	}
	printf("\n");
	// Separator line
	for(uint8_t c=0;c<=S3m.numberOfChannels;c++){
		printf("------");
	}
	printf("\n");
	// Print pattern data
	for(uint8_t r=0;r<ROWS;r++){
		printf("%3d  |",r);  // Row number
		for(uint8_t c=0;c<S3m.numberOfChannels;c++){
			// Print note, instrument, volume and effect for each cell
			if(Player.currentPattern.note[r][c]==NONOTE){
				printf(" ... ");
			}else if(Player.currentPattern.note[r][c]==KEYOFF){
				printf(" OFF ");
			}else{
				// Convert note number to note name (C-4, F#3, etc.)
				uint8_t octave=Player.currentPattern.note[r][c]/12;
				uint8_t note=Player.currentPattern.note[r][c]%12;
				const char* noteNames[12]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
				printf("%s%1d ",noteNames[note],octave);
			}
			// Print instrument number if present
			if(Player.currentPattern.instrumentNumber[r][c]){
				printf("%2d",Player.currentPattern.instrumentNumber[r][c]);
			}else{
				printf("..");
			}
			// Print volume if present
			if(Player.currentPattern.volume[r][c]!=NOVOLUME){
				printf("v%2d",Player.currentPattern.volume[r][c]);
			}else{
				printf("v..");
			}
			// Print effect if present
			if(Player.currentPattern.effectNumber[r][c]||Player.currentPattern.effectParameter[r][c]){
				printf("%1X%02X|",Player.currentPattern.effectNumber[r][c],Player.currentPattern.effectParameter[r][c]);
			}else{
				printf("...|");
			}
		}
		printf("\n");
		// Add separator every 4 rows for better readability
		if((r+1)%4==0){
			for(uint8_t c=0;c<=S3m.numberOfChannels;c++){
				printf("------");
			}
			printf("\n");
		}
	}
	printf("------------------------\n");
#endif
	return true;
}

void AudioGeneratorS3M::Portamento(uint8_t channel){
	if(Player.lastAmigaPeriod[channel]==0) return;  // Add safety check

	if(Player.lastAmigaPeriod[channel]<Player.portamentoNote[channel]){
		Player.lastAmigaPeriod[channel]+=Player.portamentoSpeed[channel]<<2;
		if(Player.lastAmigaPeriod[channel]>Player.portamentoNote[channel])
			Player.lastAmigaPeriod[channel]=Player.portamentoNote[channel];
	}
	if(Player.lastAmigaPeriod[channel]>Player.portamentoNote[channel]){
		Player.lastAmigaPeriod[channel]-=Player.portamentoSpeed[channel]<<2;
		if(Player.lastAmigaPeriod[channel]<Player.portamentoNote[channel])
			Player.lastAmigaPeriod[channel]=Player.portamentoNote[channel];
	}
	// Add safety check before frequency calculation
	if(Player.lastAmigaPeriod[channel]>0){
		Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
	}
}

void AudioGeneratorS3M::Vibrato(uint8_t channel, bool fine){
	uint16_t delta;
	uint16_t temp;

	temp=Player.vibratoPos[channel]&31;
	switch(Player.waveControl[channel]&3){
		case 0:
			delta=ReadSine(temp);
			break;
		case 1:
			temp<<=3;
			if(Player.vibratoPos[channel]<0)
				temp=255-temp;
			delta=temp;
			break;
		case 2:
			delta=255;
			break;
		case 3:
			delta=rand()&255;
			break;
	}

	delta*=Player.vibratoDepth[channel];
	if(fine) delta>>=7;
	else delta>>=5;

	// Add safety check before frequency calculations
	if(Player.lastAmigaPeriod[channel]!=0){
		if(Player.vibratoPos[channel]>=0) Mixer.channelFrequency[channel] = Player.amiga/(Player.lastAmigaPeriod[channel]+delta);
		else Mixer.channelFrequency[channel]=Player.amiga/(Player.lastAmigaPeriod[channel]-delta);
	}

	Player.vibratoPos[channel]+=Player.vibratoSpeed[channel];
	if(Player.vibratoPos[channel]>31) Player.vibratoPos[channel]-=64;
}

void AudioGeneratorS3M::Tremolo(uint8_t channel){
	uint16_t delta;
	uint16_t temp;

	temp=Player.tremoloPos[channel]&31;

	switch(Player.waveControl[channel]&3){
		case 0:
			delta=ReadSine(temp);
			break;
		case 1:
			temp<<=3;
			if(Player.tremoloPos[channel]<0)
				temp=255-temp;
			delta=temp;
			break;
		case 2:
			delta=255;
			break;
		case 3:
			delta=rand()&255;
			break;
	}

	delta*=Player.tremoloDepth[channel];
	delta>>=6;

	if(Player.tremoloPos[channel]>=0){
		if(Player.volume[channel]+delta>64) delta=64-Player.volume[channel];
		Mixer.channelVolume[channel]=Player.volume[channel]+delta;
	}else{
		if(Player.volume[channel]-delta<0) delta=Player.volume[channel];
		Mixer.channelVolume[channel]=Player.volume[channel]-delta;
	}

	Player.tremoloPos[channel]+=Player.tremoloSpeed[channel];
	if(Player.tremoloPos[channel]>31) Player.tremoloPos[channel]-=64;
}

void AudioGeneratorS3M::Tremor(uint8_t channel){
	uint8_t on=(Player.tremorOnOff[channel]>>4)+1;
	uint8_t off=(Player.tremorOnOff[channel]&0xF)+1;

	Player.tremorCount[channel]%=on+off;
	if(Player.tremorCount[channel]<on) Mixer.channelVolume[channel]=Player.volume[channel];
	else Mixer.channelVolume[channel]=0;
	Player.tremorCount[channel]++;
}

void AudioGeneratorS3M::VolumeSlide(uint8_t channel){
	if(!(Player.lastVolumeSlide[channel]&0xF)) Player.volume[channel]+=Player.lastVolumeSlide[channel]>>4;
	if(!(Player.lastVolumeSlide[channel]>>4)) Player.volume[channel]-=Player.lastVolumeSlide[channel]&0xF;

	if(Player.volume[channel]>64) Player.volume[channel]=64;
	else if(Player.volume[channel]<0)Player.volume[channel]=0;
	Mixer.channelVolume[channel]=Player.volume[channel];
}

bool AudioGeneratorS3M::ProcessRow(){
	uint8_t jumpFlag;
	uint8_t breakFlag;
	uint8_t channel;
	uint16_t note;
	uint8_t instrumentNumber;
	uint8_t volume;
	uint8_t effectNumber;
	uint8_t effectParameter;
	uint8_t effectParameterX;
	uint8_t effectParameterY;
	uint16_t sampleOffset;
	uint8_t retriggerSample;

	int8_t volSum[96]={0}; // Array for summing records
  int noteCount[96]={0}; // Array for counting the number of notes encountered

	if(!running) return false;

	Player.lastRow=Player.row++;

	jumpFlag=false;
	breakFlag=false;
	for(channel=0;channel<S3m.numberOfChannels;channel++){
		note=Player.currentPattern.note[Player.lastRow][channel];
		instrumentNumber=Player.currentPattern.instrumentNumber[Player.lastRow][channel];
		volume=Player.currentPattern.volume[Player.lastRow][channel];
		effectNumber=Player.currentPattern.effectNumber[Player.lastRow][channel];
		effectParameter=Player.currentPattern.effectParameter[Player.lastRow][channel];
		effectParameterX=effectParameter>>4;
		effectParameterY=effectParameter&0xF;
		sampleOffset=0;

		if(instrumentNumber){
			if(instrumentNumber>S3m.numberOfInstruments){
				// printf("Invalid instrument number [%d]!\n",instrumentNumber);
				instrumentNumber=S3m.numberOfInstruments;
			}
			Player.lastInstrumentNumber[channel]=instrumentNumber-1;
			if(!(effectParameter==0x13&&effectParameterX==NOTEDELAY)){
				Player.volume[channel]=S3m.instruments[Player.lastInstrumentNumber[channel]].volume;
			}
		}
	
		if(note<NONOTE){
			Player.lastNote[channel]=note;
			Player.amigaPeriod[channel]=ReadAmigaPeriods(note)*8363/S3m.instruments[Player.lastInstrumentNumber[channel]].middleC;
			// Player.amigaPeriod[channel]=CalculateAmigaPeriod(Player.lastNote[channel],Player.lastInstrumentNumber[channel]);

			if(effectNumber!=TONEPORTAMENTO&&effectNumber!=PORTAMENTOVOLUMESLIDE)
				Player.lastAmigaPeriod[channel]=Player.amigaPeriod[channel];

			if(!(Player.waveControl[channel]&0x80)) Player.vibratoPos[channel]=0;
			if(!(Player.waveControl[channel]&0x08)) Player.tremoloPos[channel]=0;
			Player.tremorCount[channel]=0;

			retriggerSample=true;
		} else retriggerSample=false;

		if(volume<=0x40) Player.volume[channel]=volume;
		if(note==0xFE) Player.volume[channel]=0;

		switch(effectNumber){
			case SETSPEED:
				Player.speed=effectParameter;
				break;
			case JUMPTOORDER:
				// printf("Jumping to order [%d]=[%d] in pattern [%d] in row [%d]\n",effectParameter,S3m.order[effectParameter],Player.orderIndex,Player.row);
				if(S3m.order[effectParameter]!=S3m.order[Player.orderIndex+1]){ // Playing without loop
					if(Player.orderIndex+1>=S3m.songLength) break;
					Player.orderIndex++;
					Player.row=0;
					jumpFlag=true;
					break;
				}
				Player.orderIndex=effectParameter;
				if(Player.orderIndex>=S3m.songLength){
					// printf("Possible End 1?\n");
					Player.orderIndex=0;
				}
				Player.row=0;
				jumpFlag=true;
				break;
			case BREAKPATTERNTOROW:
				// printf("Breaking pattern to row [%d] in pattern [%d] in row [%d]\n",effectParameter,Player.orderIndex,Player.row);
				Player.row=effectParameterX*10+effectParameterY;
				if(Player.row>=ROWS) Player.row=0;
				if(!jumpFlag&&!breakFlag){
					Player.orderIndex++;
					// printf("\033[2J");
					// printf("\033[0;0H");
					// printf("Current Song: [%s] - Number of Channels [%d]\n",S3m.name,S3m.numberOfChannels);
					// printf("Current Position: [%d] of [%d]\n",Player.orderIndex,S3m.songLength);
					if(Player.orderIndex>=S3m.songLength){
						// printf("Possible End 2?\n");
						Player.orderIndex=0;
					}
				}
				breakFlag=true;
				break;
			case VOLUMESLIDE:
				if(effectParameter) Player.lastVolumeSlide[channel]=effectParameter;
				if((Player.lastVolumeSlide[channel]&0xF)==0xF) Player.volume[channel]+=Player.lastVolumeSlide[channel]>>4;
				else if(Player.lastVolumeSlide[channel]>>4==0xF) Player.volume[channel]-=Player.lastVolumeSlide[channel]&0xF;
				if(S3m.fastVolumeSlides){
					if(!(Player.lastVolumeSlide[channel]&0xF)) Player.volume[channel]+=Player.lastVolumeSlide[channel]>>4;
					if(!(Player.lastVolumeSlide[channel]>>4)) Player.volume[channel]-=Player.lastVolumeSlide[channel]&0xF;
				}
				if(Player.volume[channel]>64) Player.volume[channel]=64;
				else if(Player.volume[channel]<0) Player.volume[channel]=0;
				break;
			case PORTAMENTODOWN:
				if(effectParameter) Player.lastPortamento[channel]=effectParameter;
				if(Player.lastPortamento[channel]>>4==0xF) Player.lastAmigaPeriod[channel]+=(Player.lastPortamento[channel]&0xF)<<2;
				if(Player.lastPortamento[channel]>>4== 0xE)	Player.lastAmigaPeriod[channel]+=Player.lastPortamento[channel]&0xF;
				break;
			case PORTAMENTOUP:
				if(effectParameter) Player.lastPortamento[channel]=effectParameter;
				if(Player.lastPortamento[channel]>>4==0xF) Player.lastAmigaPeriod[channel]-=(Player.lastPortamento[channel]&0xF)<<2;
				if(Player.lastPortamento[channel]>>4==0xE) Player.lastAmigaPeriod[channel]-=Player.lastPortamento[channel]&0xF;
				break;
			case TONEPORTAMENTO:
				if(effectParameter) Player.portamentoSpeed[channel]=effectParameter;
				Player.portamentoNote[channel]=Player.amigaPeriod[channel];
				retriggerSample=false;
				break;
			case VIBRATO:
				if(effectParameterX) Player.vibratoSpeed[channel]=effectParameterX;
				if(effectParameterY) Player.vibratoDepth[channel]=effectParameterY;
				break;
			case TREMOR:
				if(effectParameter) Player.tremorOnOff[channel]=effectParameter;
				Tremor(channel);
				break;
			case VIBRATOVOLUMESLIDE:
				if(effectParameter) Player.lastVolumeSlide[channel]=effectParameter;
				break;
			case PORTAMENTOVOLUMESLIDE:
				if(effectParameter) Player.lastVolumeSlide[channel]=effectParameter;
				Player.portamentoNote[channel]=Player.amigaPeriod[channel];
				retriggerSample = false;
				break;
			case SETSAMPLEOFFSET:
				sampleOffset=effectParameter<<8;
				if(sampleOffset>S3m.instruments[Player.lastInstrumentNumber[channel]].length)
				sampleOffset=S3m.instruments[Player.lastInstrumentNumber[channel]].length;
				break;
			case RETRIGGERNOTEVOLUMESLIDE:
				if(effectParameter){
					Player.retriggerVolumeSlide[channel]=effectParameterX;
					Player.retriggerSpeed[channel]=effectParameterY;
				}
				break;
			case TREMOLO:
				if(effectParameterX) Player.tremoloSpeed[channel]=effectParameterX;
				if(effectParameterY) Player.tremoloDepth[channel]=effectParameterY;
				break;
			case 0x13: // subset effects
				switch(effectParameterX){
					case SETFINETUNE:
						S3m.instruments[Player.lastInstrumentNumber[channel]].middleC=ReadFineTuneToHz(effectParameterY);
						break;
					case SETVIBRATOWAVEFORM:
						Player.waveControl[channel]&=0xF0;
						Player.waveControl[channel]|=effectParameterY;
						break;
					case SETTREMOLOWAVEFORM:
						Player.waveControl[channel]&=0xF;
						Player.waveControl[channel]|=effectParameterY<<4;
						break;
					case SETCHANNELPANNING:
						Mixer.channelPanning[channel]=effectParameterY;
						break;
					case STEREOCONTROL:
						if(effectParameterY>7) effectParameterY-=8;
						else effectParameterY+=8;
						Mixer.channelPanning[channel]=effectParameterY;
						break;
					case PATTERNLOOP:
						// printf("Pattern loop: [%d] times\n", effectParameterY);
						if(effectParameterY){
							if(Player.patternLoopCount[channel]) Player.patternLoopCount[channel]--;
							else Player.patternLoopCount[channel]=effectParameterY;
							if(Player.patternLoopCount[channel]) Player.row=Player.patternLoopRow[channel]-1;
						} else Player.patternLoopRow[channel]=Player.row;
						break;
					case NOTEDELAY:
						// printf("Note delay: [%d] ticks\n", effectParameterY);
						retriggerSample=false;
						break;
					case PATTERNDELAY:
						// printf("Pattern delay: [%d] ticks\n",effectParameterY);
						Player.patternDelay=effectParameterY;
						break;
				}
				break;
			case SETTEMPO:
				// printf("Set tempo: [%d]\n",effectParameter);
				Player.bpmOriginal=effectParameter;
				// Hz =  samplerate / ( (2 * BPM) / 5 )
				Player.samplesPerTick=sampleRate/((2*effectParameter)/5);
				break;
			case FINEVIBRATO:
				if(effectParameterX) Player.vibratoSpeed[channel]=effectParameterX;
				if(effectParameterY) Player.vibratoDepth[channel]=effectParameterY;
				break;
			case SETGLOBALVOLUME:
				break;
		}

		if(retriggerSample)	Mixer.channelSampleOffset[channel]=sampleOffset<<FIXED_DIVIDER;

		if(retriggerSample||Player.lastAmigaPeriod[channel]
			&&effectNumber!=VIBRATO
			&&effectNumber!=VIBRATOVOLUMESLIDE
			&&!(effectNumber==0x13&&effectParameterX==NOTEDELAY)
			&&effectNumber!=FINEVIBRATO) Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];

		if(instrumentNumber) Mixer.channelSampleNumber[channel]=Player.lastInstrumentNumber[channel];

		if(effectNumber!=TREMOR&&effectNumber!=TREMOLO)	Mixer.channelVolume[channel]=Player.volume[channel];

		switch(channel%4){
      case 0:
      case 3:
        Mixer.channelPanning[channel]=stereoSeparation;
        break;
      default:
        Mixer.channelPanning[channel]=128-stereoSeparation;
    }

    if(buffersInitialized&&note<108){
      int8_t vol=Mixer.channelVolume[channel];
      int noteIndex=(note>95)?note-96:note;
      vol=(vol>64)?64:vol;
      // Map channel to one of the 8 elements in channelEQBuffer
      uint8_t targetIndex=channel%8;
      // Keep track of how many channels have contributed to each buffer element
      static uint8_t channelCount[8]={0};
      // Reset counts at the start of each row processing
      if (channel==0){
        memset(channelCount,0,sizeof(channelCount));
      }
      // Update the buffer with a running average
      channelCount[targetIndex]++;
      // Calculate running average: newAvg = ((oldAvg * (n-1)) + newValue) / n
      if(channelCount[targetIndex]==1){
        // First value for this element
        channelEQBuffer[targetIndex]=vol;
      }else{
        // Average with existing values
        channelEQBuffer[targetIndex]=((channelEQBuffer[targetIndex]*(channelCount[targetIndex]-1))+vol)/channelCount[targetIndex];
      }
      if(noteIndex>=0&&noteIndex<=95){
        volSum[noteIndex]+=vol;
        noteCount[noteIndex]++;
      }
    }
	}

	if(buffersInitialized){
    // Calculate the average volume value for each note
    for(int i=0;i<96;i++){
      if(noteCount[i]>0){
        eqBuffer[i]=(volSum[i]/noteCount[i])>>2;
      }else{
        eqBuffer[i]=0; // Or another default value
      }
    }
  }

	return true;
}

bool AudioGeneratorS3M::ProcessTick(){
	uint8_t channel;
	uint16_t note;
	uint8_t instrumentNumber;
	uint8_t volume;
	uint8_t effectNumber;
	uint8_t effectParameter;
	uint8_t effectParameterX;
	uint8_t effectParameterY;
 
	for(channel=0;channel<S3m.numberOfChannels;channel++){
		if(Player.lastAmigaPeriod[channel]){
			note=Player.currentPattern.note[Player.lastRow][channel];
			instrumentNumber=Player.currentPattern.instrumentNumber[Player.lastRow][channel];
			volume=Player.currentPattern.volume[Player.lastRow][channel];
			effectNumber=Player.currentPattern.effectNumber[Player.lastRow][channel];
			effectParameter=Player.currentPattern.effectParameter[Player.lastRow][channel];
			effectParameterX=effectParameter>>4;
			effectParameterY=effectParameter&0xF;

			switch(effectNumber){
				case VOLUMESLIDE:
					VolumeSlide(channel);
					break;
				case PORTAMENTODOWN:
					if(Player.lastPortamento[channel]<0xE0) Player.lastAmigaPeriod[channel]+=Player.lastPortamento[channel]<<2;
					Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
					break;
				case PORTAMENTOUP:
					if(Player.lastPortamento[channel]<0xE0)
					Player.lastAmigaPeriod[channel]-=Player.lastPortamento[channel]<<2;
					Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
					break;
				case TONEPORTAMENTO:
					Portamento(channel);
					break;
				case VIBRATO:
					Vibrato(channel,false); // Fine = false
					break;
				case TREMOR:
					Tremor(channel);
					break;
				case ARPEGGIO:
					if(effectParameter)
					switch(Player.tick%3){
						case 0:
							Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
							break;
						case 1:
							Mixer.channelFrequency[channel]=Player.amiga/ReadAmigaPeriods(Player.lastNote[channel]+effectParameterX);
							// Mixer.channelFrequency[channel]=Player.amiga/CalculateAmigaPeriod(Player.lastNote[channel]+effectParameterX,Player.lastInstrumentNumber[channel]);
							break;
						case 2:
							Mixer.channelFrequency[channel]=Player.amiga/ReadAmigaPeriods(Player.lastNote[channel]+effectParameterY);
							// Mixer.channelFrequency[channel]=Player.amiga/CalculateAmigaPeriod(Player.lastNote[channel]+effectParameterY,Player.lastInstrumentNumber[channel]);
							break;
					}
					break;
				case VIBRATOVOLUMESLIDE:
					Vibrato(channel,false); // Fine = false
					VolumeSlide(channel);
					break;
				case PORTAMENTOVOLUMESLIDE:
					Portamento(channel);
					VolumeSlide(channel);
					break;
				case RETRIGGERNOTEVOLUMESLIDE:
					if(!Player.retriggerSpeed[channel]) break;
					if(!(Player.tick%Player.retriggerSpeed[channel])){
						if(Player.retriggerVolumeSlide[channel]){
							switch(Player.retriggerVolumeSlide[channel]){
								case 1:
									Player.volume[channel]--;
									break;
								case 2:
									Player.volume[channel]-=2;
									break;
								case 3:
									Player.volume[channel]-=4;
									break;
								case 4:
									Player.volume[channel]-=8;
									break;
								case 5:
									Player.volume[channel]-=16;
									break;
								case 6:
									Player.volume[channel]*=2/3;
									break;
								case 7:
									Player.volume[channel]>>=1;
									break;
								case 9:
									Player.volume[channel]++;
									break;
								case 0xA:
									Player.volume[channel]+=2;
									break;
								case 0xB:
									Player.volume[channel]+=4;
									break;
								case 0xC:
									Player.volume[channel]+=8;
									break;
								case 0xD:
									Player.volume[channel]+=16;
									break;
								case 0xE:
									Player.volume[channel]*=3/2;
									break;
								case 0xF:
									Player.volume[channel]<<=1;
									break;
							}
							if(Player.volume[channel]>64) Player.volume[channel]=64;
							else if(Player.volume[channel]<0) Player.volume[channel]=0;
							Mixer.channelVolume[channel]=Player.volume[channel];
						}
						Mixer.channelSampleOffset[channel]=0;
					}
					break;
				case TREMOLO:
					Tremolo(channel);
					break;
				case 0x13: // subset effects
					switch(effectParameterX){
						case NOTECUT:
							if(Player.tick==effectParameterY)	Mixer.channelVolume[channel]=Player.volume[channel]=0;
							break;
						case NOTEDELAY:
							if(Player.tick==effectParameterY){
								if(instrumentNumber) Player.volume[channel]=S3m.instruments[Player.lastInstrumentNumber[channel]].volume;
								if(volume<=0x40) Player.volume[channel]=volume;
								if(note<NONOTE) Mixer.channelSampleOffset[channel]=0;
								Mixer.channelFrequency[channel]=Player.amiga/Player.lastAmigaPeriod[channel];
								Mixer.channelVolume[channel]=Player.volume[channel];
							}
							break;
					}
					break;
				case FINEVIBRATO:
					Vibrato(channel,true); // Fine = true
					break;
			}
		}
	}

	return true;
}

bool AudioGeneratorS3M::RunPlayer(){
	if(!running) return false;

	if(trackFrameInitialized&&sampleRate>0){
    // Calculate how many 1/50th second frames should pass for this tick
    // One tick duration in seconds = Player.samplesPerTick / sampleRate
    // Number of 1/50th frames = (samplesPerTick / sampleRate) * 50
    float secondsPerTick=(float)Player.samplesPerTick/(float)sampleRate;
    float trackFramesPerTick=secondsPerTick*50.0f;
    // Accumulate and round to nearest whole number
    static float frameAccumulator=0.0f;
    frameAccumulator+=trackFramesPerTick;
    if(frameAccumulator>=1.0f){
      int framesToAdd=(int)frameAccumulator;
      (*trackFrame)+=framesToAdd;
      frameAccumulator-=framesToAdd;
    }
  }

	if(Player.tick==Player.speed){
    Player.tick=0;

    if(Player.row==ROWS){
      Player.orderIndex++;
      if (Player.orderIndex==S3m.songLength){
        //Player.orderIndex = 0;
        // No loop, just say we're done!
        return false;
      }
      Player.row=0;
    }

    if (Player.patternDelay){
      Player.patternDelay--;
    }else{
      if(Player.orderIndex!=Player.oldOrderIndex)
        if(!LoadPattern(S3m.order[Player.orderIndex])) return false;
      Player.oldOrderIndex=Player.orderIndex;
      if(!ProcessRow())return false;
    }
  }else{
    if(!ProcessTick()) return false;
  }
  Player.tick++;
  return true;
}

void AudioGeneratorS3M::GetSample(int16_t sample[2]) {
	if(!running||!file||!output||stopping||bufferFreed||isPaused){
		sample[AudioOutput::LEFTCHANNEL]=sample[AudioOutput::RIGHTCHANNEL]=0;
		return;
	}

	int32_t sumL=0;
	int32_t sumR=0;

	for(uint8_t channel=0;channel<S3m.numberOfChannels;channel++){
		if(!Mixer.channelFrequency[channel]
			||!FatBuffer.channels[channel]
			||!S3m.instruments[Mixer.channelSampleNumber[channel]].length
			||S3m.instruments[Mixer.channelSampleNumber[channel]].type>=2	// Skip if OPL sample type
			||!Player.lastAmigaPeriod[channel]
			||!Mixer.channelVolume[channel]) {
			continue;
		}

		bool is16bit=S3m.instruments[Mixer.channelSampleNumber[channel]].is16bit;
		uint32_t freq=Mixer.channelFrequency[channel];
		Mixer.channelSampleOffset[channel]+=freq;

		uint32_t samplePointer=Mixer.sampleBegin[Mixer.channelSampleNumber[channel]];
		uint32_t sampleOffset=Mixer.channelSampleOffset[channel]>>FIXED_DIVIDER;
		samplePointer+=is16bit?sampleOffset*2:sampleOffset;

		const uint32_t loopLength=Mixer.sampleLoopLength[Mixer.channelSampleNumber[channel]];
		if(loopLength){
			uint32_t sampleLoopEnd = Mixer.sampleLoopEnd[Mixer.channelSampleNumber[channel]];
			// if(is16bit){
			//   sampleLoopEnd=Mixer.sampleBegin[Mixer.channelSampleNumber[channel]]+loopLength;
			// }
			if(samplePointer>=sampleLoopEnd){
				Mixer.channelSampleOffset[channel]-=(loopLength<<FIXED_DIVIDER);
				samplePointer=Mixer.sampleBegin[Mixer.channelSampleNumber[channel]]+((Mixer.channelSampleOffset[channel]>>FIXED_DIVIDER)*(is16bit?2:1));
			}
		}else{
			uint32_t sampleEnd=Mixer.sampleEnd[Mixer.channelSampleNumber[channel]];
			// if(is16bit){
			//   sampleEnd=Mixer.sampleBegin[Mixer.channelSampleNumber[channel]]+S3m.instruments[Mixer.channelSampleNumber[channel]].length*2;
			// }
			if(samplePointer>=sampleEnd){
				Mixer.channelFrequency[channel]=0;
				continue;
			}
		}

		const uint32_t bufOffset=samplePointer-FatBuffer.samplePointer[channel];
		if(bufOffset>=fatBufferSize-(is16bit?4:2)||Mixer.channelSampleNumber[channel]!=FatBuffer.channelSampleNumber[channel]){
			const uint32_t toRead=min(Mixer.sampleEnd[Mixer.channelSampleNumber[channel]]-samplePointer+1,(uint32_t)fatBufferSize);
			if(!file->seek(samplePointer,SEEK_SET)||toRead!=file->read(FatBuffer.channels[channel],toRead)){
				continue;
			}
			FatBuffer.samplePointer[channel]=samplePointer;
			FatBuffer.channelSampleNumber[channel]=Mixer.channelSampleNumber[channel];
		}

		uint8_t* buf=FatBuffer.channels[channel];
		const uint32_t pos=samplePointer-FatBuffer.samplePointer[channel];
		int32_t sampleValue;

		if(is16bit){
			uint16_t raw_current=(buf[pos]<<8)|buf[pos+1];
			uint16_t raw_next=(buf[pos+2]<<8)|buf[pos+3];
			int16_t current,next;
			if(S3m.signedSample){
				current=(int16_t)raw_current;
				next=(int16_t)raw_next;
			}else{
				current=(int16_t)(raw_current-0x8000);
				next=(int16_t)(raw_next-0x8000);
			}
			const uint16_t frac=(Mixer.channelSampleOffset[channel]&((1<<FIXED_DIVIDER)-1))>>(FIXED_DIVIDER-8);
			sampleValue=current+((next-current)*frac>>8);
		}else{
			uint8_t raw_current=buf[pos];
			uint8_t raw_next=buf[pos+1];
			int16_t current,next;
			if(S3m.signedSample){
				current=((int16_t)raw_current)<<8;
				next=((int16_t)raw_next)<<8;
			}else{
				current=((int16_t)(raw_current-0x80))<<8;
				next=((int16_t)(raw_next-0x80))<<8;
			}
			const uint16_t frac=(Mixer.channelSampleOffset[channel]&((1<<FIXED_DIVIDER)-1))>>(FIXED_DIVIDER-8);
			sampleValue=current+((next-current)*frac>>8);
		}

		int32_t vol=constrain(Mixer.channelVolume[channel],0,64);
		const int32_t panL=min(0x80-Mixer.channelPanning[channel],64);
		const int32_t panR=min(Mixer.channelPanning[channel],64);

		int64_t scaledL=(int64_t)sampleValue*vol*panL;
		int64_t scaledR=(int64_t)sampleValue*vol*panR;
	#if defined(USE_EXTERNAL_DAC)||defined(CONFIG_IDF_TARGET_ESP32S3)
		sumL+=scaledL>>15;
		sumR+=scaledR>>15;
	#else
		sumL+=scaledL>>14;
		sumR+= scaledR>>14;
	#endif
	}
	sample[AudioOutput::LEFTCHANNEL]=(int16_t)constrain(sumL,INT16_MIN,INT16_MAX);
	sample[AudioOutput::RIGHTCHANNEL]=(int16_t)constrain(sumR,INT16_MIN,INT16_MAX);
}

bool AudioGeneratorS3M::LoadS3M(){
	uint8_t channel;

	if(!LoadHeader()) return false;
	LoadSamples();
 
	Player.amiga=AMIGA;
	Player.tick=Player.speed;
	Player.row=0;
 
	Player.orderIndex=0;
	Player.oldOrderIndex=0xFF;
	Player.patternDelay=0;
 
	for(channel=0;channel<S3m.numberOfChannels;channel++){
		Player.patternLoopCount[channel]=0;
		Player.patternLoopRow[channel]=0;

		Player.lastAmigaPeriod[channel]=0;

		Player.waveControl[channel]=0;

		Player.vibratoSpeed[channel]=0;
		Player.vibratoDepth[channel]=0;
		Player.vibratoPos[channel]=0;

		Player.tremoloSpeed[channel]=0;
		Player.tremoloDepth[channel]=0;
		Player.tremoloPos[channel]=0;

		Player.tremorOnOff[channel]=0;
		Player.tremorCount[channel]=0;

		Mixer.channelSampleOffset[channel]=0;
		Mixer.channelFrequency[channel]=0;
		Mixer.channelVolume[channel]=0;
		switch(channel%4){
      case 0:
      case 3:
        Mixer.channelPanning[channel]=stereoSeparation;
        break;
      default:
        Mixer.channelPanning[channel]=128-stereoSeparation;
    }
	}

	return true;
}

// Aditional protected
uint16_t AudioGeneratorS3M::CalculateAmigaPeriod(uint8_t note,uint8_t instrument){
	// Check for valid note and instrument
	// if(note>=96||instrument>=S3m.numberOfInstruments){
	// 	printf("returns amiga period ZERO!\n");
	// 	return 0;
	// }
	if(note>=96) printf("Note over 96! - [%d]\n",note);
	// ST3 base periods for octave 4 (C-4 to B-4) - 4 octave is middle!
	static const uint16_t st3_periods[12]={
	//C-4,  C#4,  D-4,  D#4,  E-4,  F-4,  F#4,  G-4,  G#4,  A-4,  A#4, B-4
		1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 907
	};
	// Calculate octave and note within octave
	uint8_t octave=note/12;
	uint8_t note_in_octave=note%12;
	// Get base period for the note
	uint32_t period=st3_periods[note_in_octave];

	// Adjust for octave difference from octave 4
	if(octave>4){
		period>>=(octave-4);  // Higher octaves = lower period
	}else if(octave<4){
		period<<=(4-octave);  // Lower octaves = higher period
	}
	// When octave == 4, no shift needed

	// Get the C4 frequency (C2SPD) for this instrument
	uint32_t c4_freq=S3m.instruments[instrument].middleC;
	if(c4_freq==0){
		c4_freq=8363;  // Default C4 frequency
	}
	// Apply C4 frequency adjustment
	period=(period*8363UL)/c4_freq;
	// Clamp to valid S3M range
	if(period<14) period=14;
	if(period>27392) period=27392;
	// Returns calculated period
	return (uint16_t)period;
}

void AudioGeneratorS3M::removeExtraSpaces(char* str){
  char* dest=str;
  bool inSpace=false;
  while(*str !='\0'){
    if(*str!=' '||(inSpace==false)){
      *dest++=*str;
    }
    inSpace=(*str==' ');
    str++;
  }
  *dest='\0';
  // Remove spaces at the end of a line
  if(dest>str&&*(dest-1)==' '){
    *(dest-1)='\0';
  }
}

void AudioGeneratorS3M::freeFatBuffer(){
	static bool inCleanup = false;
	if(inCleanup) return;
	inCleanup=true;
  // #if !defined(CONFIG_IDF_TARGET_ESP32S3) || !defined(BOARD_HAS_PSRAM)
	if(FatBuffer.channels){
		for(int i=0;i<CHANNELS;i++){
			if(FatBuffer.channels[i]){	// Check if pointer is not NULL
				free(FatBuffer.channels[i]);
				FatBuffer.channels[i]=nullptr;	// Always NULL after free
			}
		}
	}
	memset(&FatBuffer,0,sizeof(FatBuffer));
  // #endif
  #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  for(uint8_t i=0;i<S3m.numberOfInstruments;i++){
    if(S3m.instruments[i].isAllocated&&S3m.instruments[i].data!=nullptr){
      free(S3m.instruments[i].data);
      S3m.instruments[i].data=nullptr;
      S3m.instruments[i].isAllocated=false;
    }
  }
  // printf("Free PSRAM: %d bytes after free buffer\n",ESP.getFreePsram());
  #endif
	bufferFreed=true;
	inCleanup=false;
}

// Aditional public
void AudioGeneratorS3M::setPause(bool pause){
  isPaused=pause;
	vTaskDelay(pdMS_TO_TICKS(10));
}

bool AudioGeneratorS3M::initializeFile(AudioFileSource *source){
	if(!source) return false;
	file = source;
	return true;
}

uint8_t AudioGeneratorS3M::getNumberOfChannels(){
  if(!file) return 0;
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
	LoadHeader();
	// Return position
  file->seek(currentPos,SEEK_SET);
	// Return number of channels
  return S3m.numberOfChannels;
}

void AudioGeneratorS3M::getTitle(char* lfn,size_t maxLen){
  if(!file){
  	lfn[0]='\0'; // If the file is not open, return an empty string
  	return;
  }
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
  // Move the pointer to the beginning of the file
  file->seek(0,SEEK_SET);
  // Read first 28 bytes or less if maxLen is less than 28
  size_t readLen=(maxLen<28)?maxLen:28;
  file->read((uint8_t*)lfn,readLen);
  // Add a trailing zero
  if(readLen<maxLen){
  	lfn[readLen]='\0';
  }else{
  	lfn[maxLen-1]='\0';
  }
  // Return the pointer to its original position
  file->seek(currentPos,SEEK_SET);
}

void AudioGeneratorS3M::getDescription(char* description,size_t maxLen){
  if(!file){
    description[0]='\0'; // If the file is not open, return an empty string
    return;
  }
  // Save the current position of the pointer
  uint32_t currentPos=file->getPos();
  // Buffer for storing sample names
  char sampleDescription[29]={0}; // 28 bytes + 1 for terminating zero
	LoadHeader();
  size_t sampleCount=6; // Number of samples to describe
  // Clear description line
  description[0] = '\0';
  for(size_t i=0;i<sampleCount;i++){
    // Read the sample name
    memcpy(sampleDescription,S3m.instruments[i].name,28);
    sampleDescription[28]='\0'; // Terminate the string with a null character
    // Remove extra spaces in sample name
    removeExtraSpaces(sampleDescription);
    // Добавить название семпла к описанию
    strncat(description,sampleDescription,maxLen-strlen(description)-1);
    // Add space between sample names
    if(i<sampleCount-1){
      strncat(description," ",maxLen-strlen(description)-1);
    }
  }
  // Remove extra spaces in the final description line
  removeExtraSpaces(description);
  // Return the pointer to its original position
  file->seek(currentPos, SEEK_SET);
}

void AudioGeneratorS3M::initEQBuffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer){
  this->eqBuffer=eqBuffer;
  this->channelEQBuffer=channelEQBuffer;
  // Clear buffers
  memset(eqBuffer,0,sizeof(uint8_t));
  memset(channelEQBuffer,0,sizeof(uint8_t));
  buffersInitialized=true; // Set the initialization flag
}

void AudioGeneratorS3M::initTrackFrame(unsigned long* tF){
  if(tF!=nullptr){
    trackFrame=tF;
    *trackFrame=0;  // Initialize the value
    trackFrameInitialized=true;
  }
}

void AudioGeneratorS3M::SetSeparation(int sep){
  stereoSeparation=sep;
  for(int ch=0;ch<S3m.numberOfChannels;ch++){
    switch (ch%4){
      case 0:
      case 3:
        Mixer.channelPanning[ch]=stereoSeparation;
        break;
      default:
        Mixer.channelPanning[ch]=128-stereoSeparation;
    }
  }
}

void AudioGeneratorS3M::setSpeed(uint8_t speed){
  switch(speed){
    case 0:
      sampleRate=samplerateOriginal*2;
      break;
    case 1:
      sampleRate=samplerateOriginal;
      break;
    case 2:
      sampleRate=samplerateOriginal/2;
      break;
    default:
      sampleRate=samplerateOriginal;
      break;
  }
	// Hz =  samplerate / ( (2 * BPM) / 5 )
	Player.samplesPerTick=sampleRate/((2*Player.bpmOriginal)/5);
}

signed long AudioGeneratorS3M::getPlaybackTime(bool oneFiftieth){
  if(!file) return -1;

  // Store position
  uint32_t currentPos=file->getPos();
  // Load header first to get necessary data
  if(!LoadHeader()){
    file->seek(currentPos,SEEK_SET);
    return -1;
  }
  totalSeconds=0;
  float currentBPM=Player.bpmOriginal;
  uint8_t currentSpeed=Player.speed;
  
  uint8_t row=0;
  uint8_t orderIndex=0;
  uint8_t patternDelay=0;
  bool songEnd=false;

  while(!songEnd){
    // Process pattern delay if active
    if(patternDelay>0){
      float rowTime=(currentSpeed*0.02f)*(125.0f/currentBPM);
      totalSeconds+=rowTime*patternDelay;
      patternDelay=0;
    }
    // Position file pointer to start of pattern
    file->seek((S3m.patternParapointers[S3m.order[orderIndex]]<<4)+2,SEEK_SET);
    // Read pattern data row by row
    while(row<ROWS){
      uint8_t temp8;
      if(1!=file->read(&temp8,1)) break;
      if(temp8){
        uint8_t channel=S3m.channelRemapping[temp8&31];
				// Skip note and instrument if present
				if(temp8&32)file->seek(2,SEEK_CUR);
				// Skip volume if present
				if(temp8&64)file->seek(1,SEEK_CUR);
				// Read effect if present
				if(temp8&128){
					uint8_t effect, effectParam;
					if(1!=file->read(&effect,1)) break;
					if(1!=file->read(&effectParam,1)) break;
					// Process only timing-related effects
					switch(effect){
						case SETSPEED:
							if(effectParam>0) currentSpeed=effectParam;
							break;
						case SETTEMPO:
							if(effectParam>0) currentBPM=effectParam;
							break;
						case JUMPTOORDER:
							if(S3m.order[effectParam]!=S3m.order[orderIndex+1]) break; // Calculating without loop
							if(effectParam<S3m.songLength){
								orderIndex=effectParam;
								row=0;
								goto nextOrder;
							}
							break;
						case BREAKPATTERNTOROW:
							row=((effectParam>>4)*10+(effectParam&0xF));
							if(row>=ROWS) row=0;
							orderIndex++;
							if(orderIndex>=S3m.songLength) songEnd=true;
							goto nextOrder;
							break;
						case 0x13:  // Special effects
							if((effectParam>>4)==PATTERNDELAY){
								patternDelay=effectParam&0x0F;
							}
							break;
					}
				}
      }else{
        // Empty row marker - move to next row
        row++;
        // Calculate time for this row
        float rowTime=(currentSpeed*0.02f)*(125.0f/currentBPM);
        totalSeconds+=rowTime;
      }
    }
    // End of pattern
    row=0;
    orderIndex++;
    if(orderIndex>=S3m.songLength) songEnd=true;

nextOrder:
    continue;
  }
  // Return position
  file->seek(currentPos,SEEK_SET);
  // Apply factor
  signed long factor=(oneFiftieth)?50:1000;
  return static_cast<signed long>(totalSeconds*factor);
}
