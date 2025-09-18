
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <esp_crc.h>
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_task_wdt.h"

#include "main.h"
#include "xm.h"
#include "xm_internal.h"
#include "xm_cpp.h"
#include "stats.h"

// #define LIBXM_DEBUG 1

#define LOG_ERROR(fmt, ...) printf("\033[31m[ERROR] " fmt "\033[0m\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("\033[33m[WARN] " fmt "\033[0m\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  printf("\033[32m[INFO] " fmt "\033[0m\n", ##__VA_ARGS__)

using namespace stats;

QueueHandle_t i2s_queue;
QueueHandle_t player_queue;

#define MAX_INSTRUMENTS_DESC  8
#define XM_SAMPLE_RATE        44100
#define XM_FRAME_MS           100
#define XM_SAMPLES_PER_BUFFER (XM_SAMPLE_RATE*XM_FRAME_MS / 1000)
#define XM_BUF_SIZE           (XM_SAMPLES_PER_BUFFER*sizeof(i16)*2)
#define XM_BUF_NUM            3
i16 *xm_buf[XM_BUF_NUM];

void i2s_task(void *arg);
void player_task(void *arg);

TaskHandle_t player_task_handle=NULL;
TaskHandle_t i2s_task_handle=NULL;

int curr_xm_handle;

enum{
  PLAYER_STOP,
  PLAYER_PLAY,
  PLAYER_SET_LOOP,
};

xm_context_s *xm_ctx=NULL;
AudioOutputI2S * output;           // Global I2S handle
AudioFileSource* xm_file=nullptr;  // Global file handle
size_t xm_file_size=0;
size_t current_file_pos=0;
int originalSampleRate=XM_SAMPLE_RATE;
static int xm_stereo_separation=32; // Default like MOD
static unsigned long* xm_track_frame=nullptr;
// EQ and channel buffers
static uint8_t* g_eq_buffer=nullptr;
static uint8_t* g_channel_eq_buffer=nullptr;

#ifdef __cplusplus
extern "C"{
#endif
void xm_reset_logical_samples(void);
#ifdef __cplusplus
}
#endif

extern "C"{
  void xm_update_eq_buffers(void){
    if(!xm_ctx||!g_eq_buffer||!g_channel_eq_buffer) return;
    // // Clear buffers
    // memset(g_eq_buffer,0,96);
    // memset(g_channel_eq_buffer,0,8);
    uint16_t volSum[96]={0};
    uint8_t noteCount[96]={0};
    uint8_t channelCount[8]={0};
    // Process all active channels
    for(uint8_t i=0;i<xm_ctx->module.num_channels;++i){
      xm_channel_context_t* ch=xm_ctx->channels+i;
      if(ch->instrument&&ch->sample&&ch->sample_position>=0&&!ch->muted&&!ch->instrument->muted){
        // Convert XM volume (0.0-1.0) to MOD-style (0-64)
        int8_t vol=(int8_t)(ch->actual_volume*xm_ctx->global_volume*64.0f);
        if(vol>64) vol=64;
        // Calculate note index (XM note range, convert to 0-95)
        int noteIndex=(int)(ch->note+0.5f);
        if(noteIndex>=1&&noteIndex<=96){
          noteIndex--; // Convert to 0-95 range
          // Channel averaging
          uint8_t targetIndex=i%8;
          channelCount[targetIndex]++;
          if(channelCount[targetIndex]==1){
            g_channel_eq_buffer[targetIndex]=vol;
          }else{
            g_channel_eq_buffer[targetIndex]=((g_channel_eq_buffer[targetIndex]*(channelCount[targetIndex]-1))+vol)/channelCount[targetIndex];
          }
          // Note accumulation
          volSum[noteIndex]+=vol;
          noteCount[noteIndex]++;
        }
      }
    }
    // Calculate final note EQ values
    for(int i=0;i<96;i++){
      if(noteCount[i]>0){
        g_eq_buffer[i]=(volSum[i]/noteCount[i])>>2; // Divide by 4 like MOD
      }
    }
  }
}

extern "C"{
  uint8_t* xm_get_eq_buffer(void){
    return g_eq_buffer;
  }
  
  uint8_t* xm_get_channel_eq_buffer(void){
    return g_channel_eq_buffer;
  }
}

extern "C"{
  void xm_set_eq_buffers(uint8_t* eqBuffer,uint8_t* channelEQBuffer){
    g_eq_buffer=eqBuffer;
    g_channel_eq_buffer=channelEQBuffer;
  }
}

extern "C"{
  int xm_get_original_sample_rate(){
    return originalSampleRate;
  }
}

extern "C"{
  unsigned long* xm_get_track_frame_ptr(){
    return xm_track_frame;
  }
}

extern "C"{
  int xm_get_stereo_separation(){
    return xm_stereo_separation;
  }
}

extern "C"{
  bool file_seek_set(size_t offset){
    if(xm_file&&offset!=current_file_pos){
      xm_file->seek(offset,SEEK_SET);
      current_file_pos=offset;
      return true;
    }
    return false;
  }

  bool file_seek_cur(size_t offset){
    if(xm_file&&offset!=0){
      xm_file->seek(offset,SEEK_CUR);
      current_file_pos+=offset;
      return true;
    }
    return false;
  }
  
  uint8_t xm_read_u8(){
    uint8_t val=0;
    if(xm_file&&current_file_pos<xm_file_size){
      xm_file->read(&val,1);
      current_file_pos++;
    }
    return val;
  }
  
  void xm_read_memcpy(void* ptr,size_t length){
    if(xm_file&&current_file_pos+length<=xm_file_size){
      xm_file->read(ptr,length);
      current_file_pos+=length;
    }else{
      memset(ptr,0,length);
    }
  }
}

void *xm_malloc(size_t size){
  // Force PSRAM cleanup before large allocations
  void* cleanup1=heap_caps_malloc(1,MALLOC_CAP_SPIRAM);
  void* cleanup2=heap_caps_malloc(1,MALLOC_CAP_SPIRAM);
  if(cleanup1) heap_caps_free(cleanup1);  // Free the first one too!
  if(cleanup2) heap_caps_free(cleanup2);
  void *ctx_mem=heap_caps_malloc(size,MALLOC_CAP_SPIRAM);
  if(!ctx_mem){
    ctx_mem=heap_caps_malloc(size, MALLOC_CAP_8BIT);
  }
  if(!ctx_mem){
    ctx_mem=malloc(size);
  }
  if(ctx_mem){
    memset(ctx_mem,0,size);
  }
#ifdef LIBXM_DEBUG
  if(!ctx_mem) printf("\n\t\033[31m[ERROR]\tCannot allocate memory for XM context (\033[31m%u\033[0m \033[31mbytes)!\033[0m\n", size);
  else printf("\033[32m[INFO]\tMemory for XM allocated:\033[0m \033[31m0x%08X, %u\033[0m \033[32mbytes\033[0m\n", (unsigned int)ctx_mem, size);
#endif
  return ctx_mem;
}

void cleanup_xm(){
  // Free XM buffers
  for(int i=0;i<XM_BUF_NUM;i++){
    if(xm_buf[i]){
      heap_caps_free(xm_buf[i]);
      xm_buf[i]=nullptr;
    }
  }
  // Delete queues
  if(i2s_queue){
    vQueueDelete(i2s_queue);
    i2s_queue=nullptr;
  }
  if(player_queue){
    vQueueDelete(player_queue);
    player_queue=nullptr;
  }
  // Delete tasks
  if(i2s_task_handle){
    vTaskDelete(i2s_task_handle);
    i2s_task_handle=nullptr;
  }
  if(player_task_handle){
    vTaskDelete(player_task_handle);
    player_task_handle=nullptr;
  }
}

void initialize_xm(AudioOutput *out,bool playerTask){
  // Clean up existing resources first
  cleanup_xm();

  output=(AudioOutputI2S*)out;
  for(int i=0;i<XM_BUF_NUM;i++){
    xm_buf[i]=(i16*)heap_caps_malloc(XM_BUF_SIZE, MALLOC_CAP_SPIRAM);
    assert(xm_buf[i]);
  }

  // xm_queue=xQueueCreate(XM_BUF_NUM+1, sizeof(XM_TASK));
  i2s_queue=xQueueCreate(XM_BUF_NUM-2,sizeof(int));
  player_queue=xQueueCreate(2,sizeof(PLAYER_TASK));

  // xTaskCreatePinnedToCore(xm_task, "xm-helper", 4096, NULL, 20, NULL, 0);     // XM helper tasks

  xTaskCreatePinnedToCore(i2s_task,"i2s-writer",4096,NULL,20,&i2s_task_handle,1);   // I2S DAC writer
  if(playerTask){
    xTaskCreatePinnedToCore(player_task,"player",3072,NULL,22,&player_task_handle,0);    // XM renderer, libxm (should work on a separate core)
  }
#ifdef LIBXM_DEBUG
  LOG_INFO("\tI2S initialized");
#endif
}

int xmCTX(AudioFileSource* file,size_t fileSize){
  xm_file=file;
  xm_file_size=fileSize;
  // Reset logical samples counter for new track
  xm_reset_logical_samples();
  int rc=xm_create_context_safe(&xm_ctx,nullptr,fileSize,XM_SAMPLE_RATE,xm_malloc);
  switch(rc){
    case -1:
    #ifdef LIBXM_DEBUG
      LOG_ERROR("\n\tXM module is not sane!");
    #endif
      break;
    case -2:
    #ifdef LIBXM_DEBUG
      LOG_ERROR("\n\tXM context memory allocation error!");
    #endif
      break;
    default:
      {
        xm_set_max_loop_count((xm_context_s*)xm_ctx,1);
      #ifdef LIBXM_DEBUG
        LOG_INFO("\tTrack loops: %d",xm_get_loop_count((xm_context_s*)xm_ctx));
        LOG_INFO("\tXM context created");
        LOG_INFO("\tXM name: %s",xm_ctx->module.name);
        LOG_INFO("\tXM channels: %d", xm_ctx->module.num_channels);
        LOG_INFO("\tXM patterns: %d", xm_ctx->module.num_patterns);
        LOG_INFO("\tXM instruments: %d", xm_ctx->module.num_instruments);
        LOG_INFO("\tXM restart position: %d", xm_ctx->module.restart_position);
        LOG_INFO("\tXM length: %d",xm_ctx->module.length);
        LOG_INFO("\tXM order table:");
        printf("\t\t");
        for(int i=0;i<xm_ctx->module.length;i++){
          printf("%d ", xm_ctx->module.pattern_table[i]);
        }
        printf("\n");
      #endif
      }
      break;
  }
  return rc;
}

void XMPlaybackControl(u8 control){
  PLAYER_TASK t;
  t.task=control;
  t.ctx=xm_ctx;
  xQueueSend(player_queue,&t,portMAX_DELAY);
#ifdef LIBXM_DEBUG
  LOG_INFO("\tXM playback control called");
#endif
}

bool xm_player_loop(){
  if(player_task_handle==NULL){
    static PLAYER_TASK t;
    static int xm_buf_idx=0;
    // Check for new commands (non-blocking)
    xQueueReceive(player_queue,&t,0);
    // Execute current state continuously
    switch(t.task){
      case PLAYER_PLAY:
        {
          // Check if i2s queue has space before generating
          if(uxQueueSpacesAvailable(i2s_queue)>0){
            auto t1=esp_timer_get_time();
            // Clear buffer first
            memset(xm_buf[xm_buf_idx],0,XM_BUF_SIZE);
            for(int i=0;i<XM_SAMPLES_PER_BUFFER;i++){
              float left=0.0f,right=0.0f; 
              // Additional safety check before calling xm_sample
              if(t.ctx!=nullptr&&xm_ctx!=nullptr){
                if(!xm_sample(t.ctx,&left,&right)) return false;
                // Update statistics
                _st.xm_samp_min=min(_st.xm_samp_min,left);
                _st.xm_samp_min=min(_st.xm_samp_min,right);
                _st.xm_samp_max=max(_st.xm_samp_max,left);
                _st.xm_samp_max=max(_st.xm_samp_max,right);
                // Clamp values
                left=max(left,-1.0f);
                left=min(left,1.0f);
                right=max(right,-1.0f);
                right=min(right,1.0f);
              }
              int mvol=32767;
              xm_buf[xm_buf_idx][2*i]=(i16)(left*mvol);
              xm_buf[xm_buf_idx][2*i+1]=(i16)(right*mvol);
            }
            auto t2=esp_timer_get_time();
            if(xQueueSend(i2s_queue,&xm_buf_idx,0)==pdTRUE){
              xm_buf_idx++;
              xm_buf_idx%=XM_BUF_NUM; 
              // Complete statistics code
              int t=(t2-t1);
              _st.xm_render_last_us=t;
              _st.xm_render_min_us=min(_st.xm_render_min_us,t);
              _st.xm_render_max_us=max(_st.xm_render_max_us,t);
              int cpu=t*XM_SAMPLE_RATE/XM_SAMPLES_PER_BUFFER/10000;
              _st.xm_render_last_cpu=cpu;
              _st.xm_render_min_cpu=min(_st.xm_render_min_cpu,cpu);
              _st.xm_render_max_cpu=max(_st.xm_render_max_cpu,cpu);
            }
          }
        }
        break;
      case PLAYER_STOP:
        // Do nothing when stopped
        break;
    }
  }
  return true;
}

void IRAM_ATTR player_task(void *arg){
  PLAYER_TASK t;
  t.task=PLAYER_STOP;
  int xm_buf_idx=0;
#ifdef LIBXM_DEBUG
  u8 oldTask=t.task;
#endif
  while(1){
    xQueueReceive(player_queue,&t,0);
  #ifdef LIBXM_DEBUG
    if(oldTask!=t.task){
      oldTask=t.task;
      printf("\n\033[32m[PLAYER]\033[0m\tPlayer task: %s\n", t.task==1?"\033[33mPlaying\033[0m":"\033[31mStopped\033[0m");
    }
  #endif
    switch(t.task){
      case PLAYER_PLAY:
        {
          auto t1=esp_timer_get_time();
          for(int i=0;i<XM_SAMPLES_PER_BUFFER;i++){
            float left,right;
            xm_sample(t.ctx,&left,&right);
            _st.xm_samp_min=min(_st.xm_samp_min,left);
            _st.xm_samp_min=min(_st.xm_samp_min,right);
            _st.xm_samp_max=max(_st.xm_samp_max,left);
            _st.xm_samp_max=max(_st.xm_samp_max,right);
            left=max(left,-1);
            left=min(left,1);
            right=max(right,-1);
            right=min(right,1);
            int mvol=32767;
            xm_buf[xm_buf_idx][2*i]=(i16)(left*mvol);
            xm_buf[xm_buf_idx][2*i+1]=(i16)(right*mvol);
          }
          auto t2 =esp_timer_get_time();
          xQueueSend(i2s_queue,&xm_buf_idx,portMAX_DELAY);
          xm_buf_idx++;
          xm_buf_idx%=XM_BUF_NUM;
          int t=(t2-t1);

          _st.xm_render_last_us=t;
          _st.xm_render_min_us=min(_st.xm_render_min_us,t);
          _st.xm_render_max_us=max(_st.xm_render_max_us,t);

          int cpu=t*XM_SAMPLE_RATE/XM_SAMPLES_PER_BUFFER/10000;
          _st.xm_render_last_cpu=cpu;
          _st.xm_render_min_cpu=min(_st.xm_render_min_cpu, cpu);
          _st.xm_render_max_cpu=max(_st.xm_render_max_cpu, cpu);
        }
        break;
      case PLAYER_STOP:
        {
          memset(xm_buf[xm_buf_idx],0,XM_BUF_SIZE);
          // xQueueSend(i2s_queue,&xm_buf_idx,portMAX_DELAY);
          xm_buf_idx++;
          xm_buf_idx%=XM_BUF_NUM;
        }
        break;
    }
    vTaskDelay(1);
  }
}

void IRAM_ATTR i2s_task(void *arg){
  int idx;
  while(1){
    xQueueReceive(i2s_queue,&idx,portMAX_DELAY);
    // Update EQ buffers here - much better timing than in xm_sample
    extern void xm_update_eq_buffers(void);
    xm_update_eq_buffers();
    // Send samples one by one to ESP8266Audio
    for(int i=0;i<XM_SAMPLES_PER_BUFFER;i++){
      int16_t sample[2];
      sample[0]=xm_buf[idx][2*i];   // Left
      sample[1]=xm_buf[idx][2*i+1]; // Right
      while (!output->ConsumeSample(sample)){
        vTaskDelay(1); // Wait if output buffer is full
      }
    }
    vTaskDelay(1);
  }
}

bool xm_send_to_dac(){
  static int current_buf=-1;
  static int sample_pos=0;
  int idx;
  // Get new buffer if needed
  if(current_buf==-1||sample_pos>=XM_SAMPLES_PER_BUFFER){
    if(xQueueReceive(i2s_queue,&idx,0)==pdTRUE){
      current_buf=idx;
      sample_pos=0;
    }else{
      return false; // No buffer ready
    }
  }
  // Send sample to ESP8266Audio output
  if(current_buf>=0){
    int16_t sample[2];
    sample[0]=xm_buf[current_buf][2*sample_pos];
    sample[1]=xm_buf[current_buf][2*sample_pos+1];
    if(output->ConsumeSample(sample)){
      sample_pos++;
      if(sample_pos>=XM_SAMPLES_PER_BUFFER){
        current_buf=-1; // Buffer finished
      }
      return true;
    }
  }
  return false;
}

void stop_xm(){
  // Stop playback first
  XMPlaybackControl(PLAYER_STOP);
  vTaskDelay(pdMS_TO_TICKS(100)); // Wait for task to process stop command
  // Delete tsks
  if(player_task_handle!=NULL) {
    vTaskDelete(player_task_handle);
    player_task_handle=NULL;
  }
  if(i2s_task_handle!=NULL) {
    vTaskDelete(i2s_task_handle);
    i2s_task_handle=NULL;
  }
  // Delete queues
  if(i2s_queue){
    vQueueDelete(i2s_queue);
    i2s_queue=NULL;
  }
  if(player_queue){
    vQueueDelete(player_queue);
    player_queue=NULL;
  }
  // Free XM context - this should free the large allocation
  if(xm_ctx){
    xm_free_context(xm_ctx);
    xm_ctx=NULL;
  }
  // Free sample buffers
  for(int i=0;i<XM_BUF_NUM;i++){
    if(xm_buf[i]){
      heap_caps_free(xm_buf[i]);
      xm_buf[i]=NULL;
    }
  }
  // Reset file pointers
  xm_file=nullptr;
  xm_file_size=0;
  current_file_pos=0;
  
  // Force garbage collection
  // heap_caps_malloc_extmem_enable(1);
  
#ifdef LIBXM_DEBUG
  LOG_INFO("\tXM stopped and memory freed");
#endif
}

const char* xm_get_title(){
  if(!xm_ctx){
    return nullptr;
  }
  return xm_get_module_name(xm_ctx);
}

uint8_t xm_get_channels(){
  if(!xm_ctx){
    return 0;
  }
  return xm_get_number_of_channels(xm_ctx);
}

void xm_get_description(char* description,size_t maxLen){
  if(!xm_ctx){
    description[0]='\0';
    return;
  }
  description[0]='\0';
  uint16_t numInstruments=xm_get_number_of_instruments(xm_ctx);
  uint16_t maxInstr=(numInstruments<MAX_INSTRUMENTS_DESC)?numInstruments:MAX_INSTRUMENTS_DESC;
  for(uint16_t i=1;i<=maxInstr;i++){
    const char* instrName=xm_get_instrument_name(xm_ctx,i);
    if(instrName&&strlen(instrName)>0){
      strncat(description,instrName,maxLen-strlen(description)-1);
      if(i<maxInstr){
        strncat(description," ",maxLen-strlen(description)-1);
      }
    }
  }
}

void xm_set_speed(uint8_t speed){
  if(!xm_ctx) return;
  int newRate;
  switch(speed){
    case 0:
      newRate=originalSampleRate*2;  // Half speed (slower)
      break;
    case 1:
      newRate=originalSampleRate;      // Normal speed
      break;
    case 2:
      newRate=originalSampleRate/2;  // Double speed (faster)
      break;
    default:
      newRate=originalSampleRate;
      break;
  }
  // Update the context rate
  xm_ctx->rate=newRate;
}

void xm_set_separation(int separation){
  if(separation>=0&&separation<=64){
    xm_stereo_separation=separation;
  }
}

void xm_init_track_frame(unsigned long* trackFrame){
  if(trackFrame) *trackFrame=0;
  xm_track_frame=trackFrame;
}

// Main calculate playing time function like player plays!
signed long xm_get_playback_time(bool oneFiftieth){
  if(!xm_ctx) return -1;
  // Save original state
  uint8_t orig_ord=xm_ctx->current_table_index;
  uint8_t orig_row=xm_ctx->current_row;
  uint8_t orig_tick=xm_ctx->current_tick;
  uint16_t orig_tempo=xm_ctx->tempo;
  uint16_t orig_bpm=xm_ctx->bpm;
  uint8_t orig_loop_count=xm_ctx->loop_count;
  uint16_t orig_extra_ticks=xm_ctx->extra_ticks;
  bool orig_position_jump=xm_ctx->position_jump;
  bool orig_pattern_break=xm_ctx->pattern_break;
  uint8_t orig_jump_dest=xm_ctx->jump_dest;
  uint8_t orig_jump_row=xm_ctx->jump_row;
  // Initialize simulation state
  xm_ctx->current_table_index=0;
  xm_ctx->current_row=0;
  xm_ctx->current_tick=0;
  xm_ctx->position_jump=false;
  xm_ctx->pattern_break=false;
  xm_ctx->jump_dest=0;
  xm_ctx->jump_row=0;
  xm_ctx->extra_ticks=0;
  uint64_t total_ticks=0;
  uint8_t max_loop_count=1;
  // Clear loop detection array
  memset(xm_ctx->row_loop_count,0,MAX_NUM_ROWS*xm_ctx->module.length*sizeof(uint8_t));
  while(true){
    esp_task_wdt_reset();
    // Simulate xm_tick() - only process row when current_tick==0
    if(xm_ctx->current_tick==0){
      // Handle position jumps and pattern breaks first
      if(xm_ctx->position_jump){
        xm_ctx->current_table_index=xm_ctx->jump_dest;
        xm_ctx->current_row=xm_ctx->jump_row;
        xm_ctx->position_jump=false;
        xm_ctx->pattern_break=false;
        xm_ctx->jump_row=0;
        if(xm_ctx->current_table_index>=xm_ctx->module.length){
          xm_ctx->current_table_index=xm_ctx->module.restart_position;
        }
      }else if(xm_ctx->pattern_break){
        xm_ctx->current_table_index++;
        xm_ctx->current_row=xm_ctx->jump_row;
        xm_ctx->pattern_break=false;
        xm_ctx->jump_row=0;
        if(xm_ctx->current_table_index>=xm_ctx->module.length){
          xm_ctx->current_table_index=xm_ctx->module.restart_position;
        }
      }
      xm_pattern_t* cur=xm_ctx->module.patterns+xm_ctx->module.pattern_table[xm_ctx->current_table_index];
      // Process effects for this row
      for(uint8_t i=0;i<xm_ctx->module.num_channels;++i){
        xm_pattern_slot_t* s=cur->slots+xm_ctx->current_row*xm_ctx->module.num_channels+i;
        switch(s->effect_type){
          case 0xB: // Position jump
            if(s->effect_param<xm_ctx->module.length){
              xm_ctx->position_jump=true;
              xm_ctx->jump_dest=s->effect_param;
            }
            break;
          case 0xD: // Pattern break
            xm_ctx->pattern_break=true;
            xm_ctx->jump_row=(s->effect_param>>4)*10+(s->effect_param&0x0F);
            break;
          case 0xE: // Extended command
            if((s->effect_param>>4)==0xE){ // Pattern delay
              xm_ctx->extra_ticks=(s->effect_param&0x0F)*xm_ctx->tempo;
            }
            break;
          case 0xF: // Set tempo/BPM
            if(s->effect_param>0){
              if(s->effect_param<=0x1F){
                xm_ctx->tempo=s->effect_param;
              }else{
                xm_ctx->bpm=s->effect_param;
              }
            }
            break;
        }
      }
      // Loop detection - exact replication from xm_play.c
      xm_ctx->loop_count=(xm_ctx->row_loop_count[MAX_NUM_ROWS*xm_ctx->current_table_index+xm_ctx->current_row]++);
      if(xm_ctx->loop_count>=max_loop_count){
        break;
      }
      xm_ctx->current_row++;
      // Handle pattern end
      if(!xm_ctx->position_jump&&!xm_ctx->pattern_break&&
         (xm_ctx->current_row>=cur->num_rows||xm_ctx->current_row==0)){
        xm_ctx->current_table_index++;
        xm_ctx->current_row=xm_ctx->jump_row;
        xm_ctx->jump_row=0;
        if(xm_ctx->current_table_index>=xm_ctx->module.length){
          xm_ctx->current_table_index=xm_ctx->module.restart_position;
        }
      }
    }
    // Advance tick
    xm_ctx->current_tick++;
    if(xm_ctx->current_tick>=xm_ctx->tempo+xm_ctx->extra_ticks){
      xm_ctx->current_tick=0;
      xm_ctx->extra_ticks=0;
    }
    total_ticks++;
  }
  // Restore original state
  xm_ctx->current_table_index=orig_ord;
  xm_ctx->current_row=orig_row;
  xm_ctx->current_tick=orig_tick;
  xm_ctx->tempo=orig_tempo;
  xm_ctx->bpm=orig_bpm;
  xm_ctx->loop_count=orig_loop_count;
  xm_ctx->extra_ticks=orig_extra_ticks;
  xm_ctx->position_jump=orig_position_jump;
  xm_ctx->pattern_break=orig_pattern_break;
  xm_ctx->jump_dest=orig_jump_dest;
  xm_ctx->jump_row=orig_jump_row;
  memset(xm_ctx->row_loop_count,0,MAX_NUM_ROWS*xm_ctx->module.length*sizeof(uint8_t));
  // Returns result
  float seconds=(float)total_ticks/(xm_ctx->bpm*0.4f);
  return (signed long)(seconds*(oneFiftieth?50:1000));
}

void xm_set_loop(bool loop){
  if(!xm_ctx) return;
  if(loop){
    xm_set_max_loop_count((xm_context_s*)xm_ctx,0);  // infinite loops track
  }else{
    xm_set_max_loop_count((xm_context_s*)xm_ctx,1);  // once play track
    // CRITICAL: Reset loop_count to 0 when disabling loop to prevent immediate stop
    xm_ctx->loop_count=0;
    // Also clear row_loop_count array to reset loop detection
    memset(xm_ctx->row_loop_count,0,MAX_NUM_ROWS*xm_ctx->module.length*sizeof(uint8_t));
  }
}
