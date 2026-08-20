#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/engine.h"

static void rq_push(engine_state_t *st, int idx) {
    if (st->rq_size >= MAX_PROCESSES) return;
    st->ready_queue[st->rq_tail] = idx;
    st->rq_tail = (st->rq_tail + 1) % MAX_PROCESSES;
    st->rq_size++;
}

static int rq_pop(engine_state_t *st) {
    if (st->rq_size == 0) return -1;
    int idx = st->ready_queue[st->rq_head];
    st->rq_head = (st->rq_head + 1) % MAX_PROCESSES;
    st->rq_size--;
    return idx;
}

/* Removes an arbitrary process index from the ready queue (used when killing) */
static void rq_remove(engine_state_t *st, int idx) {
    int tmp[MAX_PROCESSES];
    int n = 0;
    while (st->rq_size > 0) {
        int v = rq_pop(st);
        if (v != idx) tmp[n++] = v;
    }
    for (int i = 0; i < n; i++) rq_push(st, tmp[i]);
}

void engine_init(engine_state_t *st) {
    memset(st, 0, sizeof(*st));
    st->next_pid = 1;
    st->running_pid = -1;
    st->algo = SCHED_ROUND_ROBIN;
    pthread_mutex_init(&st->lock, NULL);
    mem_init(st, MAX_FRAMES);
}

void engine_set_algo(engine_state_t *st, sched_algo_t algo) {
    pthread_mutex_lock(&st->lock);
    st->algo = algo;
    pthread_mutex_unlock(&st->lock);
}

static process_t *find_by_pid(engine_state_t *st, int pid, int *out_idx) {
    for (int i = 0; i < st->process_count; i++) {
        if (st->processes[i].pid == pid && st->processes[i].state != PROC_TERMINATED) {
            if (out_idx) *out_idx = i;
            return &st->processes[i];
        }
    }
    return NULL;
}

int engine_spawn_process(engine_state_t *st, const char *name, int priority,
                          int burst_ms, int pages) {
    pthread_mutex_lock(&st->lock);

    if (st->process_count >= MAX_PROCESSES) {
        pthread_mutex_unlock(&st->lock);
        return -1;
    }

    int idx = st->process_count++;
    process_t *p = &st->processes[idx];
    memset(p, 0, sizeof(*p));
    p->pid = st->next_pid++;
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->priority = priority;
    p->burst_total_ms = burst_ms;
    p->burst_remaining_ms = burst_ms;
    p->pages_requested = pages;
    p->state = PROC_NEW;
    clock_gettime(CLOCK_MONOTONIC, &p->created_at);

    /* Attempt memory allocation up front; page faults are resolved lazily on access */
    mem_allocate(st, p, pages);

    p->state = PROC_READY;
    rq_push(st, idx);

    int pid = p->pid;
    pthread_mutex_unlock(&st->lock);
    return pid;
}

void engine_kill_process(engine_state_t *st, int pid) {
    pthread_mutex_lock(&st->lock);
    int idx;
    process_t *p = find_by_pid(st, pid, &idx);
    if (p) {
        rq_remove(st, idx);
        mem_free_process(st, p);
        p->state = PROC_TERMINATED;
        clock_gettime(CLOCK_MONOTONIC, &p->finished_at);
        if (st->running_pid == pid) st->running_pid = -1;
    }
    pthread_mutex_unlock(&st->lock);
}

/* Selects next ready-queue slot to run according to the active algorithm.
 * For SJF/Priority we scan the queue rather than pop blindly, since those
 * algorithms are not strictly FIFO. */
static int select_next(engine_state_t *st) {
    if (st->rq_size == 0) return -1;

    if (st->algo == SCHED_ROUND_ROBIN) {
        return rq_pop(st);
    }

    /* Priority / SJF: linear scan the circular buffer for the best candidate */
    int best_pos = -1, best_idx = -1;
    for (int i = 0, pos = st->rq_head; i < st->rq_size; i++, pos = (pos + 1) % MAX_PROCESSES) {
        int idx = st->ready_queue[pos];
        process_t *p = &st->processes[idx];
        if (best_idx == -1) { best_idx = idx; best_pos = pos; continue; }
        process_t *best = &st->processes[best_idx];
        if (st->algo == SCHED_PRIORITY && p->priority < best->priority) {
            best_idx = idx; best_pos = pos;
        } else if (st->algo == SCHED_SJF && p->burst_remaining_ms < best->burst_remaining_ms) {
            best_idx = idx; best_pos = pos;
        }
    }
    if (best_idx != -1) rq_remove(st, best_idx);
    (void)best_pos;
    return best_idx;
}

/* One scheduler tick == one TIME_QUANTUM_MS slice (Round Robin) or a
 * proportional slice for the other algorithms. Called periodically by the
 * server loop thread. */
void engine_tick(engine_state_t *st) {
    pthread_mutex_lock(&st->lock);
    st->total_ticks++;

    if (st->running_pid == -1) {
        int idx = select_next(st);
        if (idx >= 0) {
            st->processes[idx].state = PROC_RUNNING;
            st->running_pid = st->processes[idx].pid;
        }
    }

    if (st->running_pid != -1) {
        int idx;
        process_t *p = find_by_pid(st, st->running_pid, &idx);
        if (p) {
            /* simulate a memory access each tick -> may trigger a page fault */
            mem_handle_access(st, p);

            int slice = (st->algo == SCHED_ROUND_ROBIN) ? TIME_QUANTUM_MS : TICK_INTERVAL_MS;
            int consumed = slice < p->burst_remaining_ms ? slice : p->burst_remaining_ms;
            p->burst_remaining_ms -= consumed;

            /* every other ready process accrues wait time */
            for (int i = 0, pos = st->rq_head; i < st->rq_size; i++, pos = (pos + 1) % MAX_PROCESSES) {
                st->processes[st->ready_queue[pos]].wait_time_ms += TICK_INTERVAL_MS;
            }

            if (p->burst_remaining_ms <= 0) {
                p->state = PROC_TERMINATED;
                clock_gettime(CLOCK_MONOTONIC, &p->finished_at);
                p->turnaround_ms = (long)((p->finished_at.tv_sec - p->created_at.tv_sec) * 1000 +
                                           (p->finished_at.tv_nsec - p->created_at.tv_nsec) / 1000000);
                mem_free_process(st, p);
                st->running_pid = -1;
                st->total_context_switches++;
                p->context_switches++;
            } else if (st->algo == SCHED_ROUND_ROBIN) {
                /* preempt: back of the queue */
                p->state = PROC_READY;
                p->context_switches++;
                rq_push(st, idx);
                st->running_pid = -1;
                st->total_context_switches++;
            }
            /* Priority/SJF: keep running until finished (non-preemptive) */
        }
    }

    /* rolling CPU utilization: fraction of ticks where a process was running */
    double running_flag = (st->running_pid != -1) ? 1.0 : 0.0;
    st->cpu_utilization = st->cpu_utilization * 0.9 + running_flag * 100.0 * 0.1;

    pthread_mutex_unlock(&st->lock);
}
