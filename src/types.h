#ifndef OKRETY_TYPES_H
#define OKRETY_TYPES_H

#include <stdbool.h>

typedef enum { STATE_REST, STATE_TRYING, STATE_INSECTION } state_t;

/* Pojedyncze żądanie w kolejce Lamporta. */
typedef struct request {
  int ts;   /* znacznik czasowy Lamporta przypisany przy utworzeniu */
  int pid;  /* identyfikator procesu nadawcy */
  int m;    /* liczba żądanych mechaników */
  int dock; /* przydzielony dok, niezmienny: ((ts + pid) mod K) + 1 */
  struct request *next;
} request_t;

/* Tagi MPI. */
#define MSG_REQ 1
#define MSG_ACK 2
#define MSG_RELEASE 3

/* Format wiadomości: cztery inty [ts, pid, m, dock].
 * m i dock są używane tylko dla REQ; dla ACK i RELEASE pola pozostają 0. */
typedef struct {
  int ts;
  int pid;
  int m;
  int dock;
} wire_msg_t;

#endif
