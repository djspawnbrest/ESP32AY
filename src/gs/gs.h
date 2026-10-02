/* gs.h - General Sound: X-Trade's card (1997), a Z80 at 12 MHz with its
 * own ROM, 2 MB of RAM and four 8-bit DACs, emulated.
 *
 * The model is ZS-256 Nano's gs.v, seen working on an FPGA board;
 * github.com/lordamot/retro-esp32ay-zplayer (.claude/docs/gs.md) has the
 * account.  Plain C, no Arduino: the same file builds on the ESP32-S3 and
 * on the host (that repository's host/).
 *
 * Two sides:
 *   the card's  - gs_run() executes the card's Z80, 320 T-states (one
 *                 37.5 kHz interrupt period) per output frame;
 *   the host's  - the Spectrum's ports BBh and B3h, gs_host_*(): safe to
 *                 call from another core while gs_run() runs.
 */

#ifndef GS_H
#define GS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GS_CLOCK        12000000        /* the card's Z80                 */
#define GS_TPERIOD      320             /* T-states between interrupts    */
#define GS_RATE         (GS_CLOCK / GS_TPERIOD)         /* 37500 Hz       */
#define GS_ROM_SIZE     0x8000
#define GS_FIXED_SIZE   0x4000
#define GS_PAGE_SIZE    0x8000
#define GS_PAGES_2MB    63              /* the card's most: 6-bit page    */

/* rom: 32 KB, the card's firmware (gs105b.rom).  fixed: 16 KB for
 * 4000-7FFF - on the ESP32 both must be internal SRAM.  ram: pages x 32 KB
 * for window pages 1..pages (PSRAM on the ESP32).  pages is 63 for 2 MB,
 * 15 for 512 KB, 3 for 128 KB.
 */
void     gs_init(uint8_t *rom, uint8_t *fixed, uint8_t *ram, int pages);
void     gs_reset(void);

/* The card's whole state (the Z80, the ports, the fixed RAM) into a buffer
 * of gs_snapshot_size() bytes, and back: saved once the ROM's start is done,
 * a restore is the card just started, in microseconds rather than the RAM
 * test's 10 s.  Not the RAM pages: the ROM keeps nothing of its own there. */
size_t   gs_snapshot_size(void);
void     gs_snapshot_save(void *buf);
void     gs_snapshot_restore(const void *buf);

/* Run the card for `frames` interrupt periods; each gives one stereo
 * frame (L, R interleaved), channels 0+1 left and 2+3 right, each
 * (byte - 128) * volume: +-16256.  Returns the T-states executed.
 */
uint32_t gs_run(int16_t *out, int frames);

/* Run the card for about `budget` T-states (it stops at an instruction's
 * end), at most `max_frames` periods; returns the frames written.  A load
 * steps in small budgets so the next byte follows as soon as the card
 * takes the last.
 */
int      gs_step(int16_t *out, int max_frames, int budget);

/* Each channel's loudest since the last call, 0..63, for the meters. */
void     gs_peaks(uint8_t peaks[4]);

/* The host's side: BBh written (command), BBh read (status: bit 7 data,
 * bit 0 command), B3h written (data for the card), B3h read (data from
 * the card).
 */
void     gs_host_cmd(uint8_t c);
uint8_t  gs_host_status(void);
void     gs_host_write(uint8_t d);
uint8_t  gs_host_read(void);

/* For the screen and the debug console. */
typedef struct {
    uint8_t  page;
    uint8_t  dac[4];
    uint8_t  vol[4];
    uint16_t pc, sp;
    uint8_t  iff1, in_halt;     /* the Z80's interrupt enable, in HALT  */
    uint8_t  f_cmd, f_data;     /* the host's flags                     */
    uint32_t tstates;           /* since gs_reset()                     */
    uint32_t ints;              /* interrupts accepted                  */
    uint32_t halted;            /* of tstates, spent in HALT (skipped)  */
} gs_info_t;
void     gs_info(gs_info_t *info);

/* The card's fixed RAM (4000h-7FFFh), where its firmware keeps its
 * variables - for the screen, which reads what a host would ask for with
 * #60..#64 (retro-esp32ay-zplayer's .claude/docs/gs.md).  gs_poke is for
 * #63's own side effect only (bit 7 of a channel's CHREAL: "this note has
 * been reported"). */
uint8_t  gs_peek(uint16_t addr);
void     gs_poke(uint16_t addr, uint8_t v);

#ifdef __cplusplus
}
#endif

#endif
