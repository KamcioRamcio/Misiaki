#include "dock.h"

static int req_less(int ts_a, int pid_a, int ts_b, int pid_b) {
  if (ts_a != ts_b)
    return ts_a < ts_b;
  return pid_a < pid_b;
}

int dock_assign(int ts, int pid, int K) {
  if (K <= 0)
    return -1;
  /* (ts + pid) mod K + 1. Modulo na nieujemnej sumie. */
  int s = ts + pid;
  if (s < 0)
    s = -s;
  return (s % K) + 1;
}

bool dock_w2_satisfied(const queue_t *q, const request_t *my_req) {
  if (!q || !my_req)
    return false;
  for (request_t *p = q->head; p; p = p->next) {
    if (p == my_req)
      break;
    if (!req_less(p->ts, p->pid, my_req->ts, my_req->pid))
      continue;
    if (p->dock == my_req->dock)
      return false;
  }
  return true;
}
