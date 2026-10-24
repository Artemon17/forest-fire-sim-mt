#ifndef SENSORS_H
#define SENSORS_H

#include "world.h"

void sensors_observe_one(World *w, int sensor_idx, unsigned int *rng);
int  sensor_burning_in_view(const World *w, int sensor_idx);

#endif