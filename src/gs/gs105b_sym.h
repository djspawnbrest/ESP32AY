// gs105b_sym.h - General Sound ROM 1.05b's variables (host/gssym.py of
// github.com/lordamot/retro-esp32ay-zplayer, from its `make gsrom`'s
// symbol file).  In the card's fixed RAM, 4000h-7FFFh:
// the song position, the row, the speed; the channels, CHANLEN apart.
#pragma once
#define GS_MTSNGPS  0x415B
#define GS_MTPATPS  0x415A
#define GS_MTSPEED  0x4158
#define GS_MTSTAT   0x4151
#define GS_MTVOL    0x4165
#define GS_MTCHNS   0x40A3
#define GS_CHANS    0x4600
#define GS_CHANLEN  0x0040
#define GS_CHSTAT   0x0000
#define GS_CHREAL   0x0014
#define GS_CHNOTE   0x0028
#define GS_CHVOL    0x0019
#define GS_CHMVOL   0x001A
#define GS_CHINS    0x0029
#define GS_CHSMP    0x002A
#define GS_CHOLDV   0x003F
