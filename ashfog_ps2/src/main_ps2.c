/* ASHFOG - PS2 backend (ps2sdk: gsKit + dmaKit + libpad).
 * Геймплей/геометрия - в game.c / render.c (проверены на хосте), здесь только железо. */
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "game.h"
#include "render.h"

static GSGLOBAL *gs;
static char padbuf[256] __attribute__((aligned(64)));
static float yscale = 1.f;
static int tri_count;

/* ---------- вывод треугольников ---------- */
static void gs_tri(void *ud, const Vtx *a, const Vtx *b, const Vtx *c) {
    (void)ud;
    /* GS: Z-test GEQUAL -> ближе = больше значение */
    int za = (int)((1.f - a->z) * 0xFFFFFF), zb = (int)((1.f - b->z) * 0xFFFFFF), zc = (int)((1.f - c->z) * 0xFFFFFF);
    gsKit_prim_triangle_gouraud_3d(gs, a->x, a->y * yscale, za, b->x, b->y * yscale, zb, c->x, c->y * yscale, zc,
        GS_SETREG_RGBAQ(a->r, a->g, a->b, 0x80, 0), GS_SETREG_RGBAQ(b->r, b->g, b->b, 0x80, 0), GS_SETREG_RGBAQ(c->r, c->g, c->b, 0x80, 0));
    if (++tri_count % 400 == 0) gsKit_queue_exec(gs);      /* не переполняем очередь GIF */
}
static void rect(float x0, float y0, float x1, float y1, int z, uint64_t col) {
    gsKit_prim_sprite(gs, x0, y0 * yscale, x1, y1 * yscale, z, col);
}

/* ---------- геймпад ---------- */
static int pad_ok;
static void pad_init(void) {
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    if (padPortOpen(0, 0, padbuf) == 0) { printf("padPortOpen failed\n"); return; }
    for (int i = 0; i < 100000; i++) { if (padGetState(0, 0) == PAD_STATE_STABLE) break; }
    padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
    pad_ok = 1;
}
static void pad_read(Input *in) {
    static unsigned prev; struct padButtonStatus b; memset(in, 0, sizeof *in);
    if (!pad_ok || padGetState(0, 0) != PAD_STATE_STABLE || padRead(0, 0, &b) == 0) return;
    unsigned btn = 0xFFFF ^ b.btns, press = btn & ~prev; prev = btn;
    float ly = (127.f - b.ljoy_v) / 127.f, lx = (b.ljoy_h - 128.f) / 128.f;
    if (fabsf(ly) < .25f) ly = 0; if (fabsf(lx) < .25f) lx = 0;
    if (btn & PAD_UP) ly = 1; if (btn & PAD_DOWN) ly = -1; if (btn & PAD_LEFT) lx = -1; if (btn & PAD_RIGHT) lx = 1;
    in->fwd = ly; in->turn = lx; in->run = (btn & PAD_SQUARE) || (btn & PAD_R1);
    in->act = (press & PAD_CROSS) != 0; in->heal = (press & PAD_TRIANGLE) != 0;
}

/* ---------- HUD (без шрифта: полосы и значки) ---------- */
static void hud(const Game *G) {
    rect(16, 16, 16 + 204, 30, 1, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
    rect(18, 18, 18 + 2 * G->p.hp, 28, 1, GS_SETREG_RGBAQ(G->p.hp < 30 ? 200 : 60, G->p.hp < 30 ? 30 : 170, 40, 0x80, 0));
    for (int i = 0; i < G->hp_items; i++) rect(18 + i * 14, 36, 28 + i * 14, 46, 1, GS_SETREG_RGBAQ(220, 60, 60, 0x80, 0));
    for (int i = 0; i < G->nkeys; i++) rect(18 + i * 14, 52, 28 + i * 14, 62, 1, GS_SETREG_RGBAQ(240, 200, 60, 0x80, 0));
    if (G->msg_t > 0) rect(220, 400, 420, 404, 1, GS_SETREG_RGBAQ(230, 230, 230, 0x80, 0));   /* индикатор сообщения */
}

int main(void) {
    gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT24; gs->PSMZ = GS_PSMZ_24;
    gs->DoubleBuffering = GS_SETTING_ON; gs->ZBuffering = GS_SETTING_ON;
    gs->Interlace = GS_INTERLACED; gs->Field = GS_FIELD; gs->Width = 640;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    yscale = (float)gs->Height / SCR_H;
    pad_init();

    static Game G; memset(&G, 0, sizeof G);
    game_load(&G, find_level("hospital"));
    uint8_t sky[3] = {0, 0, 0};
    const float dt = 1.f / 60.f;      /* фиксированный шаг; при 50 Гц (PAL) игра идёт чуть медленнее */

    for (;;) {
        Input in; pad_read(&in);
        if (G.dead) { if (in.act) { int hp = 100; memset(&G, 0, sizeof G); (void)hp; game_load(&G, find_level("hospital")); } }
        else game_update(&G, &in, dt);
        if (G.level_done) {
            const LevelDef *n = find_level(G.next_level);
            if (n) { G.level_done = 0; G.p.hp = G.p.hp ? G.p.hp : 100; game_load(&G, n); }
            else { memset(&G, 0, sizeof G); game_load(&G, find_level("hospital")); }   /* END -> заново */
        }
        tri_count = 0;
        const LevelDef *L = G.lv;
        gsKit_clear(gs, GS_SETREG_RGBAQ(L->fog[0], L->fog[1], L->fog[2], 0x00, 0));
        render_scene(&G, gs_tri, NULL, sky);
        hud(&G);
        if (G.dead) rect(0, 0, 640, 448, 1, GS_SETREG_RGBAQ(120, 0, 0, 0x40, 0));
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
    return 0;
}
