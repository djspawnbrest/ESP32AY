/* gs_mem.h - what z80user.h's macros reach inside gs.c.  Not for anyone
 * else: the host's side is gs.h.
 */

#ifndef GS_MEM_H
#define GS_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *gs_rd[4];         /* 16 KB banks the Z80 reads     */
extern unsigned char *gs_wr[4];         /* ... and writes; NULL = ROM    */
extern unsigned char  gs_dac[4];        /* the four DAC latches          */

unsigned char gs_in(unsigned port);
void          gs_out(unsigned port, unsigned char v);

#ifdef __cplusplus
}
#endif

#endif
