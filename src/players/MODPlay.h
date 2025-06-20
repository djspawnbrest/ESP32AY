#include <AudioGeneratorMOD.h>

AudioGeneratorMOD *mod;

void setModSeparation(){
  if(mod) mod->SetSeparation(lfsConfig.modStereoSeparation);
}

void MOD_Cleanup(){
  if(mod) mod->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete mod;
  delete modFile;
  mod=nullptr;
  modFile=nullptr;
  out->stop();
  delete out;
  out=nullptr;
  skipMod=false;
}

void MOD_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  mod=new AudioGeneratorMOD();
  #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
  mod->SetBufferSize(1024*6);
  #endif
  mod->SetStereoSeparation(lfsConfig.modStereoSeparation);
  modFile->open(filename);
  mod->initializeFile(modFile);
  mod->initEQBuffers(bufEQ,modEQchn);
  modChannels=mod->getNumberOfChannels();
  modChannelsEQ=(modChannels>8)?8:modChannels;
  #if !defined(CONFIG_IDF_TARGET_ESP32S3)&&!defined(BOARD_HAS_PSRAM)
  if(modChannels<2||modChannels>16){AYInfo.Length=1;skipMod=true;return;}
  #else
  if(modChannels<2||modChannels>32){AYInfo.Length=1;skipMod=true;return;}
  #endif
  AYInfo.Length=mod->getPlaybackTime();
  mod->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  mod->getDescription(AYInfo.Author,sizeof(AYInfo.Author));
  mod->initTrackFrame(&PlayerCTRL.trackFrame);
}

void MOD_Loop() {
  if(skipMod){PlayerCTRL.isFinish=true;return;}
  if(mod&&mod->isRunning()&&PlayerCTRL.isPlay){
    if(!mod->loop()){
      mod->stop();
      PlayerCTRL.isFinish=true;
    }
  }
}

void MOD_Play(){
  if(mod&&!mod->isRunning()){
    mod->begin(modFile,out);
  }
  MOD_Loop();
}
