#ifndef OKRETY_COMM_H
#define OKRETY_COMM_H

#include "types.h"
#include <stdbool.h>

/* Wysyła REQ(ts, pid, m, dock) do wszystkich procesów oprócz my_pid. */
void comm_send_req_all(int ts, int my_pid, int m, int dock, int n_procs);

/* Wysyła ACK(ts) do procesu `to`. */
void comm_send_ack(int ts, int my_pid, int to);

/* Wysyła RELEASE(ts, my_pid) do wszystkich oprócz my_pid. */
void comm_send_release_all(int ts, int my_pid, int n_procs);

/* Nieblokujące sprawdzenie skrzynki + odebranie pierwszej dostępnej wiadomości.
 * Zwraca true jeżeli odebrano wiadomość; wypełnia *tag, *from, *msg.
 * Zwraca false jeżeli skrzynka pusta. */
bool comm_try_recv(int *tag, int *from, wire_msg_t *msg);

#endif
