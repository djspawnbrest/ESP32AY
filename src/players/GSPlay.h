// GSPlay.h - .mod through General Sound: X-Trade's card, its own Z80 and
// ROM (gs105b.rom, built in), emulated on core 0 (gs/gs.c).
//
// The card is an engine task of its own.  At power-up it runs the ROM's
// start - the RAM test, 10 s of the card's time with 2 MB - at a low
// priority, in whatever core 0 has spare.  A module is then what a
// Spectrum would do with it: #30, every byte through B3h, #D2, #31.  The
// engine does the host's side itself between slices of the card's time,
// so nothing waits across cores.  While it plays, the engine runs at a
// priority above the player task and paces the card to the PCM5102A at
// the card's own 37.5 kHz.  Paused, the card is simply not run.
//
// Only 4-channel modules go to the card (M.K., M!K!, FLT4, 4CHN, and
// the old 15-sample ones: PLAY.a80's test).  Anything else, or no
// PSRAM, and TYPE_MOD stays with upstream's native player (MODPlay.h).
// The length and title come from the module read into PSRAM (zp/zpmod.h).

extern "C" {
#include "../gs/gs.h"
#include "../gs/gs_proto.h"
#include "../gs/gs105b_sym.h"
}

#include "esp_core_dump.h"

extern const uint8_t gs_rom_bin[] asm("_binary_rom_gs105b_rom_start");

#define GS_CHUNK        375             // 10 ms of the card's time
#define GS_PRIO_IDLE    1               // the ROM's start: core 0's spare time
#define GS_PRIO_PLAY    3               // above the player task (2)
#define GS_POLL_T       32              // the host's poll, in the card's T-states
#define GS_BOOT_T       (GS_CLOCK/10*104)  // the ROM's start with 2 MB: 10.4 s of the card's time

enum{
  GS_OFF=0,       // no PSRAM, or the ROM did not start
  GS_BOOTING,     // the ROM's RAM test
  GS_READY,       // the command loop, waiting; the card stands still
  GS_LOADING,
  GS_PLAYING,
};

// The player's side (core 1: music_init, music_stop) owns `want`; the
// engine is woken by a notification, raises `busy`, takes `want` as the
// truth and sets `state`.  The player waits for `busy` to fall before it
// touches the I2S again: while it is up, the engine may be in i2s_write.
struct{
  volatile uint8_t state=GS_OFF;
  volatile bool    active=false;        // this track is the card's
  volatile bool    want=false;          // the player wants the card playing GS.path
  volatile bool    busy=false;          // the engine is on a request of the player's
  volatile uint8_t cancel=0;            // the player skipped: every wait of the engine's gives up now
  volatile bool    failed=false;        // this track was refused or stalled
  volatile uint32_t loadDone,loadSize;
  volatile float   cpu;                 // % of real time the card took, last second
  volatile float   loadKBs;             // the last load's rate
  volatile uint32_t free;               // #21 after the last load
  uint32_t ram;                         // what the ROM's #20 found
  uint32_t bootT;                       // the T-states the last start took
  TaskHandle_t task=NULL;
  char path[MAX_PATH];
  uint8_t *data=NULL;                   // the whole module, PSRAM: read once, on core 1
  uint32_t size=0;
  bool    ampOn;                        // the amp let open for this track (GS_Tick)
}GS;

static uint8_t gsRom[GS_ROM_SIZE];      // internal SRAM: the card runs from it
static uint8_t gsFixed[GS_FIXED_SIZE];
static int16_t gsBuf[GS_CHUNK*2];
static int     gsFill;                  // frames in gsBuf not yet written
static bool    gsSound;                 // frames go to the I2S (else dropped)
static uint64_t gsBusyUs;
static uint32_t gsBusyFrames;
static int64_t gsLastYield;            // the engine last gave core 0 away (a delay, or a write that waited)
static int64_t gsLastWait;             // a write last waited for the DMA
static bool    gsStarved;               // said once: the I2S stopped pacing the card
static void   *gsSnap;                  // the card just started (gs_snapshot_save), PSRAM

//------------------------------------------------------------------------
// The engine's output: gain and stereo as upstream's MOD path has them
//------------------------------------------------------------------------
static void gsFlush(){
  if(gsFill==0) return;
  if(gsSound&&out){
    int gain=out->GetGain();            // F2P6, lfsConfig.dacGain
    for(int i=0;i<gsFill;i++){
      int l=gsBuf[2*i],r=gsBuf[2*i+1];
      switch(lfsConfig.modStereoSeparation){
        case MOD_HALFSTEREO:{ int a=(3*l+r)>>2,b=(l+3*r)>>2; l=a; r=b; } break;
        case MOD_MONO: l=r=(l+r)>>1; break;
        default: break;
      }
      l=(l*gain)>>6; r=(r*gain)>>6;
      gsBuf[2*i]=l>32767?32767:l<-32767?-32767:l;
      gsBuf[2*i+1]=r>32767?32767:r<-32767?-32767:r;
    }
    size_t w;
    int64_t t0=esp_timer_get_time();
    // paces the card; never for ever: an I2S that stopped must not hold the
    // engine (and the player's cleanup waiting on it)
    if(i2s_write(I2S_NUM_0,gsBuf,gsFill*4,&w,pdMS_TO_TICKS(100))!=ESP_OK)
      vTaskDelay(pdMS_TO_TICKS(10));    // no driver: never spin at this priority
    int64_t t1=esp_timer_get_time();
    if(t1-t0>=500) gsLastYield=gsLastWait=t1;   // it waited for the DMA: core 0 had the time
  }
  gsFill=0;
}

// Run the card `t` T-states, keeping what it outputs.  Unpaced (no sound)
// it would never block: core 0's idle task still has to feed the watchdog.
static void gsRunT(int t){
  int64_t t0=esp_timer_get_time();
  gsFill+=gs_step(gsBuf+2*gsFill,GS_CHUNK-gsFill,t);
  int64_t t1=esp_timer_get_time();
  gsBusyUs+=t1-t0;
  if(gsFill==GS_CHUNK){ gsBusyFrames+=GS_CHUNK; gsFlush(); }
  if(!gsSound&&t1-gsLastYield>20000){ vTaskDelay(1); gsLastYield=esp_timer_get_time(); }
  // Paced, the writes wait for the DMA and core 0's idle task runs meanwhile.
  // If they stop waiting (an I2S not running as it should), a task at this
  // priority would starve the idle task until the task watchdog reboots the
  // board - the reset of 28 Sep, noise and all.  Give the core away instead,
  // and say so.
  if(gsSound&&t1-gsLastYield>200000){  // (the DMA's buffers take a second to fill at a start)
    if(!gsStarved&&t1-gsLastWait>3000000){ gsStarved=true; Serial.printf("GS: the I2S is not pacing the card; yielding\n"); }
    vTaskDelay(1);
    gsLastYield=esp_timer_get_time();
  }
}

static void gsIdle(void *){ gsRunT(GS_POLL_T); }

// A wait gave up: the card's state, for the console.
static void gsWhy(const char *what){
  gs_info_t i; gs_info(&i);
  Serial.printf("GS: %s timed out: pc=%04X sp=%04X page=%u iff1=%u in_halt=%u cmd=%u data=%u t=%.1fs\n",
    what,i.pc,i.sp,i.page,i.iff1,i.in_halt,i.f_cmd,i.f_data,i.tstates/(double)GS_CLOCK);
  Serial.printf("GS:   stack:");                // return addresses: where it came from
  for(int k=0;k<12;k++) Serial.printf(" %04X",gs_peek(i.sp+2*k)|gs_peek(i.sp+2*k+1)<<8);
  Serial.printf("\n");
}

// The card stopped answering and there is no snapshot to go back to: its
// ROM's start again (about 11 s of the card's time, unpaced).
// why the board last started: a crash and a power switch look the same otherwise
static const char *gsResetReason(){
  switch(esp_reset_reason()){
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INTERRUPT WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWN-OUT";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_EXT: return "external";
    default: return "unknown";
  }
}

// The last crash, from the core dump in flash: its panic output goes to
// UART0, which is not on the USB console.  Said when the card is up (the
// console is open by then); the dump itself stays for esptool to read.
static void gsCrashReport(){
  static bool said;
  if(said) return;
  said=true;
  esp_reset_reason_t r=esp_reset_reason();
  if(r!=ESP_RST_PANIC&&r!=ESP_RST_INT_WDT&&r!=ESP_RST_TASK_WDT&&r!=ESP_RST_WDT) return;
  esp_core_dump_summary_t *c=(esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
  if(!c) return;
  if(esp_core_dump_get_summary(c)==ESP_OK){
    Serial.printf("crash: task '%s' pc %08X cause %u vaddr %08X\ncrash: backtrace",c->exc_task,
      (unsigned)c->exc_pc,(unsigned)c->ex_info.exc_cause,(unsigned)c->ex_info.exc_vaddr);
    for(uint32_t i=0;i<c->exc_bt_info.depth&&i<16;i++) Serial.printf(" %08X",(unsigned)c->exc_bt_info.bt[i]);
    Serial.printf("%s\n",c->exc_bt_info.corrupted?" (corrupted)":"");
  }else Serial.printf("crash: no core dump in flash\n");
  free(c);
}

static bool gsBoot();
static bool gsRevive(){
  bool snd=gsSound;
  Serial.printf("GS: restarting the card\n");
  GS.state=GS_BOOTING;
  bool ok=gsBoot();
  gsSound=snd;
  return ok;
}

//------------------------------------------------------------------------
// The engine
//------------------------------------------------------------------------
static gsp_t gsHost={gsIdle,NULL,0,&GS.cancel};

static bool gsBoot(){
  gsSound=false;
  gs_reset();
  gs_run(gsBuf,1);                      // INIT's OUT (5) would drop an earlier command
  // The ROM's start is not the player's to cancel: a track skipped during
  // its RAM test once left the card dead until the next power-on (0134).
  gsp_t boot={gsIdle,NULL,30u*(GS_CLOCK/GS_POLL_T),NULL};
  uint32_t ram;
  if(gsp_mem_size(&boot,&ram)){ gsWhy("#20"); return false; }
  GS.ram=ram;
  gs_info_t i; gs_info(&i);
  GS.bootT=i.tstates;
  Serial.printf("GS: ROM up, RAM %u bytes (the board's last reset: %s)\n",(unsigned)ram,gsResetReason());
  gsCrashReport();
  // The card as it is now - its ROM's start done, waiting in its command
  // loop - is where every module starts from (gsLoad).  As retro-esp32ay-
  // zplayer's host/gs_snaptest.c shows, a module after a restore plays
  // sample for sample as on a card just started, even after a load
  // abandoned half way.
  if(!gsSnap) gsSnap=heap_caps_malloc(gs_snapshot_size(),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(gsSnap) gs_snapshot_save(gsSnap);
  return true;
}

// The module from memory (GS_GetInfo read it): the engine never touches the
// SD card, so nothing it does can hold the card's lock or keep a file open
// across a remount by the other core.  It starts from the card just
// started (the snapshot), so nothing a module left behind - a load given up
// half way, a card that stopped answering - reaches the next one, and
// giving up (GS.cancel, any wait) needs no word to the card at all.
static bool gsLoad(){
  gsHost.timeout=5u*(GS_CLOCK/GS_POLL_T); // a byte the card has not taken in 5 s: stalled
  if(!GS.data||!GS.size) return false;
  GS.loadSize=GS.size;
  GS.loadDone=0;
  uint8_t h;
  if(gsSnap) gs_snapshot_restore(gsSnap);
  else gsp_stop(&gsHost);
  if(gsp_load_begin(&gsHost,&h)) return false;
  while(GS.loadDone<GS.loadSize){
    if(GS.cancel) return false;
    uint32_t n=GS.loadSize-GS.loadDone;
    if(n>4096) n=4096;
    const uint8_t *p=GS.data+GS.loadDone;
    for(uint32_t i=0;i<n;i++)
      if(gsp_load_byte(&gsHost,p[i])) return false;
    GS.loadDone+=n;
  }
  if(gsp_load_end(&gsHost)) return false;
  gsFlush();
  uint8_t b;                            // #21, three bytes: the card's RAM left
  uint32_t fr=0;
  if(!gsp_cmd(&gsHost,0x21)){
    for(int k=0;k<3;k++){
      for(uint32_t n=0;n<gsHost.timeout&&!GS.cancel&&!(gs_host_status()&0x80);n++) gsIdle(NULL);
      b=gs_host_read(); fr|=(uint32_t)b<<(8*k);
    }
    GS.free=fr;
  }
  gsHost.timeout=30u*(GS_CLOCK/GS_POLL_T); // #31 waits while the ROM prepares the module
  return gsp_play(&gsHost,h)==GSP_OK;
}

static void gsPlaying(){
  uint32_t frames=0;
  int64_t tick=esp_timer_get_time();
  gsBusyUs=0; gsBusyFrames=0;
  PlayerCTRL.trackFrame=0;
  while(GS.want){
    if(!PlayerCTRL.isPlay){             // paused: the card stands still
      gsFlush();
      i2s_zero_dma_buffer(I2S_NUM_0);
      while(!PlayerCTRL.isPlay&&GS.want) vTaskDelay(pdMS_TO_TICKS(20));
      tick=esp_timer_get_time();
      continue;
    }
    gsRunT(GS_CHUNK*GS_TPERIOD);        // 10 ms of the card, out to the DAC
    frames+=GS_CHUNK;
    if(frames==5*GS_RATE)               // once a module: how hard the card works here
      Serial.printf("GS: card %.1f%% of real time\n",GS.cpu);
    PlayerCTRL.trackFrame=frames/(GS_RATE/50);   // upstream counts 50 Hz frames
    uint8_t p[4]; gs_peaks(p);
    for(int i=0;i<4;i++) modEQchn[i]=p[i];
    int64_t t=esp_timer_get_time();
    if(t-tick>=250000){
      if(gsBusyFrames>=GS_RATE){
        GS.cpu=100.0f*gsBusyUs/1e6f/(gsBusyFrames/(float)GS_RATE);
        gsBusyUs=0; gsBusyFrames=0;
      }
      tick=t;
    }
  }
  gsFlush();
  gsSound=false;                        // the card is left as it is: the next load restores it
}

static void GSEngineTask(void *){
  GS.state=GS_BOOTING;
  // A card that would not start stays off, and the task stays to say so:
  // a module given to it while it was starting is refused (GS.failed), so
  // the player moves on, and the next ones go to upstream's MOD player.
  if(!gsBoot()&&!gsBoot()){ GS.state=GS_OFF; Serial.printf("GS: the ROM did not start\n"); }
  else GS.state=GS_READY;
  for(;;){
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);   // the card stands still while nothing plays
    GS.busy=true;                       // before `want` is read: see the struct
    __sync_synchronize();
    if(!GS.want){ GS.busy=false; continue; }
    if(GS.state==GS_OFF){ GS.failed=true; GS.busy=false; continue; }
    GS.state=GS_LOADING;
    vTaskPrioritySet(NULL,GS_PRIO_PLAY);
    gsSound=true;
    gsLastYield=gsLastWait=esp_timer_get_time();
    int64_t t0=esp_timer_get_time();
    Serial.printf("GS: loading %s, %u bytes\n",GS.path,(unsigned)GS.size);
    bool loaded=gsLoad();
    if(!loaded&&!GS.cancel){            // once more, from a fresh card: the snapshot, or the ROM's start
      gsWhy("the load");
      if(gsSnap||gsRevive()){
        GS.state=GS_LOADING;
        t0=esp_timer_get_time();
        loaded=!GS.cancel&&gsLoad();
      }
    }
    if(loaded){
      double ls=(esp_timer_get_time()-t0)/1e6;
      GS.loadKBs=GS.loadSize/1024.0/ls;
      Serial.printf("GS: %s, %u bytes in %.2f s (engine stack free %u)\n",GS.path,(unsigned)GS.loadSize,ls,
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
      GS.state=GS_PLAYING;
      gsPlaying();
      GS.state=GS_READY;
    }else{
      gsFlush();
      gsSound=false;
      if(!GS.cancel){                   // not a skip: the card would not have it
        gsWhy("the load");
        Serial.printf("GS: %s refused or stalled at %u of %u\n",GS.path,(unsigned)GS.loadDone,(unsigned)GS.loadSize);
        GS.failed=true;
      }
      GS.state=GS_READY;
    }
    vTaskPrioritySet(NULL,GS_PRIO_IDLE);
    GS.busy=false;
  }
}

// setup(): the card's memory and its start.  No PSRAM, no card.
void GS_Init(){
  Serial.printf("boot: last reset %s\n",gsResetReason());
  uint8_t *ram=(uint8_t *)heap_caps_malloc((size_t)GS_PAGES_2MB*GS_PAGE_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!ram){ Serial.printf("GS: no PSRAM for the 2 MB, .mod plays natively\n"); return; }
  memcpy(gsRom,gs_rom_bin,GS_ROM_SIZE);
  gs_init(gsRom,gsFixed,ram,GS_PAGES_2MB);
  xTaskCreatePinnedToCore(GSEngineTask,"GS card",6144,NULL,GS_PRIO_IDLE,&GS.task,0);
  Serial.printf("GS: 2 MB at %p; internal heap free %u, largest block %u\n",ram,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
}

//------------------------------------------------------------------------
// The player's side (player.h): open, play, cleanup
//------------------------------------------------------------------------

// Is this module one for the card?  4 channels, or 15 samples.
static bool gsTakes(const uint8_t *m,uint32_t size){
  if(size<=1084) return false;
  if(size+0x10000>GS_PAGES_2MB*(uint32_t)GS_PAGE_SIZE) return false;   // what the card can hold, roughly
  const uint8_t *sig=m+1080;
  if(!memcmp(sig,"M.K.",4)||!memcmp(sig,"M!K!",4)||!memcmp(sig,"FLT4",4)||!memcmp(sig,"4CHN",4)) return true;
  for(int i=0;i<4;i++) if(sig[i]<0x20||sig[i]>0x7e) return true;          // no signature: 15 samples
  return false;
}

static void gsFreeData(){
  if(GS.data) heap_caps_free(GS.data);
  GS.data=NULL; GS.size=0;
}

// The whole file into PSRAM, in one go under the SD card's lock.
static bool gsReadAll(const char *path){
  gsFreeData();
  FsFile f;
  xSemaphoreTake(sdCardSemaphore,portMAX_DELAY);
  bool ok=f.open(path,O_RDONLY);
  uint32_t size=ok?f.fileSize():0;
  if(ok&&size>1084&&size+0x10000<=GS_PAGES_2MB*(uint32_t)GS_PAGE_SIZE){
    GS.data=(uint8_t*)heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    ok=GS.data&&f.read(GS.data,size)==(int)size;
  }else ok=false;
  f.close();
  xSemaphoreGive(sdCardSemaphore);
  if(!ok){ gsFreeData(); return false; }
  GS.size=size;
  return true;
}

// TYPE_MOD's open.  A module for the card is read once, whole, and its
// header, patterns and length come from that copy - upstream's MOD_GetInfo
// would seek and read the SD card for every row of the song to time it.
// Anything else - or "MOD player: Built-in" in Set-Up - goes to upstream's
// native player.
void GS_GetInfo(const char *filename){
  GS.active=false;
  if(lfsConfig.modEngine!=MOD_ENGINE_NATIVE&&GS.state!=GS_OFF&&gsReadAll(filename)&&gsTakes(GS.data,GS.size)&&zpModParse(GS.data,GS.size)){
    AYInfo.Length=zpModLength();
    if(!AYInfo.Length) AYInfo.Length=1;
    strncpy(AYInfo.Name,zpMod.title,sizeof(AYInfo.Name)-1);
    modChannels=modChannelsEQ=4;         // upstream's screen (WildPlayer skin): its four channel meters
    strncpy(GS.path,filename,sizeof(GS.path)-1);
    GS.path[sizeof(GS.path)-1]=0;
    GS.active=true;
    Serial.printf("GS: %s: %u bytes, %s, %u:%02u\n",filename,(unsigned)GS.size,zpMod.label,
      (unsigned)(AYInfo.Length/50/60),(unsigned)(AYInfo.Length/50%60));
    return;
  }
  gsFreeData();
  zpModFree();
  MOD_GetInfo(filename);
}

// TYPE_MOD's init (music_init, core 1): the I2S at the card's rate, then
// the card.  Here and not in GS_Play, which the player task calls on core
// 0: started there, a start could land after the cleanup of a track
// skipped at once, and leave the engine writing to an I2S the next track
// takes down (noise, then a crash in the driver).
void GS_Start(){
  if(!GS.active) return;                // upstream's player starts itself in MOD_Play
  initOut();
  out->begin();
  out->SetRate(GS_RATE);
  muteAmp();                            // silent until the card plays (GS_Tick)
  GS.ampOn=false;
  modOutInitialized=true;
  GS.failed=false;
  GS.loadDone=GS.loadSize=0;            // not the last track's, on the screen
  GS.cancel=0;
  GS.want=true;
  xTaskNotifyGive(GS.task);
}

// player() on core 1, every loop: the amp opens once the card plays - the
// amp is on I2C, which core 1 drives, so not from the engine.
void GS_Tick(){
  if(!GS.active||GS.ampOn) return;
  if(GS.state==GS_PLAYING&&PlayerCTRL.isPlay){ GS.ampOn=true; unMuteAmp(); }
}

void GS_Play(){
  if(!GS.active){ MOD_Play(); return; }
  if(GS.failed) PlayerCTRL.isFinish=true;
}

// What keeps the module from sounding yet, for the screen, and how far it
// is (0-100); NULL once it plays.  At power-on the card's ROM has its RAM
// test to run first, and a module's bytes go in at the card's pace.
static const char *GS_Wait(int *pct){
  *pct=0;
  if(!GS.active||!GS.want) return NULL;
  if(GS.state==GS_BOOTING){
    gs_info_t i; gs_info(&i);
    uint32_t all=GS.bootT?GS.bootT:GS_BOOT_T;
    uint32_t p=i.tstates/(all/100);
    *pct=p>99?99:p;
    return "Starting";
  }
  if(GS.state==GS_LOADING||GS.state==GS_READY){
    if(GS.loadSize&&GS.loadDone>=GS.loadSize){ *pct=100; return "Preparing"; }
    *pct=GS.loadSize?(int)((uint64_t)GS.loadDone*100/GS.loadSize):0;
    return "Loading";
  }
  return NULL;
}

void GS_Cleanup(){
  if(!GS.active){ MOD_Cleanup(); return; }
  GS.cancel=1;                          // the engine's waits give up at once
  GS.want=false;
  __sync_synchronize();
  uint32_t t0=millis();
  while(GS.busy) vTaskDelay(1);
  if(millis()-t0>200) Serial.printf("GS: the engine took %lu ms to let go\n",(unsigned long)(millis()-t0));
  gsFreeData();
  zpModFree();                          // the patterns: the screen's, of this module only
  memset(modEQchn,0,sizeof(modEQchn));
  memset(&AYInfo,0,sizeof(AYInfo));
  out->stop();
  out->SetRate(44100);
  vTaskDelay(pdMS_TO_TICKS(10));
  skipMod=false;
  modOutInitialized=false;
  GS.active=false;
}
