#ifndef OKRETY_DOCK_H
#define OKRETY_DOCK_H

#include <stdbool.h>
#include "types.h"
#include "queue.h"

/* Deterministyczna reguła przydziału doku — czysta funkcja od (ts, pid).
 * Dock jest niezmienny przez cały czas życia żądania (przypisywany w chwili
 * tworzenia REQ). Każdy proces, który zna (ts, pid, K), oblicza ten sam dok. */
int  dock_assign(int ts, int pid, int K);

/* Warunek W2: żaden poprzednik my_req w Q nie ma tego samego doku.
 * (Jeśli poprzednik z tym samym dokiem istnieje, on i tak wejdzie i wyjdzie
 *  z tego doku przed my_req, ze względu na porządek priorytetów + FIFO.) */
bool dock_w2_satisfied(const queue_t *q, const request_t *my_req);

#endif
