#ifndef OKRETY_MECHANICS_H
#define OKRETY_MECHANICS_H

#include <stdbool.h>
#include "queue.h"
#include "types.h"

/* Warunek W3: Σ m_j po wszystkich poprzednikach my_req w Q + m_my ≤ M. */
bool mech_w3_satisfied(const queue_t *q, const request_t *my_req, int M);

#endif
