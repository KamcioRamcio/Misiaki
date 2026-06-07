#include "queue.h"

#include <stdlib.h>

void q_init(queue_t *q) {
  q->head = NULL;
  q->size = 0;
}

void q_destroy(queue_t *q) {
  request_t *p = q->head;
  while (p) {
    request_t *n = p->next;
    free(p);
    p = n;
  }
  q->head = NULL;
  q->size = 0;
}

/* Porządek: rosnąco po (ts, pid). */
static int req_less(int ts_a, int pid_a, int ts_b, int pid_b) {
  if (ts_a != ts_b)
    return ts_a < ts_b;
  return pid_a < pid_b;
}

void q_insert(queue_t *q, int ts, int pid, int m, int dock) {
  request_t *node = (request_t *)malloc(sizeof(request_t));
  node->ts = ts;
  node->pid = pid;
  node->m = m;
  node->dock = dock;
  node->next = NULL;

  request_t **link = &q->head;
  while (*link && req_less((*link)->ts, (*link)->pid, ts, pid)) {
    link = &(*link)->next;
  }
  /* Ochrona przed duplikatem (ts, pid) - jeśli istnieje, nie wstawiaj. */
  if (*link && (*link)->ts == ts && (*link)->pid == pid) {
    free(node);
    return;
  }
  node->next = *link;
  *link = node;
  q->size += 1;
}

bool q_remove_by_pid(queue_t *q, int pid) {
  request_t **link = &q->head;
  while (*link) {
    if ((*link)->pid == pid) {
      request_t *gone = *link;
      *link = gone->next;
      free(gone);
      q->size -= 1;
      return true;
    }
    link = &(*link)->next;
  }
  return false;
}

request_t *q_find_by_pid(queue_t *q, int pid) {
  for (request_t *p = q->head; p; p = p->next) {
    if (p->pid == pid)
      return p;
  }
  return NULL;
}

void q_predecessors(const queue_t *q, const request_t *my_req, int *count,
                    int *m_sum) {
  int c = 0, s = 0;
  for (request_t *p = q->head; p; p = p->next) {
    if (p == my_req)
      break;
    if (req_less(p->ts, p->pid, my_req->ts, my_req->pid)) {
      c += 1;
      s += p->m;
    }
  }
  if (count)
    *count = c;
  if (m_sum)
    *m_sum = s;
}
