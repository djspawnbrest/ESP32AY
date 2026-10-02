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
// under EB_STEP2: the ZxPod wants EB_STEP4_LOW (the trace below).  A state
// counts once two samples 1 ms apart agree: a real phase lasts several ms
// even spun fast, contact bounce and noise less.  EncButton queues up to 5
// steps; tick() in loop() takes one a call.
static portMUX_TYPE encMux=portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t encTimer=NULL;

// The trace (build with -D ENC_TRACE): every state the sampler takes and
// the step it makes, as they happen, and every step loop() takes.  It showed
// on 28 Sep that the ZxPod's encoder makes a full cycle a click (A1B1 ->
// A1B0 -> A0B0 -> A0B1 -> A1B1): EB_STEP2 made two steps of each.
#ifdef ENC_TRACE
struct EncTrace{ uint32_t ms; uint8_t state; int8_t step; };
static EncTrace encTrace[64];
static volatile uint8_t encTraceW=0;
static uint8_t encTraceR=0;
#endif

static void encSample(void *){
  static int8_t last=-1,stable=-1;
  int8_t s=enc.readEnc();
  if(s!=last){ last=s; return; }        // changed since the last sample: not yet
  if(s==stable) return;
  stable=s;
  portENTER_CRITICAL(&encMux);
  int8_t step=enc.VirtEncButton::tickISR(s);
  portEXIT_CRITICAL(&encMux);
#ifdef ENC_TRACE
  EncTrace &t=encTrace[encTraceW&63];
  t.ms=millis(); t.state=s; t.step=step;
  encTraceW++;
#else
  (void)step;
#endif
}

#ifdef ENC_TRACE
static void encTracePrint(){
  while(encTraceR!=encTraceW){
    EncTrace t=encTrace[encTraceR&63];
    encTraceR++;
    Serial.printf("enc: %lu ms  A%uB%u%s\n",(unsigned long)t.ms,t.state&1,(t.state>>1)&1,
      t.step>0?"  step +1":t.step<0?"  step -1":"");
  }
}
#endif

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
#ifdef ENC_TRACE
  encTracePrint();
  if(enc.turn()) Serial.printf("enc: %lu ms  taken %s%s (type %u%s)\n",(unsigned long)millis(),enc.dir()>0?"right":"left",
    enc.fast()?" FAST":"",lfsConfig.encType,lfsConfig.encReverse?", reversed":"");
#endif
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
