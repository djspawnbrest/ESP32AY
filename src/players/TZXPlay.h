#include <AudioGeneratorTZX.h>

extern int tzx_cur_block;
extern int tzx_total_blocks;
extern char blockTypeLabel[16];
extern AudioGeneratorTZX *tzx;
extern char tzxFullTitle[256];
AudioGeneratorTZX *tzx = nullptr;
char tzxFullTitle[256] = {0};
static unsigned long tzxBaseLength = 0;
static bool tzxInitialized = false;
static int tzxPrevBlock = -1;

void setTzxSpeed(){
  if(tzx){
    tzx->setSpeed(lfsConfig.tapeSpeed);
    uint8_t mult = (lfsConfig.tapeSpeed == TAPE_NORMAL) ? 1 : (lfsConfig.tapeSpeed == TAPE_TURBO1) ? 2 : 4;
    AYInfo.Length = tzxBaseLength / mult;
  }
}

void TZX_Cleanup(){
  if(tzx) tzx->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete tzx;
  delete modFile;
  tzx=nullptr;
  modFile=nullptr;
  out->stop();
  vTaskDelay(pdMS_TO_TICKS(10));
  skipMod=false;
  tzxInitialized=false;
}

void TZX_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  tzx=new AudioGeneratorTZX();
  modFile->open(filename);
  const char* message;
  bool status=tzx->initializeFile(modFile,&message);
  if(!status){
    if(message&&strlen(message)>0) showAlert(message);
    AYInfo.Length=1;
    skipMod=true;
    strcpy(blockTypeLabel, "Author");
    return;
  }
  tzx->initEQBuffers(bufEQ,modEQchn);
  tzx->setSpeed(TAPE_NORMAL);
  tzxBaseLength=tzx->getPlaybackTime();
  tzx->setSpeed(lfsConfig.tapeSpeed);
  uint8_t mult = (lfsConfig.tapeSpeed == TAPE_NORMAL) ? 1 : (lfsConfig.tapeSpeed == TAPE_TURBO1) ? 2 : 4;
  AYInfo.Length = tzxBaseLength / mult;
  tzx->getTitle(tzxFullTitle,sizeof(tzxFullTitle));
  tzx->initTrackFrame(&PlayerCTRL.trackFrame);
  tzx_total_blocks=tzx->getTotalBlocks();
  tzx_cur_block=tzx->getCurrentBlock();
  const char* blockType = tzx->getBlockType(tzx_cur_block);
  char blockName[32];
  memset(blockName, 0, sizeof(blockName));
  tzx->getBlockName(tzx_cur_block, blockName, sizeof(blockName));
  if(strlen(blockName)==0) snprintf(blockName, sizeof(blockName), "Block %d", tzx_cur_block+1);
  if(strlen(tzxFullTitle)==0){
    strncpy(tzxFullTitle, blockName, sizeof(tzxFullTitle) - 1);
    tzxFullTitle[sizeof(tzxFullTitle) - 1] = '\0';
  }
  strncpy(AYInfo.Name, tzxFullTitle, sizeof(AYInfo.Name) - 1);
  AYInfo.Name[sizeof(AYInfo.Name) - 1] = '\0';
  snprintf(AYInfo.Author, sizeof(AYInfo.Author), "%s", blockName);
  snprintf(blockTypeLabel, sizeof(blockTypeLabel), "%s", blockType);
}

void TZX_Loop(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(tzx&&tzx->isRunning()){
    tzx_cur_block=tzx->getCurrentBlock();
    if(tzx_cur_block != tzxPrevBlock){
      char blockName[32];
      memset(blockName, 0, sizeof(blockName));
      const char* blockType = tzx->getBlockType(tzx_cur_block);
      tzx->getBlockName(tzx_cur_block, blockName, sizeof(blockName));
      if(strlen(blockName)==0) snprintf(AYInfo.Author, sizeof(AYInfo.Author), "Block %d", tzx_cur_block+1);
      else snprintf(AYInfo.Author, sizeof(AYInfo.Author), "%s", blockName);
      snprintf(blockTypeLabel, sizeof(blockTypeLabel), "%s", blockType);
      tzxPrevBlock = tzx_cur_block;
    }
    if(PlayerCTRL.isPlay){
      if(!tzx->loop()){
        tzx->stop();
        PlayerCTRL.isFinish=true;
      }
    } else {
      char blockName[32];
      memset(blockName, 0, sizeof(blockName));
      const char* blockType = tzx->getBlockType(tzx_cur_block);
      tzx->getBlockName(tzx_cur_block, blockName, sizeof(blockName));
      if(strlen(blockName)==0) snprintf(AYInfo.Author, sizeof(AYInfo.Author), "Block %d", tzx_cur_block+1);
      else snprintf(AYInfo.Author, sizeof(AYInfo.Author), "%s", blockName);
      snprintf(blockTypeLabel, sizeof(blockTypeLabel), "%s", blockType);
    }
  }
}

void TZX_Play(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(tzx&&!tzxInitialized){
    initOut();
    tzx->begin(modFile,out);
    tzxInitialized=true;
  }
  TZX_Loop();
}
