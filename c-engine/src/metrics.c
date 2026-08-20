#include <stdio.h>
#include <string.h>
#include "../include/engine.h"

static const char *state_name(process_state_t s) {
    switch (s) {
        case PROC_NEW: return "NEW";
        case PROC_READY: return "READY";
        case PROC_RUNNING: return "RUNNING";
        case PROC_WAITING: return "WAITING";
        case PROC_TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

static const char *algo_name(sched_algo_t a) {
    switch (a) {
        case SCHED_ROUND_ROBIN: return "ROUND_ROBIN";
        case SCHED_PRIORITY: return "PRIORITY";
        case SCHED_SJF: return "SJF";
    }
    return "UNKNOWN";
}

/* Hand-rolled JSON writer (no external deps, keeps the C engine dependency-free).
 * Produces a single-line JSON object terminated by '\n', which the Java
 * backend reads as newline-delimited JSON over the TCP socket. */
int metrics_to_json(engine_state_t *st, char *buf, size_t buflen) {
    size_t off = 0;
    int n;

    n = snprintf(buf + off, buflen - off,
        "{\"type\":\"metrics\",\"algo\":\"%s\",\"tick\":%ld,"
        "\"cpuUtilization\":%.2f,\"contextSwitches\":%ld,"
        "\"pageFaults\":%ld,\"freeFrames\":%d,\"totalFrames\":%d,"
        "\"runningPid\":%d,\"readyQueueSize\":%d,\"processes\":[",
        algo_name(st->algo), st->total_ticks, st->cpu_utilization,
        st->total_context_switches, st->total_page_faults,
        st->free_frames, st->total_frames, st->running_pid, st->rq_size);
    if (n < 0 || (size_t)n >= buflen - off) return -1;
    off += n;

    int first = 1;
    for (int i = 0; i < st->process_count; i++) {
        process_t *p = &st->processes[i];
        if (p->state == PROC_TERMINATED) continue; /* keep the feed lean */

        n = snprintf(buf + off, buflen - off,
            "%s{\"pid\":%d,\"name\":\"%s\",\"state\":\"%s\",\"priority\":%d,"
            "\"burstTotalMs\":%d,\"burstRemainingMs\":%d,\"framesOwned\":%d,"
            "\"pageFaults\":%d,\"contextSwitches\":%d,\"waitTimeMs\":%ld}",
            first ? "" : ",", p->pid, p->name, state_name(p->state), p->priority,
            p->burst_total_ms, p->burst_remaining_ms, p->frame_count,
            p->page_faults, p->context_switches, p->wait_time_ms);
        if (n < 0 || (size_t)n >= buflen - off) return -1;
        off += n;
        first = 0;
    }

    n = snprintf(buf + off, buflen - off, "]}\n");
    if (n < 0 || (size_t)n >= buflen - off) return -1;
    off += n;

    return (int)off;
}
