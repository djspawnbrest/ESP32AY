#include <EncButton.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define DN_BTN 40
#define UP_BTN 41
#define OK_BTN 42
#define RT_ENC 1
#define LT_ENC 2
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define UP_BTN 39
#define DN_BTN 36
#define OK_BTN 33
#define LT_ENC 34
#define RT_ENC 35
#endif

Button up(UP_BTN,INPUT_PULLUP,LOW);
Button dn(DN_BTN,INPUT_PULLUP,LOW);
EncButton enc(LT_ENC,RT_ENC,OK_BTN,INPUT_PULLUP,INPUT_PULLUP);

// The encoder is sampled at 1 kHz by a timer, not polled in loop(): a loop
// with a Z-Player screen takes tens of ms, and a poll that slow missed the
// middle of a click - so with upstream's EB_STEP2 a click made one step,
// two or none ("a lucky moment").  Read in full, each click is two steps
// under EB_STEP2: the ZxPod wants EB_STEP4_LOW (traced 28 Sep).  A state
// counts once two samples 1 ms apart agree: a real phase lasts several ms
// even spun fast, contact bounce and noise less.  EncButton queues up to 5
// steps; tick() in loop() takes one a call.
static portMUX_TYPE encMux=portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t encTimer=NULL;

static void encSample(void *){
  static int8_t last=-1,stable=-1;
  int8_t s=enc.readEnc();
  if(s!=last){ last=s; return; }        // changed since the last sample: not yet
  if(s==stable) return;
  stable=s;
  portENTER_CRITICAL(&encMux);
  enc.VirtEncButton::tickISR(s);
  portEXIT_CRITICAL(&encMux);
}

void buttonsSetup(){
  enc.setEncType(lfsConfig.encType);
  enc.setEncReverse(lfsConfig.encReverse);
  enc.setEncISR(true);
  if(!encTimer){
    esp_timer_create_args_t t={};
    t.callback=encSample;
    t.name="encoder";
    if(esp_timer_create(&t,&encTimer)==ESP_OK) esp_timer_start_periodic(encTimer,1000);
  }
  enc.setClickTimeout(200);
  enc.setHoldTimeout(500);
  enc.setFastTimeout(80);
  dn.setClickTimeout(200);
  dn.setHoldTimeout(500);
  up.setClickTimeout(200);
  up.setHoldTimeout(300);
}

bool isRight=false,isLeft=false;

void generalTick(){
  portENTER_CRITICAL(&encMux);
  enc.tick();
  portEXIT_CRITICAL(&encMux);
  up.tick();
  dn.tick();
  if(enc.hold()){
    if(PlayerCTRL.screen_mode!=SCR_CONFIG&&PlayerCTRL.screen_mode!=SCR_RESET_CONFIG&&PlayerCTRL.screen_mode!=SCR_ABOUT){
      PlayerCTRL.screen_mode++;
      if(PlayerCTRL.screen_mode==SCR_CONFIG) PlayerCTRL.screen_mode=SCR_PLAYER;
      PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
      if(PlayerCTRL.screen_mode==SCR_BROWSER) scrNotPlayer=true;
      else clear_scrollbar();
    }
  }
  if(enc.hasClicks(2)){
    if(PlayerCTRL.screen_mode!=SCR_CONFIG&&PlayerCTRL.screen_mode!=SCR_RESET_CONFIG&&PlayerCTRL.screen_mode!=SCR_ABOUT){
      PlayerCTRL.prev_screen_mode=PlayerCTRL.screen_mode;
      PlayerCTRL.screen_mode=SCR_CONFIG;
      PlayerCTRL.scr_mode_update[PlayerCTRL.screen_mode]=true;
      scrNotPlayer=true;
      if(PlayerCTRL.prev_screen_mode==SCR_BROWSER) clear_scrollbar();
    }
  }
  if(PlayerCTRL.screen_mode==SCR_ALERT){scrNotPlayer=true;}
}
