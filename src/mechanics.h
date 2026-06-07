#ifndef OKRETY_MECHANICS_H
#define OKRETY_MECHANICS_H

#include "queue.h"
#include "types.h"
#include <stdbool.h>

/* Warunek W3: Σ_d max{m_j : j poprzednik my_req, dok_j = d} + m_my ≤ M.
 * Per-dok bierzemy MAX m spośród poprzedników - z W2 wynika, że na danym
 * doku równocześnie może być najwyżej jeden okręt, więc sumy per-dok są
 * rzeczywistymi ograniczeniami zasobów. */
bool mech_w3_satisfied(const queue_t *q, const request_t *my_req, int M, int K);

#endif
