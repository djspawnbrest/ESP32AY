/* gs_proto.h - the host's half of General Sound's protocol: what a
 * Spectrum program (ZPLAY) does through BBh and B3h, after Stinger's
 * programming guide and the ROM's own source (external/gs-firmware:
 * COM_L.a80, COM_H.a80, LOAD_L.a80).
 *
 *   SC  send command   (BBh written)       WC  wait: command flag falls
 *   SD  send data      (B3h written)       WD  wait: data flag falls
 *   GD  get data       (B3h read)          WN  wait: data flag rises
 *
 * Every wait calls idle() between polls: on the host that runs the card a
 * period, on the board it yields while the other core runs it.  A wait
 * gives up after `timeout` polls, or as soon as *cancel is set (the player
 * skipped the track), and the call returns GSP_TIMEOUT.
 */

#ifndef GS_PROTO_H
#define GS_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GSP_OK          0
#define GSP_TIMEOUT     (-1)
#define GSP_REFUSED     (-2)

typedef struct {
    void     (*idle)(void *ctx);
    void      *ctx;
    uint32_t   timeout;         /* polls before a wait gives up */
    volatile const uint8_t *cancel;     /* nonzero: give up now; NULL: never */
} gsp_t;

int gsp_cmd(gsp_t *g, uint8_t c);                       /* SC c, WC             */
int gsp_get(gsp_t *g, uint8_t c, uint8_t *v);           /* SC c, WC, GD         */
int gsp_mem_size(gsp_t *g, uint32_t *bytes);            /* #20: RAM the ROM found */

/* A module: #30 returns its handle, every byte goes through B3h, #D2
 * closes the stream.  The ROM's loader takes the bytes into its free RAM
 * as they come (LOAD_L.a80) and parses the module after the close.
 */
int gsp_load_begin(gsp_t *g, uint8_t *handle);
int gsp_load_byte(gsp_t *g, uint8_t b);
int gsp_load_end(gsp_t *g);

int gsp_play(gsp_t *g, uint8_t handle);                 /* #31                  */
int gsp_stop(gsp_t *g);                                 /* #32                  */
int gsp_continue(gsp_t *g);                             /* #33                  */
int gsp_song_pos(gsp_t *g, uint8_t *pos);               /* #60                  */
int gsp_pattern_pos(gsp_t *g, uint8_t *row);            /* #61                  */

#ifdef __cplusplus
}
#endif

#endif
