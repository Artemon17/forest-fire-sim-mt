#define _POSIX_C_SOURCE 200809L

#include "fire.h"
#include <stdlib.h>
#include <string.h>

static const int DR[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
static const int DC[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };

static double wind_factor_for(const Config *cfg, int wind_dir, int dir) {
    int diff = (dir - wind_dir + 8) % 8;
    double along   = cfg->fire_wind_factor_along;
    double against = cfg->fire_wind_factor_against;
    double side    = cfg->fire_wind_factor_side;
    switch (diff) {
        case 0: return along;
        case 1: return (along + side) / 2.0;
        case 2: return side;
        case 3: return (side + against) / 2.0;
        case 4: return against;
        case 5: return (side + against) / 2.0;
        case 6: return side;
        case 7: return (along + side) / 2.0;
    }
    return side;
}

static double base_prob(const Config *cfg, Terrain t) {
    switch (t) {
        case TERRAIN_GRASS:     return cfg->fire_base_prob_grass;
        case TERRAIN_TREE:      return cfg->fire_base_prob_tree;
        default:                return 0.0;
    }
}

static double burn_factor(const Config *cfg, int age) {
    double f = 1.0 + 0.1 * (double)age;
    (void)cfg;
    if (f > 2.0) f = 2.0;
    return f;
}

static int can_ignite(const World *w, int r, int c) {
    Terrain t = w->map->cells[r * w->map->cols + c];
    if (t == TERRAIN_WATER || t == TERRAIN_FIREBREAK) return 0;
    FireState s = w->state[r * w->map->cols + c];
    if (s == ST_BURNING) return 0;
    if (s == ST_BURNT && !w->cfg->fire_reburn_burnt) return 0;
    return 1;
}

int fire_burn_ticks(const World *w, int r, int c) {
    (void)w; (void)r; (void)c;
    return 6;
}

static double rnd01(unsigned int *rng) {
    return (double)rand_r(rng) / ((double)RAND_MAX + 1.0);
}

void fire_spread_worker(World *w, int idx_lo, int idx_hi, unsigned int *rng) {
    int R = w->map->rows, C = w->map->cols;
    int use8 = w->cfg->fire_spread_8dir;
    int n_dirs = use8 ? 8 : 4;
    static const int D4[4] = { 0, 2, 4, 6 };

    for (int i = idx_lo; i < idx_hi; i++) {
        if (w->state[i] != ST_BURNING) continue;
        int r = i / C, c = i % C;
        int age = w->burn_age[i];
        double bf = burn_factor(w->cfg, age);

        for (int k = 0; k < n_dirs; k++) {
            int d = use8 ? k : D4[k];
            int nr = r + DR[d], nc = c + DC[d];
            if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
            if (!can_ignite(w, nr, nc)) continue;

            double p = base_prob(w->cfg, w->map->cells[nr * C + nc]);
            p *= wind_factor_for(w->cfg, (int)w->cfg->wind_direction, d);
            p *= bf;
            if (p <= 0.0) continue;
            if (p > 1.0) p = 1.0;

            if (rnd01(rng) < p)
                atomic_store(&w->next_state[nr * C + nc], ST_BURNING);
        }

        if (age + 1 >= fire_burn_ticks(w, r, c))
            atomic_store(&w->next_state[i], ST_BURNT);
    }
}