#include "mechanics.h"

bool mech_w3_satisfied(const queue_t *q, const request_t *my_req, int M) {
    if (!q || !my_req || M <= 0) return false;
    int count = 0, m_sum = 0;
    q_predecessors(q, my_req, &count, &m_sum);
    return (m_sum + my_req->m) <= M;
}
