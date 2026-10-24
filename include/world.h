#ifndef WORLD_H
#define WORLD_H

#include "config.h"
#include "map.h"
#include "log.h"

#include <pthread.h>
#include <stdatomic.h>

#define MAX_SENSOR_THREADS 16
#define MAX_TEAM_THREADS   16
#define MAX_FIRE_WORKERS   8

typedef enum { ST_SAFE = 0, ST_BURNING, ST_EXTINGUISHED, ST_BURNT } FireState;

typedef enum { OCC_NONE = 0, OCC_SENSOR, OCC_STATION, OCC_TEAM } Occupant;

typedef struct { int id; int r, c; } Sensor;

typedef enum {
    TEAM_IDLE = 0, TEAM_MOVING, TEAM_EXTINGUISHING, TEAM_RETURNING
} TeamState;

typedef struct {
    int       id;
    int       r, c;
    TeamState state;
    int       target_r, target_c;
    int       target_task_id;
    double    progress;
    int       last_report_r;
    int       last_report_c;
} FireTeam;

typedef enum {
    TASK_PENDING = 0, TASK_ASSIGNED, TASK_IN_PROGRESS,
    TASK_DONE, TASK_CANCELLED
} TaskState;

typedef struct {
    int       id;
    int       r, c;
    TaskState state;
    int       assigned_team_id;
    int       created_tick;
    int       completed_tick;
} Task;

typedef struct { Task *items; int count; int capacity; } TaskList;

typedef struct {
    int id;
    int sensor_id;
    int origin_r, origin_c;
    int created_tick;
    int delivery_tick;
    int is_duplicate;
} Message;

typedef struct {
    Message        *items;
    int             count;
    int             capacity;
    pthread_mutex_t lock;
} MessageQueue;

typedef struct World {
    const MapData *map;
    const Config  *cfg;

    int          st_r, st_c;
    unsigned int seed;
    unsigned int main_rng;

    FireState *state;
    Occupant  *occupant;
    int       *team_id;
    int       *burn_age;

    Sensor   *sensors;
    int       sensors_count;

    FireTeam *teams;
    int       teams_count;

    int          next_message_id;
    MessageQueue in_flight;
    MessageQueue inbox;
    int         *seen_ids;
    int          seen_count;
    int          seen_cap;
    unsigned char *sensor_reported;

    TaskList tasks;
    int      next_task_id;

    EventLog log;
    int tick;

    /* ── многопоточность ── */
    pthread_barrier_t barrier_sensors;
    pthread_barrier_t barrier_teams;
    pthread_barrier_t barrier_fire;

    pthread_mutex_t   teams_lock;

    atomic_int        stop;

    _Atomic FireState *next_state;

    int num_sensor_threads;
    int num_team_threads;
    int num_fire_workers;

    pthread_t sensor_tids[MAX_SENSOR_THREADS];
    pthread_t team_tids[MAX_TEAM_THREADS];
    pthread_t fire_tids[MAX_FIRE_WORKERS];

    struct WorkerArg {
        struct World *w;
        int           thread_id;
        unsigned int  rng_state;
    } sensor_args[MAX_SENSOR_THREADS],
      team_args[MAX_TEAM_THREADS],
      fire_args[MAX_FIRE_WORKERS];
} World;

typedef struct { char terrain; char state; char occupant; } CellView;

int  world_init(World *w, const MapData *map, const Config *cfg,
                int st_r, int st_c, unsigned int seed);
void world_free(World *w);
CellView world_cell_view(const World *w, int r, int c);
char     world_display_symbol(const CellView *cv);

#endif