/* gs_proto.c - the host's half of General Sound's protocol (gs_proto.h). */

#include "gs.h"
#include "gs_proto.h"

static int wait_flag(gsp_t *g, uint8_t mask, uint8_t want)
{
    for (uint32_t n = 0; n < g->timeout; n++) {
        if ((gs_host_status() & mask) == want)
            return GSP_OK;
        if (g->cancel && *g->cancel)
            return GSP_TIMEOUT;
        g->idle(g->ctx);
    }
    return GSP_TIMEOUT;
}

#define WC(g)   wait_flag((g), 0x01, 0x00)
#define WD(g)   wait_flag((g), 0x80, 0x00)
#define WN(g)   wait_flag((g), 0x80, 0x80)

int gsp_cmd(gsp_t *g, uint8_t c)
{
    gs_host_cmd(c);
    return WC(g);
}

int gsp_get(gsp_t *g, uint8_t c, uint8_t *v)
{
    int e = gsp_cmd(g, c);
    if (e)
        return e;
    *v = gs_host_read();
    return GSP_OK;
}

/* #20: three bytes, low first, each after the host took the one before */
int gsp_mem_size(gsp_t *g, uint32_t *bytes)
{
    uint8_t b[3];
    int e = gsp_cmd(g, 0x20);
    for (int i = 0; !e && i < 3; i++) {
        e = WN(g);
        if (!e)
            b[i] = gs_host_read();
    }
    if (!e)
        *bytes = b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16;
    return e;
}

int gsp_load_begin(gsp_t *g, uint8_t *handle)
{
    int e = gsp_get(g, 0x30, handle);
    if (!e && *handle == 0)
        return GSP_REFUSED;
    return e;
}

int gsp_load_byte(gsp_t *g, uint8_t b)
{
    gs_host_write(b);
    return WD(g);
}

int gsp_load_end(gsp_t *g)
{
    return gsp_cmd(g, 0xd2);
}

int gsp_play(gsp_t *g, uint8_t handle)
{
    gs_host_write(handle);
    return gsp_cmd(g, 0x31);
}

int gsp_stop(gsp_t *g)            { return gsp_cmd(g, 0x32); }
int gsp_continue(gsp_t *g)        { return gsp_cmd(g, 0x33); }
int gsp_song_pos(gsp_t *g, uint8_t *pos)    { return gsp_get(g, 0x60, pos); }
int gsp_pattern_pos(gsp_t *g, uint8_t *row) { return gsp_get(g, 0x61, row); }
