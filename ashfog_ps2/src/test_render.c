/* Хост-тест: программный растеризатор -> PPM, чтобы проверить геометрию без PS2. */
#include <stdio.h>
#include <stdlib.h>
#include "render.h"
static uint8_t fb[SCR_H][SCR_W][3]; static float zb[SCR_H][SCR_W];
static float edge(const Vtx *a, const Vtx *b, float x, float y) { return (x - a->x) * (b->y - a->y) - (y - a->y) * (b->x - a->x); }
static void raster(void *ud, const Vtx *a, const Vtx *b, const Vtx *c) {
    float area = edge(a, b, c->x, c->y); if (fabsf(area) < 1e-3f) return;
    int x0 = fmaxf(0, floorf(fminf(a->x, fminf(b->x, c->x)))), x1 = fminf(SCR_W - 1, ceilf(fmaxf(a->x, fmaxf(b->x, c->x))));
    int y0 = fmaxf(0, floorf(fminf(a->y, fminf(b->y, c->y)))), y1 = fminf(SCR_H - 1, ceilf(fmaxf(a->y, fmaxf(b->y, c->y))));
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float w0 = edge(b, c, x + .5f, y + .5f) / area, w1 = edge(c, a, x + .5f, y + .5f) / area, w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        float z = w0 * a->z + w1 * b->z + w2 * c->z; if (z >= zb[y][x]) continue; zb[y][x] = z;
        fb[y][x][0] = w0 * a->r + w1 * b->r + w2 * c->r; fb[y][x][1] = w0 * a->g + w1 * b->g + w2 * c->g; fb[y][x][2] = w0 * a->b + w1 * b->b + w2 * c->b;
    }
}
int main(int argc, char **argv) {
    const char *lv = argc > 1 ? argv[1] : "hospital"; float px = argc > 2 ? atof(argv[2]) : -1, pz = argc > 3 ? atof(argv[3]) : 0, rot = argc > 4 ? atof(argv[4]) : 0;
    Game G; memset(&G, 0, sizeof G); game_load(&G, find_level(lv));
    if (argc > 3) { G.p.x = px; G.p.z = pz; } G.p.rot = rot;
    uint8_t sky[3]; for (int y = 0; y < SCR_H; y++) for (int x = 0; x < SCR_W; x++) zb[y][x] = 2;
    memset(fb, 0, sizeof fb);
    /* сначала узнаём цвет неба */
    int n = render_scene(&G, raster, NULL, sky);
    for (int y = 0; y < SCR_H; y++) for (int x = 0; x < SCR_W; x++) if (zb[y][x] >= 2) { fb[y][x][0] = sky[0]; fb[y][x][1] = sky[1]; fb[y][x][2] = sky[2]; }
    FILE *f = fopen(argc > 5 ? argv[5] : "/tmp/frame.ppm", "wb"); fprintf(f, "P6 %d %d 255\n", SCR_W, SCR_H); fwrite(fb, 1, sizeof fb, f); fclose(f);
    printf("%s: %d triangles\n", lv, n); return 0;
}
