// zpnone.h - the classic ESP32 (ZxPodIntDAC, ZxPodExtDAC): no PSRAM, so
// no General Sound card (2 MB) and no Z-Player framebuffer.  What
// upstream's files call of them, doing nothing: .mod stays with upstream's
// own MOD player and every screen is upstream's.

struct{
  uint8_t state=0;
  bool    active=false;
}GS;

void GS_Init(){}
void GS_GetInfo(const char *filename){ MOD_GetInfo(filename); }
void GS_Start(){}
void GS_Tick(){}
void GS_Play(){ MOD_Play(); }
void GS_Cleanup(){ MOD_Cleanup(); }

static bool zpOwns=false;
static bool zpMine(){ return false; }
static void zpRelease(){}
static void zpIntro(){ introTFT(); }
static bool zpPlayerDraw(){ return false; }
static bool zpAyDraw(){ return false; }
static bool zpGenDraw(){ return false; }
bool zpBrowserDraw(int mode){ return false; }
bool zpConfigDraw(){ return false; }
bool zpAboutDraw(){ return false; }
