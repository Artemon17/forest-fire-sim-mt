#ifndef MESSAGES_H
#define MESSAGES_H

#include "world.h"

void messages_send(World *w, int sensor_idx, int r, int c, unsigned int *rng);
void messages_send_team(World *w, int team_id, int r, int c, unsigned int *rng);
void messages_tick(World *w);

#endif