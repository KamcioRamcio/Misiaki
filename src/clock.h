#ifndef OKRETY_CLOCK_H
#define OKRETY_CLOCK_H

/* Zegar Lamporta + wektor last_seen[N] dla warunku W1. */

void clock_init(int n_procs, int my_pid);

/* Zwraca i inkrementuje lokalny zegar przed zdarzeniem wewnętrznym/wysłaniem.
 */
int clock_tick(void);

/* Aktualizuje zegar po odebraniu wiadomości od procesu `from` z znacznikiem
 * `ts`. Wykonuje: L := max(L, ts) + 1; last_seen[from] := ts. */
int clock_update(int from, int ts);

int clock_get(void);
int clock_last_seen(int pid);

/* Warunek W1: ∀k ≠ my_pid : last_seen[k] > my_ts. */
int clock_w1_satisfied(int my_ts);

#endif
