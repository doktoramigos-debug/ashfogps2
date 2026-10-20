#include "game.h"
#include "levels_data.h"
#include <stdlib.h>
#include <stdio.h>

static const char SOLID[] = "#DABGWXEbtckmTCZL";
static const struct { char k; int hp, dmg; float sp, cd, sight; } KIND[] = {
    {'w', 30, 7, 1.1f, 1.5f, 9}, {'z', 65, 11, 1.7f, 1.4f, 10}, {'N', 45, 9, 2.7f, 1.0f, 13}, {'Y', 260, 22, 1.7f, 1.5f, 16}};

const LevelDef *find_level(const char *n) {
    for (int i = 0; i < 3; i++) if (!strcmp(ALL_LEVELS[i]->name, n)) return ALL_LEVELS[i];
    return NULL;
}
int cell_solid(const Game *G, int c, int r) {
    if (c < 0 || r < 0 || c >= G->lv->W || r >= G->lv->H) return 1;
    return strchr(SOLID, G->g[r][c]) != NULL;
}
int game_has_key(const Game *G, const char *k) {
    for (int i = 0; i < G->nkeys; i++) if (!strcmp(G->keynames[i], k)) return 1;
    return 0;
}
static int hit(const Game *G, float x, float z, float rad) {
    for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2)
        if (cell_solid(G, (int)floorf((x + a * rad) / CELL + .5f), (int)floorf(-(z + b * rad) / CELL + .5f))) return 1;
    return 0;
}
static void say(Game *G, const char *m, float t) { G->msg = m; G->msg_t = t; }

void game_load(Game *G, const LevelDef *lv) {
    int keep_hp = G->p.hp > 0 ? G->p.hp : 100;
    int nk = G->nkeys; const char *kn[16]; memcpy(kn, G->keynames, sizeof kn);
    memset(G, 0, sizeof *G);
    G->nkeys = nk; memcpy(G->keynames, kn, sizeof kn);
    G->lv = lv; G->p.hp = keep_hp;
    for (int r = 0; r < lv->H; r++) for (int c = 0; c < lv->W; c++) {
        char ch = lv->grid[r][c]; G->g[r][c] = ch;
        float x = c * CELL, z = -r * CELL;
        if (strchr("wzNY", ch) && G->nen < MAX_ENEMIES) {
            Enemy *e = &G->en[G->nen++]; int k = 0;
            for (int i = 0; i < 4; i++) if (KIND[i].k == ch) k = i;
            e->kind = ch; e->x = x; e->z = z; e->hp = KIND[k].hp; e->alive = 1; e->cd = 1; G->g[r][c] = '.';
        } else if (ch == 'P') { G->p.x = x; G->p.z = z; G->g[r][c] = '.'; }
        else if (ch == 'V') G->g[r][c] = '.';
    }
    for (const ItemSpawn *s = lv->items; s->c >= 0 && G->nit < MAX_ITEMS; s++) {
        Item *i = &G->it[G->nit++]; i->x = s->c * CELL; i->z = -s->r * CELL; i->id = s->id;
    }
}

static int is_key(const char *id) { return !strncmp(id, "key_", 4); }

static void use_door(Game *G, int c, int r) {
    char ch = G->g[r][c]; const LevelDef *L = G->lv;
    for (const LockDef *l = L->locks; l->ch; l++) if (l->ch == ch) {
        if (!strcmp(l->key, "never")) { say(G, "Заколочено.", 2); return; }
        if (!game_has_key(G, l->key)) { say(G, "Заперто. Нужен ключ.", 2); return; }
        G->g[r][c] = '.'; say(G, "Открыто.", 1.5f); return;
    }
    for (const ExitDef *e = L->exits; e->ch; e++) if (e->ch == ch) {
        if (!game_has_key(G, e->key)) { say(G, "Не открыть. Нужен ключ.", 2); return; }
        G->level_done = 1; G->next_level = e->next; return;
    }
    if (ch == 'X') return;
    G->g[r][c] = '.'; say(G, "", 0);
}

void game_update(Game *G, const Input *in, float dt) {
    Player *p = &G->p;
    if (G->msg_t > 0) G->msg_t -= dt;
    if (G->dead) return;
    /* танковое управление как в оригинале */
    p->rot += in->turn * 140.f * dt * (in->fwd != 0 ? .6f : 1.f);
    float a = p->rot * (float)M_PI / 180.f;
    if (in->fwd != 0) {
        float sp = in->fwd > 0 ? (in->run ? 4.4f : 2.6f) : -1.7f;
        float dx = sinf(a) * sp * dt, dz = cosf(a) * sp * dt;
        if (!hit(G, p->x + dx, p->z, .4f)) p->x += dx;
        if (!hit(G, p->x, p->z + dz, .4f)) p->z += dz;
    }
    if (in->act) {   /* E: дверь перед игроком или предмет рядом */
        int done = 0;
        for (int i = 0; i < G->nit && !done; i++) {
            Item *t = &G->it[i]; if (t->taken) continue;
            float dx = t->x - p->x, dz = t->z - p->z;
            if (dx * dx + dz * dz < 1.2f * 1.2f) {
                t->taken = 1; done = 1;
                if (is_key(t->id) && G->nkeys < 16) { G->keynames[G->nkeys++] = t->id; say(G, "Ключ подобран.", 2); }
                else if (!strcmp(t->id, "health") || !strcmp(t->id, "medkit")) { G->hp_items++; say(G, "Аптечка.", 2); }
                else say(G, "Подобрано.", 1.5f);
            }
        }
        if (!done) {
            int c = (int)floorf((p->x + sinf(a) * 1.3f) / CELL + .5f), r = (int)floorf(-(p->z + cosf(a) * 1.3f) / CELL + .5f);
            if (strchr("DABGWXE", G->g[r][c])) use_door(G, c, r);
        }
    }
    if (in->heal && G->hp_items > 0 && p->hp < 100) { G->hp_items--; p->hp += 35; if (p->hp > 100) p->hp = 100; }
    /* враги: простое преследование */
    for (int i = 0; i < G->nen; i++) {
        Enemy *e = &G->en[i]; if (!e->alive) continue;
        float dx = p->x - e->x, dz = p->z - e->z, d = sqrtf(dx * dx + dz * dz); int k = 0;
        for (int j = 0; j < 4; j++) if (KIND[j].k == e->kind) k = j;
        if (!e->chase && d < KIND[k].sight) e->chase = 1;
        if (e->chase && d > 1.4f) {
            float nx = e->x + dx / d * KIND[k].sp * dt, nz = e->z + dz / d * KIND[k].sp * dt;
            if (!hit(G, nx, e->z, .45f)) e->x = nx;
            if (!hit(G, e->x, nz, .45f)) e->z = nz;
            e->rot = atan2f(dx, dz) * 180.f / (float)M_PI;
        } else if (e->chase) {
            e->cd -= dt; if (e->cd <= 0) { e->cd = KIND[k].cd; p->hp -= KIND[k].dmg; if (p->hp <= 0) { p->hp = 0; G->dead = 1; } }
        }
    }
}
