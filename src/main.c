#define _DEFAULT_SOURCE
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "clock.h"
#include "comm.h"
#include "dock.h"
#include "log.h"
#include "mechanics.h"
#include "queue.h"
#include "types.h"

/* Konfiguracja runtime */
static int N = 0;   /* liczba procesów (okrętów) */
static int K = 0;   /* liczba doków */
static int M = 0;   /* liczba mechaników */
static int MY = -1; /* własny rank */

static state_t state = STATE_REST;
static queue_t Q;
static request_t *MY_REQ = NULL;
static int MY_TS = 0;
static int MY_M  = 0;
static int MY_DOCK = 0;

static void handle_message(int tag, int from, const wire_msg_t *msg) {
    clock_update(from, msg->ts);

    switch (tag) {
        case MSG_REQ: {
            log_debug("recv REQ od %d (ts=%d, m=%d, dock=%d)",
                      from, msg->ts, msg->m, msg->dock);
            q_insert(&Q, msg->ts, msg->pid, msg->m, msg->dock);
            int ack_ts = clock_tick();
            comm_send_ack(ack_ts, MY, from);
            log_debug("send ACK do %d (ts=%d)", from, ack_ts);
            break;
        }
        case MSG_ACK: {
            log_debug("recv ACK od %d (ts=%d)", from, msg->ts);
            break;
        }
        case MSG_RELEASE: {
            log_debug("recv RELEASE od %d (ts=%d)", from, msg->ts);
            q_remove_by_pid(&Q, msg->pid);
            break;
        }
        default:
            break;
    }
}

static void try_enter_section(void) {
    if (state != STATE_TRYING) return;
    if (!MY_REQ) return;
    if (!clock_w1_satisfied(MY_TS)) return;
    if (!dock_w2_satisfied(&Q, MY_REQ)) return;
    if (!mech_w3_satisfied(&Q, MY_REQ, M, K)) return;

    state = STATE_INSECTION;
    log_state("Wszedłem do doku %d, mam %d mechaników", MY_DOCK, MY_M);
}

static void drain_inbox(void) {
    int tag, from;
    wire_msg_t msg;
    while (comm_try_recv(&tag, &from, &msg)) {
        handle_message(tag, from, &msg);
    }
}

static void rest_phase(void) {
    log_state("Wracam z walki");
    /* Czas walki/odpoczynku przed kolejnym powrotem do bazy. */
    for (int i = 0; i < 20; ++i) {
        drain_inbox();
        usleep(50 * 1000); /* 50 ms */
    }
}

static void start_request(void) {
    MY_M = (rand() % M) + 1;
    MY_TS = clock_tick();
    MY_DOCK = dock_assign(MY_TS, MY, K);
    q_insert(&Q, MY_TS, MY, MY_M, MY_DOCK);
    MY_REQ = q_find_by_pid(&Q, MY);
    state = STATE_TRYING;
    log_state("Rozpoczynam staranie o sekcję (m=%d, dock=%d, ts=%d)",
              MY_M, MY_DOCK, MY_TS);
    comm_send_req_all(MY_TS, MY, MY_M, MY_DOCK, N);
}

static void trying_phase(void) {
    while (state == STATE_TRYING) {
        drain_inbox();
        try_enter_section();
        if (state != STATE_TRYING) break;
        usleep(5 * 1000);
    }
}

static void section_phase(void) {
    /* Symulacja naprawy. Podczas naprawy nadal odbieramy wiadomości
     * (REQ od innych) i odpowiadamy ACK, by nie blokować innych procesów. */
    int repair_ms = (rand() % 200) + 50;
    int elapsed = 0;
    while (elapsed < repair_ms) {
        drain_inbox();
        usleep(10 * 1000);
        elapsed += 10;
    }

    /* Wyjście z sekcji: usuń własne żądanie i wyślij RELEASE. */
    int dock_done = MY_DOCK;
    int mech_done = MY_M;
    q_remove_by_pid(&Q, MY);
    MY_REQ = NULL;
    int rel_ts = clock_tick();
    log_state("Zwalniam dok %d i %d mechaników", dock_done, mech_done);
    comm_send_release_all(rel_ts, MY, N);
    state = STATE_REST;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &MY);

    if (argc < 3) {
        if (MY == 0) {
            fprintf(stderr,
                    "Użycie: mpirun -np N %s K M [seed]\n"
                    "  K — liczba doków, M — liczba mechaników\n",
                    argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    K = atoi(argv[1]);
    M = atoi(argv[2]);
    unsigned int seed = (argc >= 4)
        ? (unsigned int)atoi(argv[3]) ^ (unsigned int)MY
        : (unsigned int)time(NULL) ^ (unsigned int)MY;
    if (K <= 0 || M <= 0) {
        if (MY == 0) fprintf(stderr, "K i M muszą być dodatnie.\n");
        MPI_Finalize();
        return 1;
    }
    srand(seed);

    clock_init(N, MY);
    log_init(MY);
    q_init(&Q);

    if (MY == 0) {
        log_state("Start: N=%d, K=%d, M=%d", N, K, M);
    }

    while (1) {
        rest_phase();
        start_request();
        trying_phase();
        section_phase();
    }

    q_destroy(&Q);
    MPI_Finalize();
    return 0;
}
