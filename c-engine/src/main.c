#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/engine.h"

static engine_state_t g_state;

static const char *sample_names[] = {
    "kernel_worker", "gc_sweep", "net_daemon", "render_thread",
    "audio_mixer", "db_writer", "cache_evictor", "log_flusher",
    "auth_service", "packet_router"
};

static void seed_workload(engine_state_t *st, int count) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < count; i++) {
        const char *name = sample_names[i % (sizeof(sample_names) / sizeof(sample_names[0]))];
        int priority = rand() % 5;
        int burst = 500 + (rand() % 4000);
        int pages = 1 + (rand() % 6);
        engine_spawn_process(st, name, priority, burst, pages);
    }
}

int main(int argc, char **argv) {
    int port = ENGINE_PORT;
    int seed_count = 6;

    if (argc > 1) port = atoi(argv[1]);
    if (argc > 2) seed_count = atoi(argv[2]);

    engine_init(&g_state);
    seed_workload(&g_state, seed_count);

    fprintf(stderr, "[engine] process simulation core starting (pid table cap=%d)\n", MAX_PROCESSES);
    fprintf(stderr, "[engine] seeded %d processes, algo=ROUND_ROBIN\n", seed_count);

    return server_start(&g_state, port);
}
