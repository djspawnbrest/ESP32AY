// #include "defines.h"
#include <JC_EEPROM.h>      // https://github.com/JChristensen/JC_EEPROM
#include <Streaming.h>      // https://github.com/janelia-arduino/Streaming
#include <Wire.h>

#define LFS_FLAG_ADDRESS 2560
#define LFS_START_ADDRESS 2561
#define SD_FLAG_ADDRESS 255
#define SD_START_ADDRESS 256

#define CHUNK_SIZE 32 // bytes to write at a time
// Two 24LC256 EEPROMs on the bus
JC_EEPROM eep(JC_EEPROM::kbits_32,1,CHUNK_SIZE,0x50); // device size, number of devices, page size
constexpr uint32_t totalKBytes {4};           // total kBytes of eeprom
uint8_t eepStatus=1;                          // eeprom init status ()
bool eepInit=false;

// write 0xFF to eeprom, "chunk" bytes at a time
void eeErase(uint8_t chunk, uint32_t startAddr, uint32_t endAddr){
  chunk&=0xFC;          // force chunk to be a multiple of 4
  uint8_t data[chunk];
  printf("Erasing EEPROM....\n");
  for(int16_t i=0;i<chunk;i++) data[i]=0xFF;
  uint32_t msStart=millis();
  for (uint32_t a=startAddr;a<=endAddr;a+=chunk){
    if((a&0xFFF)==0) printf("%d ",a);
    eep.write(a,data,chunk);
  }
  uint32_t msLapse = millis() - msStart;
  printf("\nErase lapse: %d ms.\n",msLapse);
}

void eeInit(uint8_t eepAddr){
  if(eepInit) return;
  eep.setAddress(eepAddr);
  eepStatus=eep.begin(JC_EEPROM::twiClock400kHz);   // go fast!
  if(!eepStatus) eepInit=true;
  printf("EEPROM status: %s\n", eepStatus?"failed!":"ok.");
}
