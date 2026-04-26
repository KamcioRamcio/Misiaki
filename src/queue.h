#ifndef OKRETY_QUEUE_H
#define OKRETY_QUEUE_H

#include <stdbool.h>
#include "types.h"

/* Sortowana lista jednokierunkowa po (ts, pid). */

typedef struct {
    request_t *head;
    int        size;
} queue_t;

void       q_init(queue_t *q);
void       q_destroy(queue_t *q);

/* Wstawia nowe żądanie zachowując porządek po (ts, pid).
 * Pole dock przekazywane jest jawnie (statyczny hash z chwili utworzenia REQ). */
void       q_insert(queue_t *q, int ts, int pid, int m, int dock);

/* Usuwa żądanie procesu pid (pierwsze takie). Zwraca true jeżeli usunięto. */
bool       q_remove_by_pid(queue_t *q, int pid);

/* Znajduje żądanie procesu pid. Zwraca NULL jeśli nie ma. */
request_t *q_find_by_pid(queue_t *q, int pid);

/* Liczy poprzedniki my_req w sortowaniu (ts,pid) oraz sumę ich m.
 * Zwraca przez wskaźniki. */
void       q_predecessors(const queue_t *q, const request_t *my_req,
                          int *count, int *m_sum);

#endif
