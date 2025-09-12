#include <AudioGeneratorXM.h>

AudioGeneratorXM *xm;

void setXmSeparation(){
  if(xm) xm->SetSeparation(lfsConfig.modStereoSeparation);
}

void XM_Cleanup(){
  if(xm) xm->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete xm;
  delete modFile;
  xm=nullptr;
  modFile=nullptr;
  out->stop();
  delete out;
  out=nullptr;
  skipMod=false;
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
    return;
  }
  xm->playerTaskEnable(false);
  xm->initEQBuffers(bufEQ,modEQchn);
  modChannels=xm->getNumberOfChannels();
  modChannelsEQ=(modChannels>8)?8:modChannels;
  if(modChannels<2||modChannels>32){AYInfo.Length=1;skipMod=true;return;}
  AYInfo.Length=xm->getPlaybackTime();
  xm->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  xm->getDescription(AYInfo.Author,sizeof(AYInfo.Author));
  xm->initTrackFrame(&PlayerCTRL.trackFrame);
  if(lfsConfig.play_mode==PLAY_MODE_ONE) xm->SetLoop(true); // set loop control if loop once enabled on load
}

void XM_Loop(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(xm&&xm->isRunning()&&PlayerCTRL.isPlay){
    if(!xm->loop()){
      xm->stop();
      PlayerCTRL.isFinish=true;
    }
  }
}

void XM_Play(){
  if(xm&&!xm->isRunning()){
    xm->begin(modFile,out);
  }
  XM_Loop();
}
