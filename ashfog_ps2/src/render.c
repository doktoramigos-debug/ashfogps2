#include "render.h"
#include <math.h>

typedef struct { float x, y, z; } V3;
typedef struct { V3 pos, r, u, f; float focal; } Cam;
typedef struct { const Game *G; Cam cam; TriFn emit; void *ud; int ntri; uint8_t fog[3]; float fn, ff; } Ctx;

static V3 v3(float x, float y, float z) { V3 a = {x, y, z}; return a; }
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static V3 norm(V3 a) { float l = sqrtf(dot(a, a)); return v3(a.x / l, a.y / l, a.z / l); }

typedef struct { V3 v; float dist; uint8_t c[3]; } CV;      /* вершина в пространстве камеры + цвет с туманом */

static CV make_cv(Ctx *X, V3 w, const uint8_t col[3]) {
    CV o; V3 d = sub(w, X->cam.pos);
    o.v = v3(dot(d, X->cam.r), dot(d, X->cam.u), dot(d, X->cam.f));
    o.dist = sqrtf(dot(d, d));
    float f = (o.dist - X->fn) / (X->ff - X->fn); f = f < 0 ? 0 : f > 1 ? 1 : f;
    for (int i = 0; i < 3; i++) o.c[i] = (uint8_t)(col[i] + (X->fog[i] - col[i]) * f);
    return o;
}
static Vtx project(Ctx *X, CV a) {
    Vtx o; float iz = 1.f / a.v.z;
    o.x = SCR_W / 2 + a.v.x * X->cam.focal * iz; o.y = SCR_H / 2 - a.v.y * X->cam.focal * iz;
    float z = (a.v.z - .1f) / 60.f; o.z = z < 0 ? 0 : z > 1 ? 1 : z;
    o.r = a.c[0]; o.g = a.c[1]; o.b = a.c[2]; return o;
}
static CV lerp_cv(CV a, CV b, float t) {
    CV o; o.v = v3(a.v.x + (b.v.x - a.v.x) * t, a.v.y + (b.v.y - a.v.y) * t, a.v.z + (b.v.z - a.v.z) * t);
    o.dist = a.dist + (b.dist - a.dist) * t;
    for (int i = 0; i < 3; i++) o.c[i] = (uint8_t)(a.c[i] + (b.c[i] - a.c[i]) * t); return o;
}
#define NEAR .15f
static void tri(Ctx *X, CV a, CV b, CV c) {
    CV in[3] = {a, b, c}, out[4]; int n = 0;
    for (int i = 0; i < 3; i++) {                       /* отсечение по ближней плоскости */
        CV p = in[i], q = in[(i + 1) % 3]; int pi = p.v.z >= NEAR, qi = q.v.z >= NEAR;
        if (pi) out[n++] = p;
        if (pi != qi) out[n++] = lerp_cv(p, q, (NEAR - p.v.z) / (q.v.z - p.v.z));
    }
    if (n < 3) return;
    Vtx v0 = project(X, out[0]), v1 = project(X, out[1]), v2 = project(X, out[2]);
    X->emit(X->ud, &v0, &v1, &v2); X->ntri++;
    if (n == 4) { Vtx v3_ = project(X, out[3]); X->emit(X->ud, &v0, &v2, &v3_); X->ntri++; }
}
static void quad(Ctx *X, V3 a, V3 b, V3 c, V3 d, const uint8_t col[3], float shade) {
    uint8_t s[3] = {(uint8_t)(col[0] * shade), (uint8_t)(col[1] * shade), (uint8_t)(col[2] * shade)};
    CV A = make_cv(X, a, s), B = make_cv(X, b, s), C = make_cv(X, c, s), D = make_cv(X, d, s);
    if (A.v.z < NEAR && B.v.z < NEAR && C.v.z < NEAR && D.v.z < NEAR) return;
    tri(X, A, B, C); tri(X, A, C, D);
}
/* бокс с поворотом вокруг Y; (cx,cy,cz) - центр основания */
static void box(Ctx *X, float cx, float y0, float cz, float sx, float sy, float sz, float yaw, const uint8_t col[3]) {
    float cs = cosf(yaw), sn = sinf(yaw); V3 p[8];
    for (int i = 0; i < 8; i++) {
        float lx = (i & 1 ? .5f : -.5f) * sx, lz = (i & 2 ? .5f : -.5f) * sz, ly = (i & 4) ? sy : 0;
        p[i] = v3(cx + lx * cs + lz * sn, y0 + ly, cz - lx * sn + lz * cs);
    }
    quad(X, p[4], p[5], p[7], p[6], col, 1.f);       /* верх */
    quad(X, p[0], p[1], p[5], p[4], col, .62f); quad(X, p[2], p[3], p[7], p[6], col, .62f);
    quad(X, p[0], p[2], p[6], p[4], col, .78f); quad(X, p[1], p[3], p[7], p[5], col, .78f);
}

typedef struct { uint8_t wall[3], floor[3], ceil[3], ground[3]; } Pal;
static const Pal PAL[] = {   /* hospital, city, cemetery */
    {{112, 124, 120}, {78, 82, 80}, {96, 100, 96}, {60, 60, 60}},
    {{128, 112, 98}, {84, 76, 68}, {96, 92, 88}, {62, 62, 66}},
    {{92, 98, 94}, {74, 70, 64}, {80, 80, 76}, {52, 72, 46}},
};
static const uint8_t C_DOOR[3] = {120, 84, 52}, C_LOCK[3] = {120, 44, 36}, C_EXIT[3] = {50, 120, 70}, C_PROP[3] = {98, 74, 52},
    C_TOMB[3] = {120, 120, 116}, C_CAR[3] = {84, 90, 110}, C_ITEM[3] = {230, 210, 90}, C_KEY[3] = {240, 90, 60}, C_STAND[3] = {42, 43, 40},
    C_Z[3] = {138, 150, 126}, C_Y[3] = {176, 90, 90}, C_N[3] = {200, 190, 185}, C_HEAD[3] = {170, 175, 160}, C_PLAYER[3] = {60, 90, 140}, C_SKIN[3] = {210, 180, 150};

static void human(Ctx *X, float x, float z, float rot, const uint8_t body[3], const uint8_t head[3], float sc) {
    float yaw = rot * (float)M_PI / 180.f;
    box(X, x, 0, z, .3f * sc, .85f * sc, .24f * sc, yaw, body);
    box(X, x, .85f * sc, z, .42f * sc, .62f * sc, .26f * sc, yaw, body);
    box(X, x, 1.47f * sc, z, .24f * sc, .26f * sc, .24f * sc, yaw, head);
}

int render_scene(const Game *G, TriFn emit, void *ud, uint8_t sky[3]) {
    Ctx X; memset(&X, 0, sizeof X); X.G = G; X.emit = emit; X.ud = ud; const LevelDef *L = G->lv;
    X.fn = L->fog_near; X.ff = L->fog_far; for (int i = 0; i < 3; i++) X.fog[i] = sky[i] = L->fog[i];
    const Pal *P = &PAL[!strcmp(L->name, "city") ? 1 : !strcmp(L->name, "cemetery") ? 2 : 0];
    /* камера за спиной, с отодвиганием от стен */
    float yaw = G->p.rot * (float)M_PI / 180.f, fx = sinf(yaw), fz = cosf(yaw), back = 3.0f;
    while (back > .5f) {
        float cx = G->p.x - fx * back, cz = G->p.z - fz * back;
        if (!cell_solid(G, (int)floorf(cx / CELL + .5f), (int)floorf(-cz / CELL + .5f))) break; back -= .25f;
    }
    V3 tgt = v3(G->p.x + fx * 1.5f, 1.25f, G->p.z + fz * 1.5f);
    X.cam.pos = v3(G->p.x - fx * back, 1.9f, G->p.z - fz * back);
    X.cam.f = norm(sub(tgt, X.cam.pos)); X.cam.r = norm(cross(v3(0, 1, 0), X.cam.f)); X.cam.u = cross(X.cam.f, X.cam.r);
    X.cam.focal = (SCR_W / 2) / tanf(35.f * (float)M_PI / 180.f);
    int pc = (int)floorf(G->p.x / CELL + .5f), pr = (int)floorf(-G->p.z / CELL + .5f), R = (int)(L->fog_far / CELL) + 2, H = L->h;
    float h = L->h; (void)H;
    for (int r = pr - R; r <= pr + R; r++) for (int c = pc - R; c <= pc + R; c++) {
        if (r < 0 || c < 0 || r >= L->H || c >= L->W) continue;
        float x = c * CELL, z = -r * CELL, e = CELL / 2; char ch = G->g[r][c];
        float dx = x - X.cam.pos.x, dz = z - X.cam.pos.z;
        if (dx * dx + dz * dz > (L->fog_far + CELL) * (L->fog_far + CELL)) continue;
        if (dx * X.cam.f.x + dz * X.cam.f.z < -CELL * 1.5f) continue;     /* позади камеры */
        if (ch == '#') {
            static const int N[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int k = 0; k < 4; k++) {
                int nc = c + N[k][0], nr = r + N[k][1];
                if (nc < 0 || nr < 0 || nc >= L->W || nr >= L->H || G->g[nr][nc] == '#') continue;
                float sx = N[k][0], sz = -N[k][1];
                V3 a, b, cc, d;
                if (sx) { a = v3(x + sx * e, 0, z - e); b = v3(x + sx * e, 0, z + e); cc = v3(x + sx * e, h, z + e); d = v3(x + sx * e, h, z - e); }
                else    { a = v3(x - e, 0, z + sz * e); b = v3(x + e, 0, z + sz * e); cc = v3(x + e, h, z + sz * e); d = v3(x - e, h, z + sz * e); }
                quad(&X, a, b, cc, d, P->wall, sx ? .8f : .62f);
            }
            continue;
        }
        int in = !L->indoor || L->indoor[r][c] == '1';
        const uint8_t *fc = in ? P->floor : P->ground;
        quad(&X, v3(x - e, 0, z - e), v3(x + e, 0, z - e), v3(x + e, 0, z + e), v3(x - e, 0, z + e), fc, ((c + r) & 1) ? .92f : 1.f);
        if (in) quad(&X, v3(x - e, h, z - e), v3(x - e, h, z + e), v3(x + e, h, z + e), v3(x + e, h, z - e), P->ceil, .85f);
        /* двери / мебель */
        if (strchr("DABGWXE", ch)) {
            int lock = 0, ex = 0;
            for (const LockDef *l = L->locks; l->ch; l++) if (l->ch == ch) lock = 1;
            for (const ExitDef *q = L->exits; q->ch; q++) if (q->ch == ch) ex = 1;
            int horiz = G->g[r][c - 1 < 0 ? 0 : c - 1] == '#' && G->g[r][c + 1 >= L->W ? c : c + 1] == '#';
            box(&X, x, 0, z, horiz ? CELL : .4f, h, horiz ? .4f : CELL, 0, ex ? C_EXIT : lock ? C_LOCK : C_DOOR);
        } else if (ch == 'b' || ch == 't' || ch == 'c' || ch == 'k') box(&X, x, 0, z, 1.5f, ch == 'b' ? .6f : ch == 'k' ? 1.8f : 1.f, ch == 'b' ? 1.0f : 1.3f, 0, C_PROP);
        else if (ch == 'm') box(&X, x, 0, z, 1.4f, .8f, 2.0f, 0, C_PROP);
        else if (ch == 'T') box(&X, x, 0, z, .8f, 1.3f, .3f, 0, C_TOMB);
        else if (ch == 'C') box(&X, x, 0, z, 1.8f, 1.2f, 3.8f, .3f, C_CAR);
        else if (ch == 'Z' || ch == 'L') box(&X, x, 0, z, .5f, ch == 'L' ? 3.f : .8f, .5f, 0, C_TOMB);
    }
    for (int i = 0; i < G->nit; i++) if (!G->it[i].taken) {
        const Item *t = &G->it[i]; int key = !strncmp(t->id, "key_", 4);
        box(&X, t->x, 0, t->z, .48f, .15f, .48f, 0, C_STAND); box(&X, t->x, .15f, t->z, key ? .1f : .26f, .14f, key ? .3f : .26f, .6f, key ? C_KEY : C_ITEM);
    }
    for (int i = 0; i < G->nen; i++) {
        const Enemy *e = &G->en[i]; if (!e->alive) continue;
        human(&X, e->x, e->z, e->rot, e->kind == 'Y' ? C_Y : e->kind == 'N' ? C_N : C_Z, C_HEAD, e->kind == 'Y' ? 1.15f : 1.f);
    }
    human(&X, G->p.x, G->p.z, G->p.rot, C_PLAYER, C_SKIN, 1.f);
    return X.ntri;
}
