#include "clock.h"

#include <stdlib.h>
#include <string.h>

static int  L = 0;
static int  N = 0;
static int  MY = -1;
static int *last_seen = NULL;

void clock_init(int n_procs, int my_pid) {
    N = n_procs;
    MY = my_pid;
    free(last_seen);
    last_seen = calloc((size_t)N, sizeof(int));
    L = 0;
}

int clock_tick(void) {
    L += 1;
    return L;
}

int clock_update(int from, int ts) {
    if (ts > L) L = ts;
    L += 1;
    if (from >= 0 && from < N) last_seen[from] = ts;
    return L;
}

int clock_get(void) { return L; }

int clock_last_seen(int pid) {
    if (pid < 0 || pid >= N) return 0;
    return last_seen[pid];
}

int clock_w1_satisfied(int my_ts) {
    for (int k = 0; k < N; ++k) {
        if (k == MY) continue;
        if (last_seen[k] <= my_ts) return 0;
    }
    return 1;
}
