#include <stdio.h>
#include "game.h"
int main() {
    Game G; memset(&G, 0, sizeof G);
    game_load(&G, find_level("hospital"));
    printf("start %.1f %.1f enemies=%d items=%d hp=%d\n", G.p.x, G.p.z, G.nen, G.nit, G.p.hp);
    Input in = {1, 0, 0, 0, 0};
    for (int i = 0; i < 300; i++) game_update(&G, &in, 1.f / 60);
    printf("after 5s forward: %.2f %.2f (walls stop?)\n", G.p.x, G.p.z);
    G.p.rot = 90; for (int i = 0; i < 600; i++) game_update(&G, &in, 1.f / 60);
    printf("east 10s: %.2f %.2f\n", G.p.x, G.p.z);
    return 0;
}
