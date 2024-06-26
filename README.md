# ESP32AY multiformat Turbo Sound player
ESP32 based AY Turbo Sound multiformat player
## Features
### Supported formats:
- .ayl - [Ay_Emul](https://bulba.untergrund.net/emulator_e.htm) by Sergey Bulba own playlist format
- .pt1 - Pro Tracker v1 file format
- .pt2 - Pro Tracker v2 file format
- .pt3 - Pro Tracker v3 (include Turbo Sound on two AY chips) file format
- .stc - Sound Tracker file format
- .stp - Sound Tracker Pro file format
- .asc - Sound Master file format 
- .psc - Pro Sound Creator file format
- .sqt - SQ Tracker file format
- .ay  - AY file format
- .psg - Programmable Sound Generator file format
- .rsf - Registers Stream Flow file format
- .yrg - [Custom AVR-AY format](https://www.avray.ru/ru/avr-ay-player/)

### Controls
- Encoder Hold - change mode player/file browser
- Encoder Click - in player mode - play/pause; in file browser mode select track/playlist/directory; select - in config mode
- Encoder Double click - in player mode - open settings
- Encoder Push and turn right - fast forward (for ay format change next subsong)
- Encoder Push and turn left - slow down (for ay format change previous subsong)
- Left button click/hold - volume - (in player mode); cancel - in config mode
- Right button click/hold - volume + (in player mode)
- Left button double click - change AY clock (in player mode)
- Right button double click - change play mode: all/shuffle/once (in player mode)

### Playing modes:
- Playing all tracks (in folder/playlist)
- Playing random track (in folder/playlist)
- Playing once track (loop)

### Supports fully realtime file browser

### Supports realtime AY layers change:
- ABC
- ACB
- BAC
- BCA
- CAB
- CBA

### Supports realtime AY clock change:
- ZX SPECTRUM - 1773400 Hz
- PENTAGON - 1750000 Hz
- MSX - 1789772 Hz
- CPC - 1000000 Hz
- ATARI ST - 2000000 Hz

### Built-in battery

### Built-in charger and indication on LED and TFT

### Built-in headphones stereo amplifier controlled by I2C bus

## Demo video with digital sound record:

[![Watch the video](https://i.ytimg.com/an_webp/-dr-m1xszBs/mqdefault_6s.webp?du=3000&sqp=CL2L8rMG&rs=AOn4CLDMJ1whNv1aUUzgGtIppH1XILQPmw)](https://youtu.be/-dr-m1xszBs)
