#ifndef SIM_H
#define SIM_H

#include "world.h"

typedef enum {
    RESULT_RUNNING = 0,
    RESULT_EXTINGUISHED,
    RESULT_TERRITORY_EXHAUSTED,
    RESULT_TICK_LIMIT,
    RESULT_INTERRUPTED
} SimResult;

void       sim_tick(World *w);
SimResult  sim_finished(const World *w);
SimResult  sim_run(World *w);
const char *sim_result_name(SimResult r);

#endif