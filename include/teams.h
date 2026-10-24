#ifndef TEAMS_H
#define TEAMS_H

#include "world.h"

void teams_observe(World *w, unsigned int *rng);
void teams_move_one(World *w, int team_idx);
void teams_extinguish_one(World *w, int team_idx);

#endif