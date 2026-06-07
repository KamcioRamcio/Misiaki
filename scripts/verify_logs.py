#!/usr/bin/env python3
"""Sprawdza niezmienniki algorytmu Lamport-Dual na podstawie logów programu.

Niezmienniki:
  I1: w każdej chwili na każdym doku jest <= 1 okręt,
  I2: w każdej chwili suma m aktywnych okrętów <= M.

Zdarzenia są sortowane po (lamport_ts, pid, kind) - RELEASE dla danego pid
przed ENTER innego pid przy równych timestampach (LEAVE pierwszy zwalnia
zasób przed nową okupacją).

Użycie: ./verify_logs.py <plik_z_logami> <M>
"""
import re
import sys

ENTER = re.compile(r"\[(\d+)\] \[t(\d+)\] Wszedłem do doku (\d+), mam (\d+) mechaników")
LEAVE = re.compile(r"\[(\d+)\] \[t(\d+)\] Zwalniam dok (\d+) i (\d+) mechaników")


def main(path: str, M: int) -> int:
    events: list[tuple[int, int, int, str, int, int]] = []
    # (ts, kind_priority, pid, kind, dock, mech) - kind_priority: 0=LEAVE, 1=ENTER

    with open(path) as fh:
        for line in fh:
            m = ENTER.search(line)
            if m:
                pid, t, d, mech = map(int, m.groups())
                events.append((t, 1, pid, "ENTER", d, mech))
                continue
            m = LEAVE.search(line)
            if m:
                pid, t, d, mech = map(int, m.groups())
                events.append((t, 0, pid, "LEAVE", d, mech))

    events.sort()

    docks: dict[int, int] = {}
    mech_per_pid: dict[int, int] = {}
    errors: list[str] = []

    for t, _kp, pid, kind, d, mech in events:
        if kind == "ENTER":
            if d in docks:
                errors.append(
                    f"VIOLATE I1 @t={t}: dok {d} zajęty przez {docks[d]}, próbuje {pid}"
                )
            docks[d] = pid
            mech_per_pid[pid] = mech
            total = sum(mech_per_pid.values())
            if total > M:
                errors.append(
                    f"VIOLATE I2 @t={t}: suma mech {total} > M={M} (pid={pid} +{mech})"
                )
        else:  # LEAVE
            if docks.get(d) != pid:
                errors.append(
                    f"BAD RELEASE @t={t}: pid={pid} zwalnia dok {d}, trzymał {docks.get(d)}"
                )
            else:
                del docks[d]
            mech_per_pid.pop(pid, None)

    print(f"sprawdzonych zdarzeń: {len(events)}")
    if errors:
        for e in errors:
            print(e)
        return 1
    print("OK: wszystkie niezmienniki spełnione")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1], int(sys.argv[2])))
