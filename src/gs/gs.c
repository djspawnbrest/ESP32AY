#pragma GCC optimize("O2")  /* Arduino builds -Os; the card needs speed */
/* gs.c - General Sound, the card's side and the host's ports.
 *
 * As ZS-256 Nano's gs.v has it (the header there is the specification):
 *
 *   port 00 written: the page;   01 read: the command from the host
 *   02 read: the data byte from the host, and the data flag falls
 *   03 written: a byte for the host, and the data flag rises
 *   04 read: the status;         05 read or written: the command flag falls
 *   06..09 written: the volumes of channels 0..3, six bits
 *   0A: the data flag <= the page's bit 0;  0B: the command flag <= volume 0's bit 5
 *   INT every 320 T-states (37.5 kHz), IM 1, held until acknowledged
 *
 * The card decodes A3..A0 only.  The flags are one byte each, written by
 * one side and cleared by the other, so the host's side may run on the
 * other core: a byte is stored before its flag (volatile, in order).
 */

#include <string.h>
#include "gs.h"
#include "gs_mem.h"
#include "z80/z80emu.h"

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define GS_FAST         IRAM_ATTR
#else
#define GS_FAST
#endif

unsigned char *gs_rd[4];
unsigned char *gs_wr[4];
unsigned char  gs_dac[4];

static Z80_STATE z;
static uint8_t  *rom, *fixed, *ram;
static int       npages;
static uint8_t   page;
static uint8_t   vol[4];
static int       irq;           /* INT held: set each period, cleared when taken */
static int       ei_delay;      /* EI just ran: one more instruction first       */
static int       pt;            /* T-states into the current period              */
static uint32_t  tstates, ints, halted;
static uint8_t   peaks[4];      /* each channel's loudest, since gs_peaks()      */

/* The host's registers.  cmd and to_gs are written by the host, to_zx by
 * the card; each flag is set by one side and cleared by the other.
 */
static volatile uint8_t cmd, to_gs, to_zx, f_cmd, f_data;

static void set_page(uint8_t v)
{
    page = v & 0x3f;
    if (page == 0 || page > npages) {
        /* page 0: the whole ROM; a page beyond the RAM fitted reads as
         * the ROM too and takes no writes, so the firmware's RAM test
         * stops there */
        gs_rd[2] = rom;
        gs_rd[3] = rom + 0x4000;
        gs_wr[2] = gs_wr[3] = NULL;
    } else {
        uint8_t *p = ram + (size_t)(page - 1) * GS_PAGE_SIZE;
        gs_rd[2] = gs_wr[2] = p;
        gs_rd[3] = gs_wr[3] = p + 0x4000;
    }
}

void gs_init(uint8_t *rom_, uint8_t *fixed_, uint8_t *ram_, int pages)
{
    rom = rom_;
    fixed = fixed_;
    ram = ram_;
    npages = pages;
    gs_rd[0] = rom;
    gs_wr[0] = NULL;
    gs_rd[1] = gs_wr[1] = fixed;
    gs_reset();
}

void gs_reset(void)
{
    gsz80_reset(&z);
    z.halted = 0;
    set_page(0);
    memset(vol, 0, sizeof vol);
    memset(gs_dac, 0x80, sizeof gs_dac);
    ei_delay = pt = 0;
    irq = 1;                    /* the first period's INT */
    memset(peaks, 0, sizeof peaks);
    tstates = ints = halted = 0;
    cmd = to_gs = to_zx = 0;
    f_cmd = f_data = 0;
}

/* The card's state, all of it that the ROM can see: the Z80, the page, the
 * ports' latches, the interrupt's timing and the fixed 16 KB (its variables,
 * its stack).  The ROM's tables above 8000h are its own page 0; the RAM
 * pages hold only what it was sent (modules, samples), which it finds again
 * through the fixed RAM's pointers - so a card restored to the state saved
 * after its start is that card, just started, whatever was loaded since.
 */
typedef struct {
    Z80_STATE z;
    uint8_t   page, vol[4], dac[4];
    int       irq, ei_delay, pt;
    uint8_t   cmd, to_gs, to_zx, f_cmd, f_data;
    uint8_t   fixed[GS_FIXED_SIZE];
} gs_snap_t;

size_t gs_snapshot_size(void)
{
    return sizeof(gs_snap_t);
}

void gs_snapshot_save(void *buf)
{
    gs_snap_t *s = (gs_snap_t *)buf;
    s->z = z;
    s->page = page;
    memcpy(s->vol, vol, 4);
    memcpy(s->dac, gs_dac, 4);
    s->irq = irq; s->ei_delay = ei_delay; s->pt = pt;
    s->cmd = cmd; s->to_gs = to_gs; s->to_zx = to_zx; s->f_cmd = f_cmd; s->f_data = f_data;
    memcpy(s->fixed, fixed, GS_FIXED_SIZE);
}

void gs_snapshot_restore(const void *buf)
{
    const gs_snap_t *s = (const gs_snap_t *)buf;
    z = s->z;
    set_page(s->page);
    memcpy(vol, s->vol, 4);
    memcpy(gs_dac, s->dac, 4);
    irq = s->irq; ei_delay = s->ei_delay; pt = s->pt;
    to_gs = s->to_gs; to_zx = s->to_zx; cmd = s->cmd;
    f_cmd = s->f_cmd; f_data = s->f_data;
    memcpy(fixed, s->fixed, GS_FIXED_SIZE);
    memset(peaks, 0, sizeof peaks);
}

/* The card's ports, A3..A0. */
unsigned char GS_FAST gs_in(unsigned port)
{
    switch (port) {
    case 0x1: return cmd;
    case 0x2: { uint8_t v = to_gs; f_data = 0; return v; }
    case 0x3: f_data = 1; return 0xff;
    case 0x4: return (f_data ? 0x80 : 0) | (f_cmd ? 0x01 : 0);
    case 0x5: f_cmd = 0; return 0xff;
    case 0xa: f_data = page & 1; return 0xff;
    case 0xb: f_cmd = (vol[0] >> 5) & 1; return 0xff;
    default:  return 0xff;
    }
}

void GS_FAST gs_out(unsigned port, unsigned char v)
{
    switch (port) {
    case 0x0: set_page(v); break;
    case 0x3: to_zx = v; f_data = 1; break;
    case 0x5: f_cmd = 0; break;
    case 0x6: case 0x7: case 0x8: case 0x9: vol[port - 6] = v & 0x3f; break;
    case 0xa: f_data = page & 1; break;
    case 0xb: f_cmd = (vol[0] >> 5) & 1; break;
    default:  break;
    }
}

static inline int step(int cycles);

static inline const uint8_t *code_at(unsigned pc)
{
    return gs_rd[pc >> 14] + (pc & 0x3fff);
}

#ifdef GS_CHECK_INTDAC
/* The host's check (make gs-intcheck): every DAC handler run both ways, on
 * the card and on a copy by the interpreter, and compared. */
#include <stdio.h>
#include <stdlib.h>
static long intdac_checked;
static int intdac_run(int room)
{
    Z80_STATE z0 = z;
    uint8_t fixed0[GS_FIXED_SIZE], dac0[4];
    memcpy(fixed0, fixed, GS_FIXED_SIZE); memcpy(dac0, gs_dac, 4);
    int n = gsz80_intdac(&z, code_at(z.pc), room);
    if (!n) return 0;
    Z80_STATE z1 = z;
    uint8_t fixed1[GS_FIXED_SIZE], dac1[4];
    memcpy(fixed1, fixed, GS_FIXED_SIZE); memcpy(dac1, gs_dac, 4);
    z = z0; memcpy(fixed, fixed0, GS_FIXED_SIZE); memcpy(gs_dac, dac0, 4);
    int m = 0;
    while (z.pc != z1.pc || m < n) {
        int k = gsz80_emulate(&z, 1);
        m += k;
        if (m > 200) break;
    }
    if (m != n || z.pc != z1.pc || memcmp(&z.registers, &z1.registers, sizeof z.registers)
        || memcmp(z.alternates, z1.alternates, sizeof z.alternates) || z.r != z1.r
        || z.iff1 != z1.iff1 || z.iff2 != z1.iff2 || memcmp(fixed, fixed1, GS_FIXED_SIZE)
        || memcmp(gs_dac, dac1, 4)) {
        fprintf(stderr, "the DAC handler at %04X differs after %ld: T %d/%d pc %04X/%04X r %02X/%02X\n",
                z0.pc, intdac_checked, n, m, z.pc, z1.pc, z.r, z1.r);
        exit(9);
    }
    z.status = 0;
    if (!(intdac_checked++ & 0xfffff)) fprintf(stderr, "intdac: %ld checked\n", intdac_checked);
    return n;
}
#else
static inline int GS_FAST intdac_run(int room) { return gsz80_intdac(&z, code_at(z.pc), room); }
#endif

static inline int step(int cycles)
{
    int n = gsz80_emulate(&z, cycles);
    if (z.status == Z80_STATUS_EI)
        ei_delay = 1;
    return n;
}

/* A period ends: the four latches as they are now make the frame. */
static inline void GS_FAST period_end(int16_t *o)
{
    int s0 = ((int)gs_dac[0] - 128) * vol[0];
    int s1 = ((int)gs_dac[1] - 128) * vol[1];
    int s2 = ((int)gs_dac[2] - 128) * vol[2];
    int s3 = ((int)gs_dac[3] - 128) * vol[3];
    o[0] = (int16_t)(s0 + s1);
    o[1] = (int16_t)(s2 + s3);

    int a;                                      /* 0..63, for the meters */
    a = (s0 < 0 ? -s0 : s0) >> 7; if (a > peaks[0]) peaks[0] = a;
    a = (s1 < 0 ? -s1 : s1) >> 7; if (a > peaks[1]) peaks[1] = a;
    a = (s2 < 0 ? -s2 : s2) >> 7; if (a > peaks[2]) peaks[2] = a;
    a = (s3 < 0 ? -s3 : s3) >> 7; if (a > peaks[3]) peaks[3] = a;
}

int GS_FAST gs_step(int16_t *out, int max_frames, int budget)
{
    int f = 0;

    while (f < max_frames && budget > 0) {
        int t0 = pt;
        int stop = budget < GS_TPERIOD - pt ? pt + budget : GS_TPERIOD;

        while (pt < stop) {
            if (ei_delay) {             /* no INT right after EI */
                ei_delay = 0;
                pt += step(1);
                continue;
            }
            if (irq && z.iff1) {
                pt += gsz80_interrupt(&z, 0xff);
                irq = 0;
                ints++;
                /* the DAC handler in one go when all of it falls in this
                 * slice, as the interpreter would have run it: the frame at
                 * the period's end sees the same latches (z80emu.c) */
                if ((z.pc & 0x3fff) < 0x3fe0 && (z.pc & 0xe000) != 0x6000)
                    pt += intdac_run(stop - pt);
                continue;
            }
            if (z.halted) {             /* HALT: nothing until the next INT */
                halted += stop - pt;
                pt = stop;
                break;
            }
            pt += step(stop - pt);
        }
        budget -= pt - t0;
        tstates += pt - t0;
        if (pt >= GS_TPERIOD) {
            period_end(out + 2 * f++);
            pt -= GS_TPERIOD;
            irq = 1;                    /* the next period's INT, held */
        }
    }
    return f;
}

uint32_t GS_FAST gs_run(int16_t *out, int frames)
{
    uint32_t start = tstates;
    int f = 0;
    while (f < frames)
        f += gs_step(out + 2 * f, frames - f, 0x7fffffff);
    return tstates - start;
}

void gs_peaks(uint8_t p[4])
{
    memcpy(p, peaks, 4);
    memset(peaks, 0, sizeof peaks);
}

void gs_host_cmd(uint8_t c)
{
    cmd = c;
    f_cmd = 1;
}

uint8_t gs_host_status(void)
{
    return (f_data ? 0x80 : 0) | 0x7e | (f_cmd ? 0x01 : 0);
}

void gs_host_write(uint8_t d)
{
    to_gs = d;
    f_data = 1;
}

uint8_t gs_host_read(void)
{
    uint8_t v = to_zx;
    f_data = 0;
    return v;
}

void gs_info(gs_info_t *info)
{
    info->page = page;
    memcpy(info->dac, gs_dac, 4);
    memcpy(info->vol, vol, 4);
    info->pc = (uint16_t)z.pc;
    info->sp = z.registers.word[Z80_SP];
    info->iff1 = (uint8_t)z.iff1;
    info->in_halt = (uint8_t)z.halted;
    info->f_cmd = f_cmd;
    info->f_data = f_data;
    info->tstates = tstates;
    info->ints = ints;
    info->halted = halted;
}

uint8_t gs_peek(uint16_t addr)
{
    return (addr & 0xc000) == 0x4000 ? fixed[addr & 0x3fff] : 0xff;
}

void gs_poke(uint16_t addr, uint8_t v)
{
    if ((addr & 0xc000) == 0x4000)
        fixed[addr & 0x3fff] = v;
}
