#include <AudioGeneratorXM.h>

AudioGeneratorXM *xm;

void setXmSeparation(){
  if(xm) xm->SetSeparation(lfsConfig.modStereoSeparation);
}

void XM_Cleanup(){
  xmCleanupInProgress=true;  // Сигнал XM_Loop что идёт очистка
  vTaskDelay(pdMS_TO_TICKS(50));  // УВЕЛИЧЕНА задержка - дать время завершиться всем операциям
  
  if(xm){
    xm->stop();  // Это вызовет stop_xm() в библиотеке
    vTaskDelay(pdMS_TO_TICKS(50));  // Дополнительная задержка после stop()
  }
  
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  
  if(modFile){
    modFile->close();
  }
  
  if(xm){
    delete xm;
    xm=nullptr;
  }
  
  if(modFile){
    delete modFile;
    modFile=nullptr;
  }
  
  if(out){
    out->stop();
  }
  
  vTaskDelay(pdMS_TO_TICKS(10));
  skipMod=false;
  xmCleanupInProgress=false;  // Очистка завершена
}

void XM_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  xm=new AudioGeneratorXM();
  xm->SetStereoSeparation(lfsConfig.modStereoSeparation);
  modFile->open(filename);
  const char* message;
  bool status=xm->initializeFile(modFile,&message);
  // check file and show message
  if(!status){
    // need to show alert screen few second or wait any button click!
    if(message&&strlen(message)>0){
      showAlert(message);  // Show the alert
    }
    AYInfo.Length=1;
    skipMod=true;
    // FIX FOR MEMORY LEAK - delete objects if initialization failed
    if(xm){
      delete xm;
      xm=nullptr;
    }
    if(modFile){
      modFile->close();
      delete modFile;
      modFile=nullptr;
    }
    return;
  }
  xm->playerTaskEnable(false);  // Keep in main loop for pause control
  xm->initEQBuffers(bufEQ,modEQchn);
  modChannels=xm->getNumberOfChannels();
  modChannelsEQ=(modChannels>8)?8:modChannels;
  if(modChannels<2||modChannels>32){
    AYInfo.Length=1;
    skipMod=true;
    // FIX FOR MEMORY LEAK - delete objects if channel check failed
    if(xm){
      delete xm;
      xm=nullptr;
    }
    if(modFile){
      modFile->close();
      delete modFile;
      modFile=nullptr;
    }
    return;
  }
  AYInfo.Length=xm->getPlaybackTime();
  xm->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  xm->getDescription(AYInfo.Author,sizeof(AYInfo.Author));
  xm->initTrackFrame(&PlayerCTRL.trackFrame);
  if(lfsConfig.play_mode==PLAY_MODE_ONE) xm->SetLoop(true); // set loop control if loop once enabled on load
}

void XM_Loop(){
  if(xmCleanupInProgress) return;  // Выход если идёт очистка
  
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(xm&&xm->isRunning()&&PlayerCTRL.isPlay){
    if(!xm->loop()){
      if(!xmCleanupInProgress){  // Дополнительная проверка перед stop()
        xm->stop();
      }
      PlayerCTRL.isFinish=true;
    }
  }
}

void XM_Play(){
  if(xmCleanupInProgress) return;  // Выход если идёт очистка
  
  if(xm&&!xm->isRunning()&&!xmCleanupInProgress){
    initOut();
    xm->begin(modFile,out);
  }
  XM_Loop();
}
