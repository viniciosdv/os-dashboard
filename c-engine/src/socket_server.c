#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "../include/engine.h"

#define MAX_CLIENTS   16
#define METRICS_BUF   16384

typedef struct {
    engine_state_t *st;
    int client_fds[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t clients_lock;
} server_ctx_t;

static server_ctx_t g_ctx;

static void broadcast(const char *msg, int len) {
    pthread_mutex_lock(&g_ctx.clients_lock);
    for (int i = 0; i < g_ctx.client_count; i++) {
        int fd = g_ctx.client_fds[i];
        if (fd >= 0) {
            ssize_t sent = write(fd, msg, len);
            if (sent < 0) {
                close(fd);
                g_ctx.client_fds[i] = -1;
            }
        }
    }
    pthread_mutex_unlock(&g_ctx.clients_lock);
}

static void add_client(int fd) {
    pthread_mutex_lock(&g_ctx.clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_ctx.client_fds[i] == -1) {
            g_ctx.client_fds[i] = fd;
            if (i >= g_ctx.client_count) g_ctx.client_count = i + 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_ctx.clients_lock);
}

/* Parses simple line-based commands sent by the Java backend, e.g.:
 *   SPAWN name priority burst_ms pages
 *   KILL pid
 *   ALGO RR|PRIORITY|SJF
 */
static void handle_command(engine_state_t *st, char *line) {
    char cmd[16];
    if (sscanf(line, "%15s", cmd) != 1) return;

    if (strcmp(cmd, "SPAWN") == 0) {
        char name[MAX_NAME_LEN];
        int priority, burst, pages;
        if (sscanf(line, "SPAWN %31s %d %d %d", name, &priority, &burst, &pages) == 4) {
            engine_spawn_process(st, name, priority, burst, pages);
        }
    } else if (strcmp(cmd, "KILL") == 0) {
        int pid;
        if (sscanf(line, "KILL %d", &pid) == 1) {
            engine_kill_process(st, pid);
        }
    } else if (strcmp(cmd, "ALGO") == 0) {
        char algo[16];
        if (sscanf(line, "ALGO %15s", algo) == 1) {
            if (strcmp(algo, "RR") == 0) engine_set_algo(st, SCHED_ROUND_ROBIN);
            else if (strcmp(algo, "PRIORITY") == 0) engine_set_algo(st, SCHED_PRIORITY);
            else if (strcmp(algo, "SJF") == 0) engine_set_algo(st, SCHED_SJF);
        }
    }
}

static void *client_reader_thread(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    char buf[512];
    char linebuf[512];
    int linelen = 0;

    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                linebuf[linelen] = '\0';
                if (linelen > 0) handle_command(g_ctx.st, linebuf);
                linelen = 0;
            } else if (linelen < (int)sizeof(linebuf) - 1) {
                linebuf[linelen++] = buf[i];
            }
        }
    }
    return NULL;
}

static void *accept_loop(void *arg) {
    int server_fd = *(int *)arg;
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) continue;

        int one = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        add_client(client_fd);

        int *fd_copy = malloc(sizeof(int));
        *fd_copy = client_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, client_reader_thread, fd_copy);
        pthread_detach(tid);

        fprintf(stderr, "[engine] client connected (fd=%d)\n", client_fd);
    }
    return NULL;
}

static void *tick_loop(void *arg) {
    (void)arg;
    char json_buf[METRICS_BUF];
    while (1) {
        engine_tick(g_ctx.st);

        pthread_mutex_lock(&g_ctx.st->lock);
        int len = metrics_to_json(g_ctx.st, json_buf, sizeof(json_buf));
        pthread_mutex_unlock(&g_ctx.st->lock);

        if (len > 0) broadcast(json_buf, len);

        struct timespec ts = { .tv_sec = 0, .tv_nsec = TICK_INTERVAL_MS * 1000000L };
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int server_start(engine_state_t *st, int port) {
    g_ctx.st = st;
    g_ctx.client_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) g_ctx.client_fds[i] = -1;
    pthread_mutex_init(&g_ctx.clients_lock, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return -1;
    }
    if (listen(server_fd, 8) < 0) {
        perror("listen"); close(server_fd); return -1;
    }

    fprintf(stderr, "[engine] listening on port %d\n", port);

    pthread_t accept_tid, tick_tid;
    int *fd_ptr = malloc(sizeof(int));
    *fd_ptr = server_fd;
    pthread_create(&accept_tid, NULL, accept_loop, fd_ptr);
    pthread_create(&tick_tid, NULL, tick_loop, NULL);

    pthread_join(accept_tid, NULL);
    pthread_join(tick_tid, NULL);
    return 0;
}
