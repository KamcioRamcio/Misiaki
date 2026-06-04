# Szczegółowy opis implementacji — Okręty / Misie

Ten dokument opisuje implementację algorytmu Lamport-Dual w katalogu `src/`.
Opis algorytmu (bez kodu) — patrz `docs/opis_algorytmu.md`.

---

## 1. Mapa modułów

```
src/
  types.h      — stałe, enumy, struktury danych (wire_msg_t, request_t, stan)
  clock.c/h    — skalarny zegar Lamporta + wektor last_seen[N] (warunek W1)
  queue.c/h    — posortowana lista żądań (kolejka Lamporta Q)
  dock.c/h     — reguła dock_assign + sprawdzenie warunku W2
  mechanics.c/h — sprawdzenie warunku W3 (per-dok max mechaników)
  comm.c/h     — wysyłanie/odbiór wiadomości MPI
  log.c/h      — logowanie zmian stanu + tryb DEBUG
  main.c       — automat stanów REST/TRYING/INSECTION, pętla główna
```

Zależności między modułami:

```
main.c → clock, comm, dock, log, mechanics, queue, types
dock.c → queue, types
mechanics.c → queue, types
comm.c → types
log.c → clock
```

---

## 2. Struktury danych (`src/types.h`, `src/queue.h`)

### `request_t` (jeden wpis w kolejce Q)

| Pole   | Typ  | Znaczenie                                                  |
|--------|------|------------------------------------------------------------|
| `ts`   | int  | Znacznik Lamporta przypisany w chwili wysłania REQ         |
| `pid`  | int  | Identyfikator procesu nadawcy                              |
| `m`    | int  | Liczba żądanych mechaników (1..M)                          |
| `dock` | int  | Przypisany dok (1..K), wyliczony deterministycznie         |
| `next` | ptr  | Wskaźnik listy jednokierunkowej                            |

Porządek sortowania: `(ts, pid)` rosnąco — mniejszy ts = wyższy priorytet;
przy remisie ts — mniejszy pid = wyższy priorytet.

### `queue_t`

| Pole   | Typ        | Znaczenie                  |
|--------|------------|----------------------------|
| `head` | request_t* | Głowa posortowanej listy   |
| `size` | int        | Liczba elementów w liście  |

### `wire_msg_t` (format wiadomości MPI)

| Pole   | Typ | Używane w          |
|--------|-----|--------------------|
| `ts`   | int | REQ, ACK, RELEASE  |
| `pid`  | int | REQ, ACK, RELEASE  |
| `m`    | int | REQ (0 w pozostałych) |
| `dock` | int | REQ (0 w pozostałych) |

Format na "drucie" = 4 x int, zarejestrowany jako `MPI_Datatype WIRE_T`.

### `state_t`

```c
STATE_REST      — brak aktywnego żądania
STATE_TRYING    — oczekiwanie na W1 ∧ W2 ∧ W3
STATE_INSECTION — w sekcji krytycznej
```

---

## 3. Moduł zegara (`src/clock.c`)

### Zmienne modułu

| Zmienna      | Typ  | Znaczenie                                              |
|--------------|------|--------------------------------------------------------|
| `L`          | int  | Lokalny zegar Lamporta                                 |
| `last_seen[]`| int* | `last_seen[k]` = ts ostatniej wiadomości od procesu k  |
| `N`, `MY`    | int  | Liczba procesów i własny pid (do sprawdzania W1)       |

### Funkcje

#### `clock_init(n_procs, my_pid)`
Alokuje `last_seen[n_procs]` wyzerowany. Ustawia `N = n_procs`, `MY = my_pid`, `L = 0`.

#### `clock_tick() → int`
Zdarzenie lokalne lub wysłanie wiadomości: `L += 1; return L`.
Używane przy: tworzeniu nowego żądania (`MY_TS`), wysłaniu ACK, wysłaniu RELEASE.

#### `clock_update(from, ts) → int`
Odbiór wiadomości: `L = max(L, ts) + 1; last_seen[from] = ts; return L`.
Wywołane dla **każdej** odebranej wiadomości (REQ, ACK, RELEASE) przed
dalszą obsługą. Kumuluje wiedzę o stanach innych procesów.

#### `clock_w1_satisfied(my_ts) → int`
Sprawdza warunek W1: `∀k ≠ MY : last_seen[k] > my_ts`.
Zwraca 1 (prawda) jeśli od każdego innego procesu odebrano wiadomość
ze znacznikiem ściśle większym niż `my_ts`. Gwarantuje, że nasze REQ
dotarło do każdego (kanały FIFO, monotoniczność zegara).

---

## 4. Moduł kolejki (`src/queue.c`)

### `q_insert(q, ts, pid, m, dock)`
Wstawia nowy węzeł w porządku `(ts, pid)`. Zawiera ochronę przed duplikatem:
jeśli `(ts, pid)` już istnieje — nie wstawia. Ważne przy własnym żądaniu
(proces wstawia je do Q przed broadcast, więc nie chcemy duplikatu gdy
dostanie własne REQ z zewnątrz — ale REQ wysyłane jest tylko do innych,
więc duplikat nie wystąpi; zabezpieczenie istnieje pro forma).

### `q_remove_by_pid(q, pid) → bool`
Usuwa pierwszy wpis procesu `pid`. Wywoływane przy obsłudze RELEASE.

### `q_find_by_pid(q, pid) → request_t*`
Wyszukuje wpis procesu `pid`. Używane po `q_insert` w `start_request`
do uzyskania wskaźnika `MY_REQ` na własny element kolejki.

### `q_predecessors(q, my_req, *count, *m_sum)`
Liczy poprzedników (wpisy z `(ts,pid) < my_req.(ts,pid)`) i ich sumę `m`.
Używane jedynie w kodzie diagnostycznym; logika W3 używa własnej pętli.

---

## 5. Moduł doku (`src/dock.c`)

### `dock_assign(ts, pid, K) → int`
Czysta funkcja deterministyczna: `((ts + pid) % K) + 1`.
Wywoływana raz przy tworzeniu żądania. Wynik niezmienny przez całe życie żądania.
Każdy proces, który zna `(ts, pid, K)` żądania, oblicza ten sam dok.

### `dock_w2_satisfied(q, my_req) → bool`
Warunek W2: przegląda poprzedników `my_req` w Q (pętla identyczna jak
w `q_predecessors`). Zwraca false, jeśli jakikolwiek poprzednik ma
`p->dock == my_req->dock`. Znaczenie: jeśli ktoś z wyższym priorytetem
chce tego samego doku, my czekamy aż go zwolni.

---

## 6. Moduł mechaników (`src/mechanics.c`)

### `mech_w3_satisfied(q, my_req, M, K) → bool`

Warunek W3 w nowej wersji (per-dok max):

```
1. Alokuj tablicę dock_max[K+1] wyzerowaną (indeks 1..K).
2. Iteruj poprzedników my_req w Q:
     for p in Q while p != my_req:
       if (ts_p, pid_p) < (ts_my, pid_my):
         dock_max[p->dock] = max(dock_max[p->dock], p->m)
3. total = my_req->m + Σ_{d=1}^{K} dock_max[d]
4. Zwróć total ≤ M.
```

**Dlaczego per-dok max, nie suma wszystkich?**
Warunek W2 gwarantuje, że na tym samym doku nie mogą równocześnie przebywać
dwa okręty w INSECTION. Spośród wszystkich poprzedników na doku `d` co
najwyżej jeden będzie jednocześnie w sekcji — ten z największym `m`.
Sumowanie surowychm wszystkich poprzedników na tym samym doku było zawyżaniem
zajętości, co blokowało wejście mimo realnie dostępnych mechaników.

---

## 7. Moduł komunikacji (`src/comm.c`)

### `comm_send_req_all(ts, my_pid, m, dock, n_procs)`
Tworzy `wire_msg_t = {ts, my_pid, m, dock}` i wysyła (tag `MSG_REQ`)
do wszystkich procesów oprócz `my_pid`. Jeden `MPI_Send` per odbiorca.

### `comm_send_ack(ts, my_pid, to)`
Wysyła `wire_msg_t = {ts, my_pid, 0, 0}` (tag `MSG_ACK`) do procesu `to`.

### `comm_send_release_all(ts, my_pid, n_procs)`
Wysyła `wire_msg_t = {ts, my_pid, 0, 0}` (tag `MSG_RELEASE`)
do wszystkich oprócz `my_pid`.

### `comm_try_recv(tag, from, msg) → bool`
Nieblokujące sprawdzenie skrzynki: `MPI_Iprobe` + `MPI_Recv`.
Zwraca true i wypełnia `*tag, *from, *msg` jeśli coś czeka.
Zwraca false natychmiast jeśli skrzynka pusta.
Używane w pętlach `drain_inbox` we wszystkich fazach.

### `WIRE_T` (MPI_Datatype)
Zarejestrowany typ MPI dla `wire_msg_t` (4 x int, obliczane offsety
przez `MPI_Get_address`). Inicjalizowany leniwie przy pierwszym wysłaniu
(`ensure_type`). Zapewnia przenośność między architekturami.

---

## 8. Moduł logowania (`src/log.c`)

### `log_state(fmt, ...)`
Zawsze drukowany. Format: `[pid] [tX] tekst\n`.
Zegar Lamporta pobierany przez `clock_get()` w chwili logowania.

### `log_debug(fmt, ...)`
Drukowany tylko z `-DDEBUG`. Format: `[pid] [tX] (dbg) tekst\n`.
W trybie release jest inline no-op (kompilator usuwa wywołania całkowicie).

---

## 9. Główny automat (`src/main.c`)

### Zmienne globalne modułu

| Zmienna    | Typ        | Znaczenie                                                    |
|------------|------------|--------------------------------------------------------------|
| `N`        | int        | Liczba procesów MPI (z `MPI_Comm_size`)                      |
| `K`        | int        | Liczba doków (argument programu)                             |
| `M`        | int        | Liczba mechaników (argument programu)                        |
| `MY`       | int        | Własny rank MPI (z `MPI_Comm_rank`)                          |
| `state`    | state_t    | Bieżący stan automatu                                        |
| `Q`        | queue_t    | Lokalna kopia kolejki żądań Lamporta                         |
| `MY_REQ`   | request_t* | Wskaźnik na własny wpis w Q (NULL gdy REST)                  |
| `MY_TS`    | int        | Znacznik Lamporta bieżącego żądania                          |
| `MY_M`     | int        | Liczba mechaników żądana w bieżącej iteracji                 |
| `MY_DOCK`  | int        | Dok przypisany bieżącemu żądaniu                             |

### `handle_message(tag, from, msg)`
Centralna obsługa jednej wiadomości:
1. `clock_update(from, msg->ts)` — zawsze pierwsze,
2. `MSG_REQ`: `q_insert` + `comm_send_ack`,
3. `MSG_ACK`: tylko efekt na zegarze i `last_seen`,
4. `MSG_RELEASE`: `q_remove_by_pid`.

### `drain_inbox()`
Pętla `while (comm_try_recv(...)) handle_message(...)`.
Wywołana we wszystkich trzech fazach — proces nigdy nie blokuje się
biernie na wiadomości. Gwarantuje, że inne procesy nie czekają na ACK.

### `rest_phase()`
Symuluje czas walki/odpoczynku: 20 × 50 ms = ok. 1 s.
W każdej iteracji wywołuje `drain_inbox()` — nie blokuje postępu innych.

### `start_request()`
Tworzy nowe żądanie i inicjuje TRYING:
1. `MY_M = rand() % M + 1`,
2. `MY_TS = clock_tick()`,
3. `MY_DOCK = dock_assign(MY_TS, MY, K)`,
4. `q_insert(&Q, MY_TS, MY, MY_M, MY_DOCK)`,
5. `MY_REQ = q_find_by_pid(&Q, MY)`,
6. `state = STATE_TRYING`,
7. `comm_send_req_all(MY_TS, MY, MY_M, MY_DOCK, N)`.

### `try_enter_section()`
Sprawdza po kolei (krótkie spięcie): `W1 → W2 → W3`.
Gdy wszystkie prawdziwe: `state = STATE_INSECTION`.

### `trying_phase()`
Aktywna pętla oczekiwania:
```
while state == TRYING:
    drain_inbox()
    try_enter_section()
    if state != TRYING: break
    usleep(5 ms)   // krótki spin-sleep by nie palić 100% CPU
```

### `section_phase()`
Symuluje naprawę (losowy czas 50–249 ms) z ciągłym `drain_inbox()`.
Po naprawie:
1. `q_remove_by_pid(&Q, MY)`,
2. `MY_REQ = NULL`,
3. `rel_ts = clock_tick()`,
4. `comm_send_release_all(rel_ts, MY, N)`,
5. `state = STATE_REST`.

---

## 10. Pełny cykl jednego okrętu (kolejność zdarzeń)

```
1. MPI_Init, odczyt N/MY/K/M/seed, clock_init, q_init.

2. REST:
   - log "Wracam z walki"
   - drain_inbox w pętli przez ok. 1 s

3. TRYING (start):
   - losowanie MY_M
   - clock_tick → MY_TS
   - dock_assign → MY_DOCK
   - q_insert → MY_REQ
   - state = TRYING
   - log "Rozpoczynam staranie"
   - broadcast REQ

4. TRYING (oczekiwanie):
   - drain_inbox (obsługa REQ/ACK/RELEASE od innych)
   - sprawdzanie W1 ∧ W2 ∧ W3 po każdym zestawie wiadomości

5. INSECTION:
   - log "Wszedłem do doku d, mam m mechaników"
   - drain_inbox w pętli przez 50–249 ms (nadal odpowiada ACK na REQ)

6. Wyjście z sekcji:
   - usunięcie MY_REQ z Q
   - broadcast RELEASE
   - log "Zwalniam dok d i m mechaników"
   - state = REST

7. Powrót do kroku 2.
```

---

## 11. Jak rośnie zegar Lamporta

`L` rośnie w dwóch przypadkach:

1. **`clock_tick()`** — zdarzenie lokalne / wysyłka:
   - przy tworzeniu własnego żądania (MY_TS),
   - przy wysyłaniu ACK po odebranym REQ,
   - przy wysyłaniu RELEASE po wyjściu z sekcji.

2. **`clock_update(from, ts)`** — odbiór wiadomości:
   - `L = max(L, ts) + 1`,
   - `last_seen[from] = ts`.

Konsekwencja: każda odebrana wiadomość przesuwa `last_seen[nadawcy]`,
co może domknąć warunek W1 dla odpowiedniego k.

---

## 12. Niezmienniki i ich sprawdzanie

| Niezmiennik | Definicja                                          | Skąd wynika     |
|-------------|---------------------------------------------------|-----------------|
| I1          | Na każdym doku ≤ 1 okręt w INSECTION jednocześnie | W2 + FIFO + W1  |
| I2          | Σ m aktywnych okrętów ≤ M w każdej chwili         | W3 + I1         |
| I3          | Brak zakleszczenia                                 | Min. priorytet ma puste predecessors |
| I4          | Brak głodzenia                                     | Skończona liczba poprzedników |

Weryfikacja automatyczna: `scripts/verify_logs.py <plik_log> <M>`.
Skrypt parsuje linie `Wszedłem do doku` i `Zwalniam dok`, sortuje
po znaczniku Lamporta i sprawdza I1 i I2 w symulowanym czasie.

---

## 13. Kompilacja i uruchomienie

```
make            # release (tylko logi zmian stanu)
make debug      # DEBUG: dodatkowe logi per-wiadomość (flag -DDEBUG)
make clean
```

Uruchomienie:
```
mpirun -np <N> ./okrety <K> <M> [seed]
```

Przykład:
```
mpirun -np 6 ./okrety 3 5 42 2>&1 | tee /tmp/run.log
python3 scripts/verify_logs.py /tmp/run.log 5
```

Wielomaszynowe (wymóg `wymagania.md`, linia 73):
```
mpirun --hostfile hosts -np 8 ./okrety 3 5 42
```

---

## 14. Interpretacja logów

Format każdej linii stanu: `[pid] [tX] tekst`.

| Tekst                                  | Stan         | Znaczenie                                   |
|----------------------------------------|--------------|---------------------------------------------|
| `Wracam z walki`                       | → REST       | Okręt wchodzi w fazę odpoczynku             |
| `Rozpoczynam staranie (m=X, dock=Y, ts=Z)` | → TRYING | REQ wysłany do wszystkich                  |
| `Wszedłem do doku D, mam M mechaników`| → INSECTION  | W1∧W2∧W3 spełnione, sekcja krytyczna zajęta|
| `Zwalniam dok D i M mechaników`        | → REST       | RELEASE wysłany, zasoby zwolnione           |

W trybie DEBUG dochodzą linie z `(dbg)`:
- `recv REQ od j (ts=X, m=Y, dock=Z)` — odbiór żądania,
- `send ACK do j (ts=X)` — wysłanie potwierdzenia,
- `recv ACK od j (ts=X)` — odbiór potwierdzenia,
- `recv RELEASE od j (ts=X)` — odbiór zwolnienia.

---

## 15. Punkty rozbudowy (dla prowadzącego)

Zmiany w algorytmie wymagające modyfikacji konkretnych funkcji:

| Zmiana                               | Pliki          | Funkcja/linia                   |
|--------------------------------------|----------------|---------------------------------|
| Inna reguła przydziału doku          | `dock.c`       | `dock_assign`                   |
| Zmiana definicji W3                  | `mechanics.c`  | `mech_w3_satisfied`             |
| Dodanie nowego typu wiadomości       | `types.h`      | `MSG_*`, `wire_msg_t`           |
|                                      | `comm.c`       | nowa funkcja `comm_send_*`      |
|                                      | `main.c`       | `handle_message` case           |
| Zmiana czasu naprawy / odpoczynku    | `main.c`       | `section_phase`, `rest_phase`   |
| Algorytm Ricarta-Agrawali            | `main.c`       | zastąpienie `MSG_RELEASE` + ACK |
