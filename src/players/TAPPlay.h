#include <AudioGeneratorTAP.h>

extern int tap_cur_block;
extern int tap_total_blocks;
extern char blockTypeLabel[16];
extern AudioGeneratorTAP *tap;
AudioGeneratorTAP *tap=nullptr;

void setTapSpeed(){
  if(tap) tap->setSpeed(lfsConfig.tapeSpeed);
}

void TAP_Cleanup(){
  if(tap) tap->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete tap;
  delete modFile;
  tap=nullptr;
  modFile=nullptr;
  out->stop();
  vTaskDelay(pdMS_TO_TICKS(10));
  skipMod=false;
}

void TAP_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  tap=new AudioGeneratorTAP();
  modFile->open(filename);
  const char* message;
  bool status=tap->initializeFile(modFile,&message);
  if(!status){
    if(message&&strlen(message)>0){
      showAlert(message);
    }
    AYInfo.Length=1;
    skipMod=true;
    return;
  }
  tap->initEQBuffers(bufEQ,modEQchn);
  tap->initTrackFrame(&PlayerCTRL.trackFrame,&AYInfo.Length,AYInfo.Author,blockTypeLabel);
  AYInfo.Length=tap->getPlaybackTime();
  tap->setSpeed(lfsConfig.tapeSpeed);
  tap->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  tap_total_blocks=tap->getTotalBlocks();
  tap_cur_block=tap->getCurrentBlock();
  const char* blockType=tap->getBlockType(tap_cur_block);
  char blockName[32];
  memset(blockName,0,sizeof(blockName));
  tap->getBlockName(tap_cur_block,blockName,sizeof(blockName));
  if(strlen(blockName)==0){
    snprintf(AYInfo.Author,sizeof(AYInfo.Author),"Block %d",tap_cur_block+1);
  }else{
    snprintf(AYInfo.Author,sizeof(AYInfo.Author),"%s",blockName);
  }
  snprintf(blockTypeLabel,sizeof(blockTypeLabel),"%s",blockType);
}

void TAP_Loop(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(tap&&tap->isRunning()){
    tap_cur_block=tap->getCurrentBlock();
    if(PlayerCTRL.isPlay){
      if(!tap->loop()){
        tap->stop();
        if(lfsConfig.skipTapeFormats) PlayerCTRL.isFinish=true;
        else PlayerCTRL.isPlay=false;
      }
    }
  }
}

void TAP_Play(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(tap&&!tap->isRunning()){
    initOut();
    tap->begin(modFile,out);
  }
  TAP_Loop();
}
