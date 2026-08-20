#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <pthread.h>
#include <time.h>

#define MAX_PROCESSES     64
#define MAX_FRAMES        256
#define FRAME_SIZE_KB     4
#define TIME_QUANTUM_MS   50
#define TICK_INTERVAL_MS  100
#define ENGINE_PORT       5051
#define MAX_NAME_LEN      32
#define MAX_LOG_LINE      256

typedef enum {
    PROC_NEW = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_WAITING,
    PROC_TERMINATED
} process_state_t;

typedef enum {
    SCHED_ROUND_ROBIN = 0,
    SCHED_PRIORITY,
    SCHED_SJF
} sched_algo_t;

typedef struct {
    int pid;
    char name[MAX_NAME_LEN];
    process_state_t state;
    int priority;              /* 0 = highest */
    int burst_total_ms;        /* total CPU time required */
    int burst_remaining_ms;    /* remaining CPU time */
    int pages_requested;       /* number of memory pages needed */
    int frames_owned[MAX_FRAMES];
    int frame_count;
    int page_faults;
    int context_switches;
    long wait_time_ms;
    long turnaround_ms;
    struct timespec created_at;
    struct timespec finished_at;
} process_t;

typedef struct {
    int frame_id;
    int owner_pid;   /* -1 if free */
    int dirty;
} frame_t;

typedef struct {
    /* process table */
    process_t processes[MAX_PROCESSES];
    int process_count;
    int next_pid;

    /* ready queue (indices into processes[], circular buffer) */
    int ready_queue[MAX_PROCESSES];
    int rq_head, rq_tail, rq_size;

    /* memory */
    frame_t frames[MAX_FRAMES];
    int total_frames;
    int free_frames;
    long total_page_faults;

    /* scheduler stats */
    sched_algo_t algo;
    long total_context_switches;
    long total_ticks;
    int running_pid; /* -1 if idle */
    double cpu_utilization; /* rolling percentage */

    pthread_mutex_t lock;
} engine_state_t;

/* engine_core.c */
void engine_init(engine_state_t *st);
int  engine_spawn_process(engine_state_t *st, const char *name, int priority,
                           int burst_ms, int pages);
void engine_kill_process(engine_state_t *st, int pid);
void engine_tick(engine_state_t *st);
void engine_set_algo(engine_state_t *st, sched_algo_t algo);

/* memory.c */
void mem_init(engine_state_t *st, int total_frames);
int  mem_allocate(engine_state_t *st, process_t *p, int pages);
void mem_free_process(engine_state_t *st, process_t *p);
int  mem_handle_access(engine_state_t *st, process_t *p);

/* metrics.c */
int  metrics_to_json(engine_state_t *st, char *buf, size_t buflen);

/* socket_server.c */
int  server_start(engine_state_t *st, int port);

#endif /* ENGINE_H */
