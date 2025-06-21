#include <AudioGeneratorS3M.h>

AudioGeneratorS3M *s3m;

void setS3mSeparation(){
  if(s3m) s3m->SetSeparation(lfsConfig.modStereoSeparation);
}

void S3M_Cleanup(){
  if(s3m) s3m->stop();
  memset(modEQchn,0,sizeof(modEQchn));
  memset(bufEQ,0,sizeof(bufEQ));
  memset(&music_data,0,sizeof(music_data));
  memset(&AYInfo,0,sizeof(AYInfo));
  modFile->close();
  delete s3m;
  delete modFile;
  s3m=nullptr;
  modFile=nullptr;
  out->stop();
  delete out;
  out=nullptr;
  skipMod=false;
}

void S3M_GetInfo(const char *filename){
  modFile=new AudioFileSourceSDFAT(sdCardSemaphore);
  s3m=new AudioGeneratorS3M();
  s3m->SetStereoSeparation(lfsConfig.modStereoSeparation);
  modFile->open(filename);
  s3m->initializeFile(modFile);
  s3m->initEQBuffers(bufEQ,modEQchn);
  modChannels=s3m->getNumberOfChannels();
  modChannelsEQ=(modChannels>8)?8:modChannels;
  #if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(BOARD_HAS_PSRAM)
  if(modChannels<2||modChannels>16){AYInfo.Length=1;skipMod=true;return;}
  #else
  if(modChannels<2||modChannels>32){AYInfo.Length=1;skipMod=true;return;}
  #endif
  AYInfo.Length=s3m->getPlaybackTime();
  s3m->getTitle(AYInfo.Name,sizeof(AYInfo.Name));
  s3m->getDescription(AYInfo.Author,sizeof(AYInfo.Author));
  s3m->initTrackFrame(&PlayerCTRL.trackFrame);
	s3m->SetSampleRate((modChannels>14)?32000:44100);
}

void S3M_Loop(){
  if(skipMod){PlayerCTRL.isFinish=true;return;}
  if(s3m&&s3m->isRunning()&&PlayerCTRL.isPlay){
    if(!s3m->loop()){
      s3m->stop();
      PlayerCTRL.isFinish=true;
    }
  }
}

void S3M_Play(){
  if(s3m&&!s3m->isRunning()){
    s3m->begin(modFile,out);
  }
  S3M_Loop();
}
