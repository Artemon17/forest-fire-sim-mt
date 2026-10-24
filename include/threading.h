#ifndef THREADING_H
#define THREADING_H

#include "world.h"

void *sensor_thread_fn(void *arg);
void *team_thread_fn(void *arg);
void *fire_worker_fn(void *arg);

int  world_start_threads(World *w);
void world_stop_threads(World *w);

#endif