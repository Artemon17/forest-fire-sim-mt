#define _POSIX_C_SOURCE 200809L

#include "threading.h"
#include "sensors.h"
#include "teams.h"
#include "fire.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

void *sensor_thread_fn(void *arg) {
    struct WorkerArg *a = arg;
    World *w = a->w;
    while (1) {
        pthread_barrier_wait(&w->barrier_sensors);
        if (atomic_load(&w->stop)) break;
        sensors_observe_one(w, a->thread_id, &a->rng_state);
        pthread_barrier_wait(&w->barrier_sensors);
    }
    return NULL;
}

void *team_thread_fn(void *arg) {
    struct WorkerArg *a = arg;
    World *w = a->w;
    while (1) {
        pthread_barrier_wait(&w->barrier_teams);
        if (atomic_load(&w->stop)) break;

        pthread_mutex_lock(&w->teams_lock);
        teams_move_one(w, a->thread_id);
        teams_extinguish_one(w, a->thread_id);
        pthread_mutex_unlock(&w->teams_lock);

        pthread_barrier_wait(&w->barrier_teams);
    }
    return NULL;
}

void *fire_worker_fn(void *arg) {
    struct WorkerArg *a = arg;
    World *w = a->w;
    int total = w->map->rows * w->map->cols;
    int W = w->num_fire_workers;
    int slice = (total + W - 1) / W;
    int lo = a->thread_id * slice;
    int hi = lo + slice;
    if (hi > total) hi = total;

    while (1) {
        pthread_barrier_wait(&w->barrier_fire);
        if (atomic_load(&w->stop)) break;
        fire_spread_worker(w, lo, hi, &a->rng_state);
        pthread_barrier_wait(&w->barrier_fire);
    }
    return NULL;
}

static unsigned int seed_for(unsigned int base, int tag) {
    return base ^ (0x9E3779B9u * (unsigned)(tag + 1));
}

int world_start_threads(World *w) {
    for (int i = 0; i < w->num_sensor_threads; i++) {
        w->sensor_args[i].w = w;
        w->sensor_args[i].thread_id = i;
        w->sensor_args[i].rng_state = seed_for(w->seed, 1000 + i);
        if (pthread_create(&w->sensor_tids[i], NULL,
                           sensor_thread_fn, &w->sensor_args[i]) != 0) {
            fprintf(stderr, "Не удалось запустить сенсорный поток %d\n", i);
            exit(6);
        }
    }
    for (int i = 0; i < w->num_team_threads; i++) {
        w->team_args[i].w = w;
        w->team_args[i].thread_id = i;
        w->team_args[i].rng_state = seed_for(w->seed, 2000 + i);
        if (pthread_create(&w->team_tids[i], NULL,
                           team_thread_fn, &w->team_args[i]) != 0) {
            fprintf(stderr, "Не удалось запустить поток команды %d\n", i);
            exit(6);
        }
    }
    for (int i = 0; i < w->num_fire_workers; i++) {
        w->fire_args[i].w = w;
        w->fire_args[i].thread_id = i;
        w->fire_args[i].rng_state = seed_for(w->seed, 3000 + i);
        if (pthread_create(&w->fire_tids[i], NULL,
                           fire_worker_fn, &w->fire_args[i]) != 0) {
            fprintf(stderr, "Не удалось запустить fire-воркер %d\n", i);
            exit(6);
        }
    }
    return 0;
}

void world_stop_threads(World *w) {
    atomic_store(&w->stop, 1);

    /* один круг барьеров, чтобы отпустить всех */
    pthread_barrier_wait(&w->barrier_sensors);
    pthread_barrier_wait(&w->barrier_teams);
    pthread_barrier_wait(&w->barrier_fire);

    for (int i = 0; i < w->num_sensor_threads; i++)
        pthread_join(w->sensor_tids[i], NULL);
    for (int i = 0; i < w->num_team_threads; i++)
        pthread_join(w->team_tids[i], NULL);
    for (int i = 0; i < w->num_fire_workers; i++)
        pthread_join(w->fire_tids[i], NULL);
}