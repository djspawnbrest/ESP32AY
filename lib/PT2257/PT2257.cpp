#include <Arduino.h>
#include <Wire.h>
#include "PT2257.h"

PT2257::PT2257(){
	Wire.begin();
}

void PT2257::setLeft(int left){
        left = 78 - left;
    int left10 = left / 10;
    int left01 = left - left10 * 10;

  writeWire(left10+0b10110000);
  writeWire(left01+0b10100000);
}

void PT2257::setRight(int right){
        right = 78 - right;
    int right10 = right / 10;
    int right01 = right - right10 * 10;

  writeWire(right10+0b00110000);
  writeWire(right01+0b00100000);
}

void PT2257::setMute(int mute){
 if(mute == 1){
  writeWire(0b01111001);}
 else{
  writeWire(0b01111000);}
}

void PT2257::writeWire(char a){
  Wire.beginTransmission(PT2257_address);
  Wire.write (a);
  Wire.endTransmission();
}
