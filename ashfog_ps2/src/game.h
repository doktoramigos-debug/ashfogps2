#pragma once
/* ASHFOG PS2 - портативное ядро (без зависимостей от PS2). */
#include <math.h>
#include <string.h>
#include <stdint.h>

#define CELL 2.4f
#define MAX_ENEMIES 48
#define MAX_ITEMS 48

typedef struct { int c, r; const char *id; } ItemSpawn;
typedef struct { char ch; const char *key; } LockDef;
typedef struct { char ch; const char *next; const char *key; } ExitDef;
typedef struct {
    const char *name; int W, H; const char *const *grid;
    const ItemSpawn *items; const LockDef *locks; const ExitDef *exits;
    float h; int dark; uint8_t fog[3]; float fog_near, fog_far; const char *const *indoor;
} LevelDef;

typedef struct { float x, z, rot; int hp; } Player;
typedef struct { char kind; float x, z, rot; int hp; int alive; int chase; float cd, stun; } Enemy;
typedef struct { float x, z; const char *id; int taken; } Item;

typedef struct {
    const LevelDef *lv; char g[64][64];   /* изменяемая копия карты */
    Player p; Enemy en[MAX_ENEMIES]; int nen; Item it[MAX_ITEMS]; int nit;
    int keys[16]; const char *keynames[16]; int nkeys; int hp_items;
    const char *msg; float msg_t; int level_done; const char *next_level; int dead;
} Game;

typedef struct { float fwd, turn; int run, act, heal; } Input;

const LevelDef *find_level(const char *name);
void game_load(Game *G, const LevelDef *lv);
void game_update(Game *G, const Input *in, float dt);
int  cell_solid(const Game *G, int c, int r);
int  game_has_key(const Game *G, const char *k);
