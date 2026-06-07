#include "mechanics.h"

#include <stdlib.h>
#include <string.h>

static int req_less_m(int ts_a, int pid_a, int ts_b, int pid_b) {
  if (ts_a != ts_b)
    return ts_a < ts_b;
  return pid_a < pid_b;
}

bool mech_w3_satisfied(const queue_t *q, const request_t *my_req, int M,
                       int K) {
  if (!q || !my_req || M <= 0 || K <= 0)
    return false;

  int *dock_max = calloc((size_t)(K + 1), sizeof(int));
  if (!dock_max)
    return false;

  for (request_t *p = q->head; p && p != my_req; p = p->next) {
    if (!req_less_m(p->ts, p->pid, my_req->ts, my_req->pid))
      continue;
    if (p->dock >= 1 && p->dock <= K && p->m > dock_max[p->dock])
      dock_max[p->dock] = p->m;
  }

  int total = my_req->m;
  for (int d = 1; d <= K; d++)
    total += dock_max[d];

  free(dock_max);
  return total <= M;
}
