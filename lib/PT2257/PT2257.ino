#include <Wire.h>
#include <PT2257.h>
PT2257 rt;

void setup() {
  Wire.begin();
}

void loop() {
  rt.setLeft(33); // int 0...79 
  rt.setRight(33);// int 0...79
  rt.setMute(0);  // int 0...1
  delay(1000);
}
