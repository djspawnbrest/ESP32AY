#include <Wire.h>
 
// --- Audio Amplifier I2C definitions ---
#define AMP_ADDRESS 0x60       // TPA6130A2 device address
// Register 1: HP_EN_L, HP_EN_R, Mode[1], Mode[0], Reserved, Reserved, Reserved, Thermal, SWS
#define AMP_REG1 0x1           // TPA6130A2 register 1 address
#define AMP_REG1_SETUP 0xc0    // default configuration: 11000000 - both channels enabled
// Register 2: Mute_L, Mute_R, Volume[5-0]
#define AMP_REG2 0x2           // TPA6130A2 register 2 address
#define AMP_REG2_SETUP 0x3f        // default configuration: 00110100 - both channels on –0.3 dB Gain

bool muteL=false;
bool muteR=false;

void writeToAmp(byte address,byte val){
  if(foundAmp){
    Wire.beginTransmission(ampAddress); // start transmission to device   
    Wire.write(address);                 // send register address
    Wire.write(val);                     // send value to write
    Wire.endTransmission();              // end transmission
  }
}

void ampInit(){
  writeToAmp(AMP_REG1,AMP_REG1_SETUP);
  writeToAmp(AMP_REG2,(muteL<<7|muteR<<6|sdConfig.volume));
}

void muteAmp(){
  writeToAmp(AMP_REG2,(muteL<<7|muteR<<6|0));
}

void unMuteAmp(){
  writeToAmp(AMP_REG2,(muteL<<7|muteR<<6|sdConfig.volume));
}