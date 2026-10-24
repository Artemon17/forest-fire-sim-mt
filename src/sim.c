#define _POSIX_C_SOURCE 200809L

#include "sim.h"
#include "sensors.h"
#include "messages.h"
#include "center.h"
#include "teams.h"
#include "fire.h"
#include "threading.h"

#include <stdatomic.h>
#include <pthread.h>

static void close_pending_tasks(World *w) {
    for (int i = 0; i < w->tasks.count; i++) {
        Task *t = &w->tasks.items[i];
        if (t->state == TASK_PENDING ||
            t->state == TASK_ASSIGNED ||
            t->state == TASK_IN_PROGRESS) {
            t->state = TASK_CANCELLED;
            t->completed_tick = w->tick;
            }
    }
    for (int i = 0; i < w->teams_count; i++) {
        FireTeam *t = &w->teams[i];
        t->target_task_id = 0;
        t->target_r = -1;
        t->target_c = -1;
        if (t->state != TEAM_IDLE)
            t->state = TEAM_IDLE;
    }
}

static int count_burning(const World *w) {
    int n = w->map->rows * w->map->cols;
    int c = 0;
    for (int i = 0; i < n; i++)
        if (w->state[i] == ST_BURNING) c++;
    return c;
}

static int count_flammable_safe(const World *w) {
    int n = w->map->rows * w->map->cols;
    int c = 0;
    for (int i = 0; i < n; i++) {
        Terrain t = w->map->cells[i];
        if (t == TERRAIN_WATER || t == TERRAIN_FIREBREAK) continue;
        FireState s = w->state[i];
        if (s == ST_SAFE || s == ST_EXTINGUISHED) c++;
    }
    return c;
}

SimResult sim_finished(const World *w) {
    if (count_burning(w) == 0)        return RESULT_EXTINGUISHED;
    if (count_flammable_safe(w) == 0) return RESULT_TERRITORY_EXHAUSTED;
    if (!w->cfg->end_unlimited && w->tick >= w->cfg->end_max_ticks)
        return RESULT_TICK_LIMIT;
    return RESULT_RUNNING;
}

void sim_tick(World *w) {
    w->tick++;

    /* Фаза 1: датчики (N потоков) */
    pthread_barrier_wait(&w->barrier_sensors);
    pthread_barrier_wait(&w->barrier_sensors);

    /* Группы как датчики — в main */
    teams_observe(w, &w->main_rng);

    /* Фаза 2: сообщения (main) */
    messages_tick(w);

    /* Фаза 3: центр (main) */
    center_tick(w);

    /* Фаза 4–5: команды (K потоков) */
    pthread_barrier_wait(&w->barrier_teams);
    pthread_barrier_wait(&w->barrier_teams);

    /* Фаза 6: огонь (W потоков) */
    int total = w->map->rows * w->map->cols;
    for (int i = 0; i < total; i++)
        atomic_store(&w->next_state[i], w->state[i]);

    pthread_barrier_wait(&w->barrier_fire);
    pthread_barrier_wait(&w->barrier_fire);

    /* Применяем next_state последовательно */
    for (int i = 0; i < total; i++) {
        FireState prev = w->state[i];
        FireState nx   = atomic_load(&w->next_state[i]);
        if (prev == ST_BURNING && nx == ST_BURNT)
            w->burn_age[i] = -1;
        else if (prev == ST_BURNING)
            w->burn_age[i]++;
        else if (prev != ST_BURNING && nx == ST_BURNING)
            w->burn_age[i] = 0;
        w->state[i] = nx;
    }
}

SimResult sim_run(World *w) {
    if (world_start_threads(w) != 0) return RESULT_INTERRUPTED;
    SimResult r = sim_finished(w);
    while (r == RESULT_RUNNING) {
        sim_tick(w);
        r = sim_finished(w);
    }
    world_stop_threads(w);
    close_pending_tasks(w);   
    return r;
}

const char *sim_result_name(SimResult r) {
    switch (r) {
        case RESULT_RUNNING:             return "RUNNING";
        case RESULT_EXTINGUISHED:        return "EXTINGUISHED";
        case RESULT_TERRITORY_EXHAUSTED: return "TERRITORY_EXHAUSTED";
        case RESULT_TICK_LIMIT:          return "TICK_LIMIT";
        case RESULT_INTERRUPTED:         return "INTERRUPTED";
    }
    return "?";
}