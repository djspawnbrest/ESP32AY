/* z80user.h - General Sound's memory and ports for z80emu (gs.c).
 *
 *   0000-3FFF  the ROM's first half          gs_rd[0]
 *   4000-7FFF  the fixed 16 KB               gs_rd[1], gs_wr[1]
 *   8000-FFFF  the window: page 0 the ROM,   gs_rd[2..3], gs_wr[2..3]
 *              page n RAM block n            (NULL = a ROM write, dropped)
 *   6000-7FFF  a read also latches its byte into DAC A9:A8
 *
 * gs_rd/gs_wr are four 16 KB banks, set by gs_page().  Every read goes
 * through GS_RD, opcode fetches included: on the card the DAC latch sees
 * any read of 6000-7FFF.
 */

#ifndef __Z80USER_INCLUDED__
#define __Z80USER_INCLUDED__

#include "../gs_mem.h"

#define GS_RD(address, x) {                                             \
        unsigned _a = (address) & 0xffff;                               \
        (x) = gs_rd[_a >> 14][_a & 0x3fff];                             \
        if ((_a & 0xe000) == 0x6000) gs_dac[(_a >> 8) & 3] = (x);       \
}

#define GS_WR(address, x) {                                             \
        unsigned _a = (address) & 0xffff;                               \
        unsigned char *_b = gs_wr[_a >> 14];                            \
        if (_b) _b[_a & 0x3fff] = (unsigned char)(x);                   \
}

#define Z80_READ_BYTE(address, x)       GS_RD((address), (x))
#define Z80_FETCH_BYTE(address, x)      GS_RD((address), (x))

#define Z80_READ_WORD(address, x) {                                     \
        unsigned _lo, _hi;                                              \
        GS_RD((address), _lo);                                          \
        GS_RD((address) + 1, _hi);                                      \
        (x) = _lo | (_hi << 8);                                         \
}
#define Z80_FETCH_WORD(address, x)      Z80_READ_WORD((address), (x))

#define Z80_WRITE_BYTE(address, x)      GS_WR((address), (x))
#define Z80_WRITE_WORD(address, x) {                                    \
        GS_WR((address), (x));                                          \
        GS_WR((address) + 1, (x) >> 8);                                 \
}

#define Z80_READ_WORD_INTERRUPT(address, x)     Z80_READ_WORD((address), (x))
#define Z80_WRITE_WORD_INTERRUPT(address, x)    Z80_WRITE_WORD((address), (x))

#define Z80_INPUT_BYTE(port, x)         (x) = gs_in((port) & 0x0f);
#define Z80_OUTPUT_BYTE(port, x)        gs_out((port) & 0x0f, (x));

#endif
