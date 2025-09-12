
#pragma once

#include "main.h"
#include "xm.h"
#include "AudioFileSource.h"
#include "AudioOutputI2S.h"

// Undefine Arduino macros before including SdFat
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Redefine for compatibility if needed
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

enum
{
  XM_TASK_INIT,
  XM_TASK_PLAY,
  XM_TASK_STOP,
  XM_TASK_SET_POS,
};

typedef struct
{
  u8 task;
  int handle;
} XM_TASK;

typedef struct
{
  u8 task;
  xm_context_t *ctx;
  bool loop_enabled;
} PLAYER_TASK;

extern "C" {
  int xm_get_stereo_separation();
}

extern QueueHandle_t xm_queue;
void XMPlaybackControl(u8 control);
extern QueueHandle_t player_queue;

extern PLAYER_TASK t;

void initialize_xm(AudioOutput *out,bool playerTask=true);
int xmCTX(AudioFileSource* file,size_t fileSize);
bool xm_send_to_dac();
void xm_player_loop();
void stop_xm();

const char* xm_get_title();
uint8_t xm_get_channels();
void xm_get_description(char* description,size_t maxLen);
void xm_set_speed(uint8_t speed);
void xm_set_separation(int separation);
void xm_init_track_frame(unsigned long* trackFrame);
signed long xm_get_playback_time(bool oneFiftieth=true);
void xm_set_loop(bool loop=false);
