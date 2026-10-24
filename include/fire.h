#ifndef FIRE_H
#define FIRE_H

#include "world.h"

int  fire_burn_ticks(const World *w, int r, int c);
void fire_spread_worker(World *w, int idx_lo, int idx_hi, unsigned int *rng);

#endif