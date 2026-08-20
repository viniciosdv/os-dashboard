#include <string.h>
#include <stdlib.h>
#include "../include/engine.h"

void mem_init(engine_state_t *st, int total_frames) {
    st->total_frames = total_frames;
    st->free_frames = total_frames;
    for (int i = 0; i < total_frames; i++) {
        st->frames[i].frame_id = i;
        st->frames[i].owner_pid = -1;
        st->frames[i].dirty = 0;
    }
}

/* Evicts one frame belonging to any process other than `p`, using a simple
 * clock-hand / FIFO-ish policy: first free-looking frame found. Returns the
 * evicted frame index, or -1 if nothing could be reclaimed. */
static int evict_one_frame(engine_state_t *st, process_t *p) {
    for (int i = 0; i < st->total_frames; i++) {
        if (st->frames[i].owner_pid != -1 && st->frames[i].owner_pid != p->pid) {
            int victim_pid = st->frames[i].owner_pid;
            /* remove frame from victim's owned list */
            for (int j = 0; j < st->process_count; j++) {
                process_t *victim = &st->processes[j];
                if (victim->pid == victim_pid) {
                    for (int k = 0; k < victim->frame_count; k++) {
                        if (victim->frames_owned[k] == i) {
                            victim->frames_owned[k] = victim->frames_owned[--victim->frame_count];
                            break;
                        }
                    }
                    break;
                }
            }
            st->frames[i].owner_pid = -1;
            st->frames[i].dirty = 0;
            st->free_frames++;
            return i;
        }
    }
    return -1;
}

int mem_allocate(engine_state_t *st, process_t *p, int pages) {
    int allocated = 0;
    for (int n = 0; n < pages; n++) {
        int frame_idx = -1;
        if (st->free_frames > 0) {
            for (int i = 0; i < st->total_frames; i++) {
                if (st->frames[i].owner_pid == -1) { frame_idx = i; break; }
            }
        } else {
            frame_idx = evict_one_frame(st, p);
        }
        if (frame_idx == -1) break; /* out of memory entirely */

        st->frames[frame_idx].owner_pid = p->pid;
        st->free_frames--;
        p->frames_owned[p->frame_count++] = frame_idx;
        allocated++;
    }
    return allocated;
}

void mem_free_process(engine_state_t *st, process_t *p) {
    for (int i = 0; i < p->frame_count; i++) {
        int fidx = p->frames_owned[i];
        st->frames[fidx].owner_pid = -1;
        st->frames[fidx].dirty = 0;
        st->free_frames++;
    }
    p->frame_count = 0;
}

/* Simulates a memory access for the running process. With a small
 * probability the "page" touched isn't resident (page fault), forcing a
 * fresh allocation (and possibly an eviction). Deterministic-ish via rand(). */
int mem_handle_access(engine_state_t *st, process_t *p) {
    if (p->frame_count == 0) return 0;

    int fault_roll = rand() % 100;
    int fault_threshold = 8; /* ~8% chance per tick */

    if (fault_roll < fault_threshold) {
        p->page_faults++;
        st->total_page_faults++;
        /* simulate: drop one owned frame and re-fault it in */
        int idx = rand() % p->frame_count;
        int fidx = p->frames_owned[idx];
        st->frames[fidx].owner_pid = -1;
        st->free_frames++;
        p->frames_owned[idx] = p->frames_owned[--p->frame_count];
        mem_allocate(st, p, 1);
        return 1;
    }
    return 0;
}
