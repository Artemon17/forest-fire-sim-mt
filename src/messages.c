#define _POSIX_C_SOURCE 200809L

#include "messages.h"

#include <stdlib.h>
#include <string.h>

static void queue_push(MessageQueue *q, const Message *m) {
    pthread_mutex_lock(&q->lock);
    if (q->count >= q->capacity) {
        int ncap = q->capacity ? q->capacity * 2 : 16;
        Message *ni = realloc(q->items, (size_t)ncap * sizeof(Message));
        if (!ni) { pthread_mutex_unlock(&q->lock); return; }
        q->items = ni;
        q->capacity = ncap;
    }
    q->items[q->count++] = *m;
    pthread_mutex_unlock(&q->lock);
}

static void queue_remove_at(MessageQueue *q, int idx) {
    pthread_mutex_lock(&q->lock);
    if (idx >= 0 && idx < q->count) {
        if (idx < q->count - 1)
            memmove(&q->items[idx], &q->items[idx + 1],
                    (size_t)(q->count - idx - 1) * sizeof(Message));
        q->count--;
    }
    pthread_mutex_unlock(&q->lock);
}

static int seen_has(const World *w, int id) {
    for (int i = 0; i < w->seen_count; i++)
        if (w->seen_ids[i] == id) return 1;
    return 0;
}

static int seen_add(World *w, int id) {
    if (w->seen_count >= w->seen_cap) {
        int ncap = w->seen_cap ? w->seen_cap * 2 : 32;
        int *ni = realloc(w->seen_ids, (size_t)ncap * sizeof(int));
        if (!ni) return -1;
        w->seen_ids = ni;
        w->seen_cap = ncap;
    }
    w->seen_ids[w->seen_count++] = id;
    return 0;
}

static double rnd01(unsigned int *rng) {
    return (double)rand_r(rng) / ((double)RAND_MAX + 1.0);
}

static int rnd_range(unsigned int *rng, int lo, int hi) {
    if (hi < lo) { int t = lo; lo = hi; hi = t; }
    return lo + rand_r(rng) % (hi - lo + 1);
}

void messages_send(World *w, int sensor_idx, int r, int c, unsigned int *rng) {
    const Config *cfg = w->cfg;
    const Sensor *s   = &w->sensors[sensor_idx];

    if (rnd01(rng) < cfg->msg_loss_prob) {
        log_add(&w->log, w->tick,
                "Сообщение от датчика #%d потеряно в канале", s->id);
        return;
    }

    int delay = rnd_range(rng, cfg->msg_delay_min, cfg->msg_delay_max);

    /* next_message_id инкрементируется в фазе 1 из N потоков —
     * защищаем через teams_lock как «общий» мьютекс мира,
     * либо можно вынести отдельный id_lock. Здесь используем teams_lock. */
    pthread_mutex_lock(&w->teams_lock);
    Message m;
    m.id            = w->next_message_id++;
    pthread_mutex_unlock(&w->teams_lock);

    m.sensor_id     = s->id;
    m.origin_r      = r;
    m.origin_c      = c;
    m.created_tick  = w->tick;
    m.delivery_tick = w->tick + delay;
    m.is_duplicate  = 0;

    queue_push(&w->in_flight, &m);

    log_add(&w->log, w->tick,
            "Датчик #%d шлёт сообщение #%d о (%d,%d), задержка %d",
            s->id, m.id, r + 1, c + 1, delay);

    if (rnd01(rng) < cfg->msg_dup_prob) {
        Message dup = m;
        dup.is_duplicate = 1;
        queue_push(&w->in_flight, &dup);
        log_add(&w->log, w->tick,
                "Сообщение #%d продублировано в канале", m.id);
    }
}

void messages_send_team(World *w, int team_id, int r, int c, unsigned int *rng) {
    const Config *cfg = w->cfg;

    if (rnd01(rng) < cfg->msg_loss_prob) {
        log_add(&w->log, w->tick,
                "Сообщение от группы #%d потеряно в канале", team_id);
        return;
    }

    int delay = rnd_range(rng, cfg->msg_delay_min, cfg->msg_delay_max);

    pthread_mutex_lock(&w->teams_lock);
    Message m;
    m.id            = w->next_message_id++;
    pthread_mutex_unlock(&w->teams_lock);

    m.sensor_id     = 100 + team_id;
    m.origin_r      = r;
    m.origin_c      = c;
    m.created_tick  = w->tick;
    m.delivery_tick = w->tick + delay;
    m.is_duplicate  = 0;

    queue_push(&w->in_flight, &m);

    log_add(&w->log, w->tick,
            "Группа #%d шлёт сообщение #%d о (%d,%d), задержка %d",
            team_id, m.id, r + 1, c + 1, delay);
}

void messages_tick(World *w) {
    /* все сенсоры уже отстрелялись (после барьера) — пишем in_flight безопасно */
    int i = 0;
    while (i < w->in_flight.count) {
        Message *m = &w->in_flight.items[i];
        if (m->delivery_tick > w->tick) { i++; continue; }

        Message copy = *m;

        if (seen_has(w, copy.id)) {
            log_add(&w->log, w->tick,
                    "Дубликат сообщения #%d отброшен", copy.id);
        } else {
            seen_add(w, copy.id);
            queue_push(&w->inbox, &copy);
            log_add(&w->log, w->tick,
                    "Сообщение #%d доставлено в центр: (%d,%d)",
                    copy.id, copy.origin_r + 1, copy.origin_c + 1);
        }
        queue_remove_at(&w->in_flight, i);
    }
}