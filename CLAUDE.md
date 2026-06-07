# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository status

This repo currently holds **only the Polish-language specification** for a university distributed-computing course project (Przetwarzanie Rozproszone). There is no source code, build system, or tests yet - implementation has not started. All documents are under `project/`. A `project/plan_projektu_okrety.md` was previously present but has been deleted (visible in `git status`).

When the user asks for implementation, default to **C + MPI** (the only officially supported combination per `project/wymagania.md`). Build with `mpicc`, run with `mpirun -np <N>`. Python+MPI is tolerated but unsupported; other message-passing frameworks are allowed if they are async, FIFO, and run across multiple machines.

## The problem (`project/task.md`) - "Okręty / Misie"

N ships return from war and compete for:
- **K distinguishable docks** (load must be balanced; selection must be a deterministic rule, not random),
- **M indistinguishable mechanics** (count needed depends on damage).

A ship may legitimately wait for dock `i` even when docks `1..i-1` are free - the deterministic dock-assignment rule decides which dock each ship targets, and ships only contend for the dock they were assigned. The algorithm must answer two questions concurrently for each waiting ship: *do I get access*, and *to which dock*.

## Non-negotiable design constraints (`project/wymagania.md`)

These come from the course rubric - violating any of them fails the project. Read `project/wymagania.md` in full before proposing or reviewing an algorithm.

- **No central coordinator, no resource managers, no global locks.** Resources are passive; processes negotiate among themselves.
- **Indistinguishable resources of count X must be guarded by a generalized critical section of capacity X.** Forbidden alternatives: (a) a capacity-1 section guarding a counter, (b) X separate capacity-1 sections. So the M mechanics must live behind one capacity-M section.
- **No nested critical sections** to protect access to other critical sections.
- **Communication: only `MPI_Send`/`MPI_Recv`** (or their async variants). Collective/sync ops are allowed *only* for one-time init.
- **Environment assumptions:** fully async, reliable FIFO channels, no process failures, processes run in an infinite loop. Do not assume that "waiting a few seconds" changes anything - clocks are unsynchronized.
- **Maximize parallelism.** For the dock-assignment piece especially, avoid serializing through `n-1` critical sections in the worst case. Random selection is allowed but considered suboptimal.
- **Liveness, not just probability-1.** Algorithms must guarantee eventual access (no starvation). Fairness is welcome but not required.
- **Time complexity is the primary optimization target**, then communication complexity. Token-based algorithms are discouraged - justify them if used.
- **Parameterize everything** (N, K, M, capacities) so tests can vary them quickly.

## Logging convention

Every state-change log line must be prefixed with process id and Lamport clock:

```
[1] [t100101] Rozpoczynam staranie o sekcję krytyczną
[1] [t100102] Jestem w sekcji krytycznej
[1] [t100103] Wychodzę z sekcji krytycznej
```

Provide a verbose mode (toggled via `#define`) for per-message debug output, but **strip "sent X" / "received X" / "waiting…" lines from the final version** - they drown out correctness signals during defense. The course suggests reusing the `println` macro from the earlier Lamport-clock lab.

## Reference algorithms (`project/rozproszona_sekcja_krytyczna.md`)

Two textbook bases the implementation is expected to extend to capacity-K:

- **Lamport** - `REQ` / `ACK` / `RELEASE`, local sorted request queue + per-process last-seen timestamp vector. Time 3, comm 3n.
- **Ricart–Agrawala** - `REQ` / `ACK` only, deferred-ack queue. Time 2, comm 2n. Has an optimization that drops one `ACK` per pair when both processes are contending, giving instantaneous time 1 / comm n in the all-contending case.

Both can be generalized to allow X concurrent processes in the section. The doc contrasts these against trivially broken approaches (no priority, priority without queue) and the centralized-manager solution that is *forbidden* here.

## Algorithm-presentation format

When drafting or reviewing an algorithm description for the instructor, follow the four-section template in `project/wymagania.md` (lines 35–65): **OPIS OGÓLNY**, **STANY PROCESÓW**, **TYPY WIADOMOŚCI**, then per-state message handling. The course explicitly **rejects** Python-pseudocode or thread-by-thread narration. Number the lines so they can be referenced during defense.

## Defense logistics

Final demo must run on **at least two machines**. The instructor may ask either group member algorithm or code questions and may request live modifications. A printed copy of the algorithm description is recommended.
