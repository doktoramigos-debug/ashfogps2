#pragma once
#include "game.h"
typedef struct { float x, y, z; uint8_t r, g, b; } Vtx;           /* x,y - пиксели, z - глубина 0..1 */
typedef void (*TriFn)(void *ud, const Vtx *a, const Vtx *b, const Vtx *c);
#define SCR_W 640
#define SCR_H 448
/* Строит кадр: вызывает emit для каждого треугольника. Возвращает число треугольников. */
int render_scene(const Game *G, TriFn emit, void *ud, uint8_t sky[3]);
