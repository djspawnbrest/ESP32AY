#ifndef _AUDIOFILESOURCESDFAT_H
#define _AUDIOFILESOURCESDFAT_H

#include "AudioFileSource.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <SdFat.h>

class AudioFileSourceSDFAT:public AudioFileSource{
public:
  AudioFileSourceSDFAT();  // Original constructor
  AudioFileSourceSDFAT(SemaphoreHandle_t semaphore);  // Сonstructor with semaphore
  AudioFileSourceSDFAT(const char *filename);  // Сonstructor with filename
  AudioFileSourceSDFAT(const char *filename,SemaphoreHandle_t semaphore);  // Сonstructor with both

  virtual ~AudioFileSourceSDFAT() override;
  
  virtual bool open(const char *filename) override;
  virtual uint32_t read(void *data,uint32_t len) override;
  virtual bool seek(int32_t pos,int dir) override;
  virtual bool close() override;
  virtual bool isOpen() override;
  virtual uint32_t getSize() override;
  virtual uint32_t getPos() override;

private:
  FsFile f;
  SemaphoreHandle_t _semaphore;
  bool _useSemaphore;
};

#endif
