This is an automatic translation, may be incorrect in some places. See sources and examples!

# GyverFIFO
Lightweight Universal Ring Buffer for Arduino
- Read, write, clear
- Static size
- Data type selection

### Compatibility
Compatible with all Arduino platforms (using Arduino functions)

## Content
- [Install](#install)
- [Initialization](#init)
- [Usage](#usage)
- [Example](#example)
- [Versions](#versions)
- [Bugs and feedback](#feedback)

<a id="install"></a>
## Installation
- The library can be found by the name **GyverFIFO** and installed through the library manager in:
    - Arduino IDE
    - Arduino IDE v2
    - PlatformIO
- [Download library](https://github.com/GyverLibs/GyverFIFO/archive/refs/heads/main.zip) .zip archive for manual installation:
    - Unzip and put in *C:\Program Files (x86)\Arduino\libraries* (Windows x64)
    - Unzip and put in *C:\Program Files\Arduino\libraries* (Windows x32)
    - Unpack and put in *Documents/Arduino/libraries/*
    - (Arduino IDE) automatic installation from .zip: *Sketch/Include library/Add .ZIP library…* and specify the downloaded archive
- Read more detailed instructions for installing libraries [here] (https://alexgyver.ru/arduino-first/#%D0%A3%D1%81%D1%82%D0%B0%D0%BD%D0%BE% D0%B2%D0%BA%D0%B0_%D0%B1%D0%B8%D0%B1%D0%BB%D0%B8%D0%BE%D1%82%D0%B5%D0%BA)

<a id="init"></a>
## Initialization
```cpp
GyverFIFO<data type, buffer size> buf;
// data type: any. byte/int/float...
// sizeBuffer: code runs faster when buffer size is a multiple of a power of two (2, 4, 8, 16, 32...)
```

<a id="usage"></a>
## Usage
```cpp
// TYPE - data type specified during initialization
bool write(TYPE newVal); // write to the buffer. Returns true on successful write
bool availableForWrite(); // writable (free space)
TYPE read(); // read from buffer
TYPE peek(); // returns the extreme value without deleting from the buffer
int available(); // will return the number of unread items
void clear(); // "clear" the buffer
```

<a id="example"></a>
## Example
See **examples** for other examples!
```cpp
#include <GyverFIFO.h>
GyverFIFO<int, 16> buf;

void setup() {
  Serial.begin(9600);
  buf.write(12);
  buf.write(34);
  buf.write(56);
  Serial.println(buf.available());
  while (buf.available()) {
    Serial.println(buf.read());
  }
}

void loop() {
}
```

<a id="versions"></a>
## Versions
- v1.0

<a id="feedback"></a>
## Bugs and feedback
When you find bugs, create an **Issue**, or better, immediately write to the mail [alex@alexgyver.ru](mailto:alex@alexgyver.ru)
The library is open for revision and your **Pull Request**'s!