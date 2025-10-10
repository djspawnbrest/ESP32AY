#include <AudioFileSourceSDFAT.h>
#include <SPI.h>
#include <Wire.h>

#define VERSION         "3.5"

#define CLK_SPECTRUM  	1773400
#define CLK_PENTAGON  	1750000
#define CLK_MSX			    1789772
#define CLK_CPC			    1000000
#define CLK_ATARIST		  2000000

#define FPS_PENTAGON  	48828
#define FPS_SPECTRUM  	50000
#define FPS_NTSC	  	  60000

#define CFG_FILENAME  "/config.cfg"

#define TIMER_RATE  44100

#ifdef USE_EXTERNAL_DAC
#define PIN_BCK 25
#define PIN_DIN 26
#define PIN_LCK 27
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define PIN_BCK 4
#define PIN_DIN 5
#define PIN_LCK 6
#endif

// SD configs
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define SD_CS    10
#define SD_MOSI  11
#define SD_SCK   12
#define SD_MISO  13
#define SD_CONFIG SdSpiConfig(SD_CS,DEDICATED_SPI,SD_SCK_MHZ(50)) // 39 max
#else
#define SD_CONFIG SdSpiConfig(SS,DEDICATED_SPI,SD_SCK_MHZ(30)) // 39 max
#endif

#define MAX_PATH  260

// Declare the semaphore handle
SemaphoreHandle_t sdCardSemaphore=NULL;
SemaphoreHandle_t outSemaphore=NULL;

//--------------------------------------
// count of voltage read
#define READ_CNT 100
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// voltmeter pin
#define VOLTPIN 7
#else
// voltmeter pin
#define VOLTPIN 32
#endif

#if defined(CONFIG_IDF_TARGET_ESP32)
#ifndef USE_EXTERNAL_DAC
//charge sense
#define CHGSENS 27
#endif
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
//charge sense
#define CHGSENS 38
#endif
// internal Vref (need to set)
const float VRef=1.1;
// Input resistive divider ratio (Rh + Rl) / Rl. IN <-[ Rh ]--(analogInPin)--[ Rl ]--|GND
const float VoltMult=(100.0+10.0)/10.0;
// maximum battery charge voltage (need to set)
const float v_max=4.1;
// minimum battery charge voltage (need to set)
const float v_min=3.0;
// voltage
float volt=0;
// charge precent
uint8_t precent=0;
// index of battery sprite
uint8_t spriteIndex=0;
// voltmeter update each ms
#define V_UPD 2000
unsigned long mlsV=0;
unsigned int vUp=V_UPD;
bool batChange=true;
//--------------------------------------
bool keysEvent=false;
bool lcdBlackout=false;
bool scrNotPlayer=false;
bool tftInitialized=false;
unsigned long mlsScr=0;
unsigned long mlsPrevTrack=0;
unsigned long loadingTime=0;
#define PREVTRACKDELAY 2000

#define S_UPD0 6  // browser cur filename
#define S_UPD1 10 // player name
#define S_UPD2 15 // player author
#define S_UPD3 6  // player file
#define S_UPD4 20 // date
// scroll end or start
#define S_UPD_DIR 1000
unsigned int sUpC[5]={S_UPD0,S_UPD1,S_UPD2,S_UPD3,S_UPD4};
unsigned int sUp[5]={S_UPD0,S_UPD1,S_UPD2,S_UPD3,S_UPD4};

unsigned long mls=0;
unsigned long mlsS[5]={0,0,0,0,0};
int sPos[5]={0,0,0,0,0};
bool scrollDir[5]={true,true,true,true,true};

// bool update_list=true;
bool scroll=false;
uint16_t scrollSY=0;

uint8_t modEQchn[8];
uint8_t modChannels=0;
uint8_t modChannelsEQ=0;

SdFs sd_fat;
FsFile sd_dir;
FsFile sd_file;
FsFile sd_play_dir;
FsFile sd_play_file;
AudioFileSourceSDFAT *modFile=NULL;
bool skipMod=false;

uint8_t music_data[48*1024];
uint32_t music_data_size;

char lfn[MAX_PATH]; //common array for all lfn operations
char playFileName[MAX_PATH];
char scrollbuf[MAX_PATH];
char playedFileName[MAX_PATH];
byte fileInfoBuf[128]; // 31 bytes per frame max, 50*31=1550 per sec, 155 per 0.1 sec
char tme[200];

extern bool &ayPlayerAct;

volatile uint32_t frame_cnt=0;
volatile uint32_t frame_div=0;
volatile uint32_t frame_max=TIMER_RATE*1000/48880; // Pentagon int

struct{
  bool isBrowserPlaylist;       // browser mode
  bool isPlayAYL;               // player mode
  int16_t dir_cur;              // Browser cursor pointer
  int16_t play_cur;             // Player pointer
  int16_t dir_cur_prev;         // Previous browser pointer
  int16_t play_prev_cur;        // Previous player pointer
  uint8_t volume;               // Amp current volume 32 (0...63)
  uint16_t play_count_files;    // Player count files in dir
  uint16_t play_cur_start;      // Start from pointer
  char play_dir[MAX_PATH];      // Play dir path
  char active_dir[MAX_PATH];    // Browser active dir path
  char prev_dir[MAX_PATH];      // Previous dir path
  char ayl_file[MAX_PATH];      // Browser ayl file path
  char play_ayl_file[MAX_PATH]; // Current playing playlist file path
}sdConfig;

struct{
  bool zx_int;                  // ZX/PENTAGON
  bool encReverse;
  bool showClock;
  int8_t ay_layout;             // ABC/BCA/ACB e.t.c.
  int8_t play_mode;             // ALL/Shuffle/One
  int8_t scr_bright;            // Screen brightness
  int8_t cfg_cur;               // Config menu pointer
  int8_t cfg_datetime_cur;      // Cursor in datetime menu
  uint8_t volume;               // Amp default volume 32 (0...63)
  uint8_t encType;              // Encoder type
  uint8_t playerSource;         // SD/UART
  uint8_t modStereoSeparation;  // DAC channels panning
  uint16_t scr_timeout;         // Screen off timeout
  uint32_t ay_clock;            // 1.75 MHz e.t.c.
  float batCalib;               // Battery calibration
  float dacGain;
}lfsConfig;

enum{
  PENT_INT=0,
  ZX_INT=1
};

// enum{
//   PLAY_FAST=TIMER_RATE*1000/97656,   //100000  97656
//   PLAY_NORMAL=TIMER_RATE*1000/48828,  //50000   48828
//   PLAY_SLOW=TIMER_RATE*1000/24414     //25000   24414
// };

enum{
  PLAY_SLOW=0,
  PLAY_NORMAL=1,
  PLAY_FAST=2
};

enum{
  PLAY_MODE_ONE=0,
  PLAY_MODE_ALL,
  PLAY_MODE_SHUFFLE,
  PLAY_MODES_ALL,
};

enum{
  MOD_FULLSTEREO=0,
  MOD_HALFSTEREO=32,  // for S3M*2
  MOD_MONO=64,        // for S3M*2
  MOD_SEPARATION_ALL
};

enum{
  SCR_PLAYER=0,
  SCR_BROWSER,
  SCR_CONFIG,
  SCR_RESET_CONFIG,
  SCR_DATETIME,
  SCR_ABOUT,
  SCR_NOFILES,
  SCR_SDEJECT,
  SCR_ALERT,
};

enum{
  YES=0,
  NO
};

struct{
  int8_t msg_cur=0;
  uint8_t music_type;
  int screen_mode=SCR_SDEJECT;
  int prev_screen_mode=SCR_PLAYER;
  bool scr_mode_update[9]={true,true,true,true,true,true,true,true,true};
  bool isPlay=false;
  bool isFinish=true;
  bool autoPlay=true;
  bool isSDeject=true;
  bool isBrowserCommand=false;
  bool isFastForward=false;
  bool isSlowBackward=false;
  unsigned long trackFrame=0;
  char alert_message[128];
  unsigned long alert_start_time;
}PlayerCTRL;

struct AYSongInfo{
  char Author[180]; /* Song author */
  char Name[64]; /* Song name */
  /*un*/signed long Length; /* Song length in 1/50 of second, negative value for unknown/infinite length */
  unsigned long Loop; /* Loop start position */
  void *data; /* used for players */
  void *data1; /* used for players */
  unsigned char *module; /* z80 memory or raw song data */
  unsigned char *module1; /* z80 memory or raw song data */
  unsigned char *file_data; /* z80 memory or raw song data */
  unsigned long file_len; /* file length */
  unsigned long module_len; /* file length */
  bool is_ts; /* 2xay - turbo sound */
};

struct AYSongInfo AYInfo;

enum{
  AY_CHNL_A_FINE=0,
  AY_CHNL_A_COARSE,
  AY_CHNL_B_FINE,
  AY_CHNL_B_COARSE,
  AY_CHNL_C_FINE,
  AY_CHNL_C_COARSE,
  AY_NOISE_PERIOD,
  AY_MIXER,
  AY_CHNL_A_VOL,
  AY_CHNL_B_VOL,
  AY_CHNL_C_VOL,
  AY_ENV_FINE,
  AY_ENV_COARSE,
  AY_ENV_SHAPE,
  AY_GPIO_A, AY_GPIO_B
};

const char* const file_ext_list[]={
  "???",
  "ayl",
  "pt1",
  "pt2",
  "pt3",
  "stc",
  "stp",
  "asc",
  "psc",
  "sqt",
  "ay",
  "psg",
  "rsf",
  "yrg",
  "mod",
  "s3m",
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  "xm",
#endif
};

enum{
  TYPE_UNK=0,
  TYPE_AYL,
  TYPE_PT1,
  TYPE_PT2,
  TYPE_PT3,
  TYPE_STC,
  TYPE_STP,
  TYPE_ASC,
  TYPE_PSC,
  TYPE_SQT,
  TYPE_AY,
  TYPE_PSG,
  TYPE_RSF,
  TYPE_YRG,
  TYPE_MOD,
  TYPE_S3M,
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  TYPE_XM,
#endif
  TYPES_ALL
};

enum{
  LAY_ABC=0,
  LAY_ACB,
  LAY_BAC,
  LAY_BCA,
  LAY_CAB,
  LAY_CBA,
  LAY_ALL
};

enum{
  PLAYER_MODE_SD=0,
  PLAYER_MODE_UART,
  PLAYER_MODE_ALL
};

const char* ay_layout_names[]={"ABC","ACB","BAC","BCA","CAB","CBA"};

void browser_reset_directory();
int browser_check_ext(const char* name);
void ay_set_clock(uint32_t f);
void AY_PlayBuf();
void music_play();
void muteAYBeep();
void setDacGain();
int checkSDonStart();
void playerSourceChange();
uint16_t frameMax(uint8_t speed);
// DAC Tracker formats
void MOD_Loop();
void MOD_Play();
void setModSeparation();
void S3M_Loop();
void S3M_Play();
void setS3mSeparation();
#if defined(CONFIG_IDF_TARGET_ESP32S3)
void XM_Loop();
void XM_Play();
void setXmSeparation();
#endif

//I2C autedect
byte eepAddress=80;   // 0x50  - default
byte ampAddress=96;   // 0x60  - default
byte rtcAddress=104;  // 0x68  - default
bool foundAmp=false,foundRom=false,foundRtc=false;

void i2cInit(){
  Wire.begin();
  byte error,address;
  int nDevices=0;
  for(address=1;address<127;address++){
    Wire.beginTransmission(address);
    error=Wire.endTransmission();
    if(error==0){
      if(address>=eepAddress&&address<=eepAddress+7&&error==0){
        eepAddress=address;
        printf("EEPROM found on address: [%d(0x%02X)] \n",eepAddress,eepAddress);
        foundRom=true;
      }
      if(address>=ampAddress&&address<rtcAddress&&error==0){
        ampAddress=address;
        printf("Amp found on address: [%d(0x%02X)] \n",ampAddress,ampAddress);
        foundAmp=true;
      }
      if(address>=rtcAddress&&address<=rtcAddress+7&&error==0){
        rtcAddress=address;
        printf("RTC found on address: [%d(0x%02X)] \n",rtcAddress,rtcAddress);
        foundRtc=true;
      }
      nDevices++;
      // break;
    }
  }
  if(!foundRom){
    printf("EEPROM not found on I2C\n");
    eepAddress=80;
  }
  if(!foundAmp){
    printf("Amp not found on I2C\n");
    ampAddress=96;
  }
  if(!foundRtc){
    printf("RTC not found on I2C\n");
    rtcAddress=104;
  }
  // Wire.end();
}
