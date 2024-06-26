// PT2257 | rcl-radio.ru | liman324@yandex.ru | Автор: Лиман А.А.
#ifndef  PT2257_H
#define  PT2257_H

#define  PT2257_address 0x44 // адрес

#include <Arduino.h>
class  PT2257
{
  public:
     PT2257();
        void setLeft(int left);      // 0...-79 дБ  int 0...79
        void setRight(int right);    // 0...-79 дБ  int 0...79
        void setMute(int mute);      // int 1 - on mute  | int 0 - off mute
  private:
	void writeWire(char a);
};
	
#endif // PT2257_H
