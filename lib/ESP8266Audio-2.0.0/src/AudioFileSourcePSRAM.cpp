#include "AudioFileSourcePSRAM.h"

AudioFileSourcePSRAM::AudioFileSourcePSRAM()
{
  opened = false;
  psramData = NULL;
  psramLen = 0;
  filePointer = 0;
}

AudioFileSourcePSRAM::AudioFileSourcePSRAM(AudioFileSource *source)
{
  opened = false;
  psramData = NULL;
  psramLen = 0;
  filePointer = 0;
  open(source);
}

AudioFileSourcePSRAM::~AudioFileSourcePSRAM()
{
  close();
}

bool AudioFileSourcePSRAM::open(AudioFileSource *source)
{
  if (!ESP.getPsramSize()) {
    // No PSRAM available
    log_d("No PSRAM available");
    return false;
  }

  if (opened) close();
  
  uint32_t size = source->getSize();
  if (size == 0) return false;
  
  if (size > ESP.getFreePsram()) {
    // Not enough PSRAM
    log_d("Not enough PSRAM: %d < %d", ESP.getFreePsram(), size);
    return false;
  }
  
  psramData = (uint8_t*)ps_malloc(size);
  if (!psramData) {
    return false;
  }

  log_d("Copying %d bytes to PSRAM", size);
  
  // Copy the entire file to PSRAM
  source->seek(0, SEEK_SET);
  uint32_t bytesRead = 0;
  uint32_t chunkSize = 512; // Read in chunks
  uint8_t *ptr = psramData;
  
  while (bytesRead < size) {
    uint32_t toRead = (size - bytesRead < chunkSize) ? size - bytesRead : chunkSize;
    uint32_t read = source->read(ptr, toRead);
    if (read == 0) break; // Error or EOF
    bytesRead += read;
    ptr += read;
  }

  log_d("Copied %d bytes to PSRAM", bytesRead);
  
  psramLen = bytesRead;
  filePointer = 0;
  opened = true;
  
  return true;
}

uint32_t AudioFileSourcePSRAM::read(void *data, uint32_t len)
{
  if (!opened) return 0;
  
  if (filePointer >= psramLen) return 0;
  
  uint32_t toRead = (len < (psramLen - filePointer)) ? len : (psramLen - filePointer);
  memcpy(data, psramData + filePointer, toRead);
  filePointer += toRead;
  
  return toRead;
}

bool AudioFileSourcePSRAM::seek(int32_t pos, int dir)
{
  if (!opened) return false;
  
  if (dir == SEEK_SET) {
    if (pos < 0) return false;
    if (pos > psramLen) return false;
    filePointer = pos;
    return true;
  } else if (dir == SEEK_CUR) {
    int32_t newPos = filePointer + pos;
    if (newPos < 0) return false;
    if (newPos > psramLen) return false;
    filePointer = newPos;
    return true;
  } else if (dir == SEEK_END) {
    int32_t newPos = psramLen + pos;
    if (newPos < 0) return false;
    if (newPos > psramLen) return false;
    filePointer = newPos;
    return true;
  }
  
  return false;
}

bool AudioFileSourcePSRAM::close()
{
  if (psramData) {
    free(psramData);
    psramData = NULL;
  }
  opened = false;
  return true;
}

bool AudioFileSourcePSRAM::isOpen()
{
  return opened;
}

uint32_t AudioFileSourcePSRAM::getSize()
{
  if (!opened) return 0;
  return psramLen;
}
