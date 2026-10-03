/* z80config.h - General Sound's copy of z80emu (Lin Ke-Fong), configured
 * for the card.  A second instance beside upstream's players/z80/ (the .ay
 * player's), so its entry points are renamed and its memory macros
 * (z80user.h) are the card's.
 */

#ifndef __Z80CONFIG_INCLUDED__
#define __Z80CONFIG_INCLUDED__

/* Not a big endian host (Xtensa LX7, x86-64). */

#define Z80_DOCUMENTED_FLAGS_ONLY

/* EI is caught: an interrupt pending while EI runs is taken after the
 * NEXT instruction, as on the chip (the firmware's handlers end EI; RET).
 * HALT is not caught: it ends the slice and waits for the interrupt
 * (Z80_STATE.halted).
 */
#define Z80_CATCH_EI

/* The entry points, renamed away from upstream's copy. */
#define Z80Reset                gsz80_reset
#define Z80Interrupt            gsz80_interrupt
#define Z80NonMaskableInterrupt gsz80_nmi
#define Z80Emulate              gsz80_emulate

/* The decoding tables are read on every instruction, and the core
 * (15 KB) runs all the time: both in internal SRAM, out of reach of the
 * flash cache the other core's UI shares.
 */
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define GSZ80_TABLE             DRAM_ATTR
#define GSZ80_FAST              IRAM_ATTR
#else
#define GSZ80_TABLE
#define GSZ80_FAST
#endif
#define pgm_read_byte(p)        (*(const unsigned char *)(p))

#endif
