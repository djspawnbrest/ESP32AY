#define VERSION         "3.05"

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
#define SD_CONFIG SdSpiConfig(SS,SHARED_SPI,SD_SCK_MHZ(30))

#define MAX_PATH  260

//--------------------------------------
// count of voltage read
#define READ_CNT  100
// voltmeter pin
#define VOLTPIN 32
//charge sense
#define CHGSENS 27
// internal Vref (need to set)
const float VRef = 1.1;
// Input resistive divider ratio (Rh + Rl) / Rl. IN <-[ Rh ]--(analogInPin)--[ Rl ]--|GND
const float VoltMult = (100 + 10) / 10;
// maximum battery charge voltage (need to set)
const float v_max = 4.1;
// minimum battery charge voltage (need to set)
const float v_min = 3.0;
// voltage
float volt = 0;
// charge precent
uint8_t precent = 0;
// index of battery sprite
uint8_t spriteIndex = 0;
// voltmeter update each ms
#define V_UPD 2000
unsigned long mlsV = 0;
unsigned int vUp = V_UPD;
bool batChange = true;
//--------------------------------------
bool keysEvent = false;
unsigned long mlsScr = 0;

#define S_UPD0 6  // browser cur filename
#define S_UPD1 10 // player name
#define S_UPD2 15 // player author
#define S_UPD3 6  // player file
// scroll end or start
#define S_UPD_DIR 1000
unsigned int sUpC[4]={S_UPD0,S_UPD1,S_UPD2,S_UPD3};
unsigned int sUp[4]={S_UPD0,S_UPD1,S_UPD2,S_UPD3};

unsigned long mls=0;
unsigned long mlsS[4]={0,0,0,0};
int sPos[4]={0,0,0,0};
bool scrollDir[4]={true,true,true,true};

// bool update_list=true;
bool scroll=false;
uint16_t scrollSY=0;

SdFat sd_fat;
SdFile sd_dir;
SdFile sd_file;
SdFile sd_play_dir;
SdFile sd_play_file;

uint8_t music_data[64*1024];
uint32_t music_data_size;

char lfn[MAX_PATH]; //common array for all lfn operations
char playFileName[MAX_PATH];
char scrollbuf[MAX_PATH];
char playedFileName[MAX_PATH];
byte fileInfoBuf[128]; // 31 bytes per frame max, 50*31 = 1550 per sec, 155 per 0.1 sec
char tme[64];

struct {
  uint8_t ay_layout;
  uint32_t ay_clock;
  uint16_t scr_timeout;
  int8_t scr_bright;
  int16_t dir_cur;
  int16_t dir_cur_prev;
  uint8_t play_mode;
  int8_t cfg_cur;
  uint8_t sound_vol;
  uint16_t play_count_files;
  int16_t play_cur;
  int16_t play_prev_cur;
  uint16_t play_cur_start;
  uint8_t volume;
  char play_dir[MAX_PATH];
  char active_dir[MAX_PATH];
  char prev_dir[MAX_PATH];
  char ayl_file[MAX_PATH];  // browser ayl file
  char play_ayl_file[MAX_PATH]; // used for now playing playlist
  bool isBrowserPlaylist; // browser mode
  bool isPlayAYL; // player mode
}Config;

enum {
  PLAY_MODE_ONE=0,
  PLAY_MODE_ALL,
  PLAY_MODE_SHUFFLE,
  PLAY_MODES_ALL,
};

enum {
  VOL_OFF = 0,
  VOL_MIN,
  VOL_MID,
  VOL_MAX,
  VOL_MODES_ALL
};

enum{
  SCR_PLAYER=0,
  SCR_BROWSER,
  SCR_CONFIG,
  SCR_RESET_CONFIG,
  SCR_ABOUT,
  SCR_SDEJECT,
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
  bool scr_mode_update[5]={true,true,true,true,true};
  bool isPlay=false;
  bool isFinish=true;
  bool autoPlay=true;
  bool isSDeject=true;
  bool isBrowserCommand=false;
  bool isFastForward=false;
  bool isSlowBackward=false;
  unsigned long trackFrame=0;
}PlayerCTRL;

struct AYSongInfo{
  char Author[64]; /* Song author */
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

const char* const file_ext_list[] = {
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
  "yrg"
};

enum {
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
  TYPES_ALL
};

enum {
  SFX_CANCEL=0,
  SFX_MOVE,
  SFX_SELECT
};

enum {
  LAY_ABC=0,
  LAY_ACB,
  LAY_BAC,
  LAY_BCA,
  LAY_CAB,
  LAY_CBA,
  LAY_ALL
};

const char* ay_layout_names[] = {"ABC","ACB","BAC","BCA","CAB","CBA"};

void sound_play(int id);
void browser_reset_directory();
int browser_check_ext(const char* name);
void AY_PlayBuf();
void music_play();
void muteAYBeep();
int checkSDonStart();