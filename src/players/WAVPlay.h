#include <AudioGeneratorWAV.h>

AudioGeneratorWAV *wav;

void WAV_Cleanup(){
  if(wav) wav->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete wav;
  delete modFile;
  wav=nullptr;
  modFile=nullptr;
  out->stop();
  vTaskDelay(pdMS_TO_TICKS(10));
  skipMod=false;
}

void WAV_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  wav=new AudioGeneratorWAV();
  wav->SetBufferSize(2048);
  modFile->open(filename);
  wav->initializeFile(modFile);
  
  wav->initEQBuffers(bufEQ,modEQchn);
  AYInfo.Length=wav->getPlaybackTime();
  wav->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  wav->getDescription(AYInfo.Author,sizeof(AYInfo.Author));
  wav->initTrackFrame(&PlayerCTRL.trackFrame);
  bitrate=wav->getBitrate();
  channelMode=wav->getChannelMode();
}

void WAV_Loop(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(wav&&wav->isRunning()&&PlayerCTRL.isPlay){
    if(!wav->loop()){
      wav->stop();
      PlayerCTRL.isFinish=true;
    }
  }
}

void WAV_Play(){
  if(wav&&!wav->isRunning()){
    initOut();
    wav->begin(modFile,out);
  }
  WAV_Loop();
}
