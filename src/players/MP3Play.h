#include <AudioGeneratorMP3.h>

AudioGeneratorMP3 *mp3;

void MP3_Cleanup(){
  if(mp3) mp3->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete mp3;
  delete modFile;
  mp3=nullptr;
  modFile=nullptr;
  out->stop();
  vTaskDelay(pdMS_TO_TICKS(10));
  skipMod=false;
  isVBR=false;
}

void MP3_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  mp3=new AudioGeneratorMP3();
  modFile->open(filename);
  bool status=mp3->initializeFile(modFile);
  mp3->initEQBuffers(bufEQ,modEQchn);
  AYInfo.Length=mp3->getPlaybackTime();
  mp3->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  mp3->getDescription(AYInfo.Author,sizeof(AYInfo.Author));
  mp3->initTrackFrame(&PlayerCTRL.trackFrame);
  bitrate=mp3->getBitrate();
  channelMode=mp3->getChannelMode();
  isVBR=mp3->isVBR();
}

void MP3_Loop(){
  if(skipMod){
    if(lfsConfig.play_mode==PLAY_MODE_ONE) sdConfig.play_cur++;
    PlayerCTRL.isFinish=true;
    return;
  }
  if(mp3&&mp3->isRunning()&&PlayerCTRL.isPlay){
    if(!mp3->loop()){
      mp3->stop();
      PlayerCTRL.isFinish=true;
    }
  }
}

void MP3_Play(){
  if(mp3&&!mp3->isRunning()){
    initOut();
    mp3->begin(modFile,out);
  }
  MP3_Loop();
}
