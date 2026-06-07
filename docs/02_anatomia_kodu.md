# Anatomia kodu — opis pliku po pliku

Każdy plik z `src/` opisany osobno: cel, zmienne, każda funkcja (parametry, zwrot, logika, skąd wołana).

---

## Tabela zależności modułów

```
main.c      → clock, comm, dock, log, mechanics, queue, types
dock.c      → queue, types
mechanics.c → queue, types
comm.c      → types (+ mpi.h)
log.c       → clock
queue.c     → types
clock.c     → (samowystarczalny, stdlib.h + string.h)
types.h     → (samowystarczalny, stdbool.h)
```

---

## src/types.h

**Cel modułu:** Centralne miejsce na wszystkie typy danych i stałe. Nie zawiera kodu wykonywalnego.

**Zależy od:** `<stdbool.h>` (systemowy).

---

### Typy wyliczeniowe

#### `state_t`

```c
typedef enum { STATE_REST, STATE_TRYING, STATE_INSECTION } state_t;
```

Reprezentuje bieżący stan automatu procesu:

| Wartość | Znaczenie |
|---------|-----------|
| `STATE_REST` | Okręt odpoczywa/walczy. Brak aktywnego żądania. `MY_REQ == NULL`. |
| `STATE_TRYING` | Okręt wysłał REQ, czeka na spełnienie W1 ∧ W2 ∧ W3. |
| `STATE_INSECTION` | Okręt jest w doku, mechanicy przydzieleni. Sekcja krytyczna zajęta. |

---

### Struktury

#### `request_t` — jeden wpis w kolejce Q

```c
typedef struct request {
    int ts;
    int pid;
    int m;
    int dock;
    struct request *next;
} request_t;
```

| Pole | Typ | Wartości | Znaczenie |
|------|-----|----------|-----------|
| `ts` | `int` | ≥1 | Znacznik Lamporta przypisany w chwili wysłania REQ. Wyznacza priorytet: mniejszy ts = wyższy priorytet. |
| `pid` | `int` | 0..N-1 | Identyfikator procesu nadawcy (rank MPI). Tie-breaker przy równych ts. |
| `m` | `int` | 1..M | Ile mechaników żąda okręt. Losowane raz przy wejściu do TRYING. |
| `dock` | `int` | 1..K | Przypisany dok. Obliczony deterministycznie jako `((ts+pid) mod K)+1`. Niezmienny przez całe życie żądania. |
| `next` | `request_t*` | wskaźnik lub NULL | Łącze listy jednokierunkowej. Używane przez `queue_t`. |

Porządek sortowania pary `(ts, pid)`: leksykograficzny rosnąco. Mniejsza para = wyższy priorytet w algorytmie Lamporta.

#### `wire_msg_t` — format wiadomości MPI

```c
typedef struct {
    int ts;
    int pid;
    int m;
    int dock;
} wire_msg_t;
```

Cztery pola `int` wysyłane przez MPI jako jeden rekord (zarejestrowany typ `WIRE_T`).

| Pole | Używane w | Dla ACK i RELEASE |
|------|-----------|-------------------|
| `ts` | REQ, ACK, RELEASE | znacznik Lamporta nadawcy w chwili wysłania |
| `pid` | REQ, ACK, RELEASE | identyfikator nadawcy |
| `m` | REQ | 0 (zerowane) |
| `dock` | REQ | 0 (zerowane) |

**Uwaga:** Dla ACK i RELEASE pola `m=0` i `dock=0` — odbiorca nie powinien interpretować tych pól dla tych tagów.

---

### Stałe — tagi MPI

```c
#define MSG_REQ     1
#define MSG_ACK     2
#define MSG_RELEASE 3
```

Używane jako tag w `MPI_Send`/`MPI_Recv`. Jednoznacznie identyfikują typ wiadomości bez parsowania treści. Wartości arbitralne, ważne żeby różne.

---

## src/clock.c / src/clock.h

**Cel modułu:** Skalarny zegar Lamporta plus wektor `last_seen[N]` potrzebny do sprawdzenia warunku W1.

**Zależy od:** `<stdlib.h>`, `<string.h>`.

---

### Zmienne statyczne (prywatne dla modułu)

| Zmienna | Typ | Inicjalizacja | Znaczenie |
|---------|-----|---------------|-----------|
| `L` | `int` | 0 | Bieżąca wartość zegara Lamporta. Rośnie przy tick() i update(). |
| `N` | `int` | 0 → n_procs | Liczba procesów. Do iteracji w `clock_w1_satisfied`. |
| `MY` | `int` | -1 → my_pid | Własny rank. Pomijany w pętli W1 (`k != MY`). |
| `last_seen[]` | `int*` | NULL → calloc | `last_seen[k]` = ts ostatniej odebranej wiadomości od procesu k. Zerowane przy `clock_init`. |

---

### Funkcja: `clock_init(n_procs, my_pid)` → `void`

- **Parametry:**
  - `n_procs` — liczba procesów MPI (z `MPI_Comm_size`).
  - `my_pid` — własny rank MPI (z `MPI_Comm_rank`).
- **Logika:**
  1. `free(last_seen)` — bezpieczne ponowne wywołanie.
  2. `last_seen = calloc(n_procs, sizeof(int))` — alokacja i zerowanie.
  3. `N = n_procs; MY = my_pid; L = 0`.
- **Skąd wołana:** `main()` raz przed pętlą główną.

---

### Funkcja: `clock_tick()` → `int`

- **Zwraca:** nową wartość zegara Lamporta po inkrementacji.
- **Logika:** `L += 1; return L`.
- **Semantyka:** zdarzenie lokalne lub wysłanie wiadomości. Zegar rośnie zanim wiadomość opuści proces.
- **Skąd wołana:**
  - `start_request()` — przed wysłaniem REQ (`MY_TS = clock_tick()`).
  - `handle_message()` dla MSG_REQ — przed wysłaniem ACK.
  - `section_phase()` — przed wysłaniem RELEASE.

---

### Funkcja: `clock_update(from, ts)` → `int`

- **Parametry:**
  - `from` — rank procesu, od którego odebrano wiadomość.
  - `ts` — znacznik Lamporta w odebranej wiadomości.
- **Zwraca:** nową wartość L po aktualizacji.
- **Logika:**
  1. `if (ts > L) L = ts;` — reguła Lamporta: bierzemy max.
  2. `L += 1;` — odbiór to zdarzenie, inkrementacja.
  3. `if (from >= 0 && from < N) last_seen[from] = ts;` — aktualizacja wektora (z ochroną przed out-of-bounds).
  4. `return L;`.
- **Skąd wołana:** `handle_message()` jako **pierwsze** działanie po odebraniu dowolnej wiadomości (REQ, ACK, RELEASE).

---

### Funkcja: `clock_get()` → `int`

- **Zwraca:** bieżącą wartość `L` bez modyfikacji.
- **Skąd wołana:** `log_state()` i `log_debug()` — do formatowania `[tX]` w logach.

---

### Funkcja: `clock_last_seen(pid)` → `int`

- **Parametry:** `pid` — rank procesu do zapytania.
- **Zwraca:** `last_seen[pid]`, lub 0 jeśli `pid` poza zakresem.
- **Skąd wołana:** nie używane bezpośrednio w logice głównej; dostępne dla potrzeb diagnostycznych.

---

### Funkcja: `clock_w1_satisfied(my_ts)` → `int`

- **Parametry:** `my_ts` — znacznik Lamporta własnego żądania.
- **Zwraca:** 1 (prawda) lub 0 (fałsz).
- **Logika:**
  ```
  for k in 0..N-1:
      if k == MY: pomiń
      if last_seen[k] <= my_ts: return 0  ← co najmniej jeden nie wie
  return 1  ← wszyscy mają wiadomość nowszą niż my_ts
  ```
- **Semantyka:** `last_seen[k] > my_ts` gwarantuje (przez FIFO i monotoniczność ts), że wiadomość od `k` z ts większym niż nasz REQ dotarła do nas już PO tym, jak `k` przetworzył nasz REQ. Czyli `k` zna nasze żądanie.
- **Skąd wołana:** `try_enter_section()` w `main.c`.

---

## src/queue.c / src/queue.h

**Cel modułu:** Posortowana lista jednokierunkowa reprezentująca kolejkę żądań Lamporta Q.

**Zależy od:** `types.h`, `<stdlib.h>`.

---

### Typ `queue_t`

```c
typedef struct {
    request_t *head;
    int size;
} queue_t;
```

| Pole | Znaczenie |
|------|-----------|
| `head` | Wskaźnik na pierwszy (o najwyższym priorytecie) element listy. NULL gdy pusta. |
| `size` | Liczba elementów. Używana pomocniczo. |

---

### Funkcja pomocnicza `req_less(ts_a, pid_a, ts_b, pid_b)` → `int` (statyczna)

- **Parametry:** dwie pary `(ts, pid)`.
- **Zwraca:** 1 jeśli `(ts_a,pid_a) < (ts_b,pid_b)` w sensie leksykograficznym.
- **Logika:** `if ts_a != ts_b: return ts_a < ts_b; return pid_a < pid_b`.
- **Cel:** jednoznaczny totalny porządek na żądaniach; unikanie duplikowania kodu porównania.

---

### Funkcja: `q_init(q)` → `void`

- **Parametry:** `q` — wskaźnik na `queue_t`.
- **Logika:** `q->head = NULL; q->size = 0`.
- **Skąd wołana:** `main()` raz przed pętlą główną.

---

### Funkcja: `q_destroy(q)` → `void`

- **Parametry:** `q` — wskaźnik na `queue_t`.
- **Logika:** iteruje listę `head → ... → NULL`, wywołuje `free()` na każdym węźle, na końcu zeruje `head` i `size`.
- **Skąd wołana:** `main()` po wyjściu z pętli głównej (technicznie nieosiągalne w tej implementacji, ale dla poprawności zarządzania pamięcią).

---

### Funkcja: `q_insert(q, ts, pid, m, dock)` → `void`

- **Parametry:**
  - `q` — kolejka.
  - `ts, pid` — klucz sortowania.
  - `m` — liczba mechaników.
  - `dock` — przypisany dok.
- **Logika krok po kroku:**
  1. Alokacja nowego węzła `malloc(sizeof(request_t))`.
  2. Wypełnienie pól: `ts, pid, m, dock, next=NULL`.
  3. Znalezienie miejsca wstawienia: iteracja `**link = &q->head` dopóki `*link` istnieje i `req_less((*link)->ts, (*link)->pid, ts, pid)` — tzn. przesuwamy się dopóki bieżący element jest mniejszy (wyższy priorytet) od nowego.
  4. **Ochrona przed duplikatem:** jeśli `*link` istnieje i ma tę samą parę `(ts, pid)` → `free(node); return`. Zapobiega zduplikowaniu własnego żądania (proces wstawia je lokalnie PRZED broadcastem REQ, więc gdyby odebrał „echo" nie wstawiałby duplikatu — ale REQ wysyłane jest tylko do innych, więc echo nie wystąpi; zabezpieczenie pro forma).
  5. `node->next = *link; *link = node; q->size += 1`.
- **Złożoność:** O(n) — n = rozmiar kolejki.
- **Skąd wołana:**
  - `start_request()` — własne żądanie.
  - `handle_message()` dla MSG_REQ — żądanie od innego procesu.

---

### Funkcja: `q_remove_by_pid(q, pid)` → `bool`

- **Parametry:**
  - `q` — kolejka.
  - `pid` — który proces usunąć.
- **Zwraca:** `true` jeśli usunięto, `false` jeśli nie znaleziono.
- **Logika:** iteracja z `**link`; gdy `(*link)->pid == pid`: wyciągnięcie węzła, `free`, `q->size -= 1`, `return true`.
- **Skąd wołana:**
  - `handle_message()` dla MSG_RELEASE — usunięcie żądania innego procesu.
  - `section_phase()` — usunięcie własnego żądania przed broadcastem RELEASE.

---

### Funkcja: `q_find_by_pid(q, pid)` → `request_t*`

- **Parametry:** `q`, `pid`.
- **Zwraca:** wskaźnik na znaleziony `request_t` lub `NULL`.
- **Logika:** prosta iteracja `head → ...`, porównanie `p->pid == pid`.
- **Skąd wołana:** `start_request()` — po `q_insert` własnego żądania, żeby zachować wskaźnik `MY_REQ`.

---

### Funkcja: `q_predecessors(q, my_req, *count, *m_sum)` → `void`

- **Parametry:**
  - `q, my_req` — kolejka i nasze żądanie.
  - `*count` — wyjście: liczba poprzedników.
  - `*m_sum` — wyjście: suma m poprzedników.
- **Logika:** iteracja po Q, zatrzymanie na `p == my_req`; dla każdego `p` z `req_less(p, my_req)` inkrementacja `c` i `s += p->m`.
- **Skąd wołana:** dostępna jako utility, w tej wersji używana głównie diagnostycznie. Logika W3 w `mechanics.c` ma własną pętlę (per-dok max zamiast prostej sumy).

---

## src/dock.c / src/dock.h

**Cel modułu:** Deterministyczna reguła przydziału doku oraz sprawdzenie warunku W2.

**Zależy od:** `queue.h`, `types.h`, `<stdbool.h>`.

---

### Funkcja pomocnicza `req_less(ts_a, pid_a, ts_b, pid_b)` → `int` (statyczna)

Identyczna z wersją w `queue.c`. Duplikacja celowa — moduły są niezależne.

---

### Funkcja: `dock_assign(ts, pid, K)` → `int`

- **Parametry:**
  - `ts` — znacznik Lamporta żądania.
  - `pid` — identyfikator procesu.
  - `K` — liczba doków.
- **Zwraca:** numer doku z zakresu `1..K`. Zwraca -1 dla `K <= 0`.
- **Logika:**
  1. `if (K <= 0) return -1`.
  2. `int s = ts + pid`.
  3. `if (s < 0) s = -s` — ochrona przed ujemnym wynikiem modulo (w C modulo liczby ujemnej może być ujemne; ts i pid są nieujemne, ale suma teoretycznie może przepełnić int — `abs` jako ostrożność).
  4. `return (s % K) + 1` — mapuje na `{1, 2, ..., K}`.
- **Własności:**
  - Deterministyczna: ta sama trójka `(ts, pid, K)` → ten sam dok zawsze.
  - Każdy proces znający `(ts, pid, K)` z treści REQ oblicza ten sam dok bez komunikacji.
  - Balansuje: różne `(ts+pid)` trafiają na różne doki, równomiernie przy dużej liczbie żądań.
- **Skąd wołana:** `start_request()` — raz przy tworzeniu żądania.

---

### Funkcja: `dock_w2_satisfied(q, my_req)` → `bool`

- **Parametry:**
  - `q` — lokalna kolejka żądań.
  - `my_req` — wskaźnik na własne żądanie w Q (nie może być NULL — sprawdzane przez wywołującego).
- **Zwraca:** `true` jeśli W2 spełniony (można wejść), `false` jeśli nie.
- **Logika krok po kroku:**
  1. Iteracja po Q od głowy do `my_req` (pętla zatrzymuje się gdy `p == my_req`).
  2. Dla każdego `p` (który jest kandydatem na poprzednika):
     - `if (!req_less(p->ts, p->pid, my_req->ts, my_req->pid)) continue` — jeśli `p` nie jest **ściśle mniejszy** niż `my_req` (wyższy priorytet), pomiń. (W posortowanej liście elementy przed `my_req` mogą mieć równy ts — choć nie taki sam `(ts,pid)` — ten check jest defensywny.)
     - `if (p->dock == my_req->dock) return false` — poprzednik z tym samym dokiem → W2 fałszywy.
  3. `return true` — brak blokującego poprzednika.
- **Semantyka:** Jeśli poprzednik z tym samym dokiem istnieje, **on wejdzie do tego doku wcześniej** (ma wyższy priorytet) i wyjdzie przed nami. Musimy poczekać aż wyśle RELEASE i zniknie z Q.
- **Skąd wołana:** `try_enter_section()` w `main.c`.

---

## src/mechanics.c / src/mechanics.h

**Cel modułu:** Sprawdzenie warunku W3 — czy jest wystarczająco mechaników dla naszego żądania z uwzględnieniem poprzedników.

**Zależy od:** `queue.h`, `types.h`, `<stdlib.h>`, `<string.h>`, `<stdbool.h>`.

---

### Funkcja pomocnicza `req_less_m(...)` (statyczna)

Identyczna z `req_less` z `queue.c` i `dock.c`. Lokalna kopia dla niezależności modułu.

---

### Funkcja: `mech_w3_satisfied(q, my_req, M, K)` → `bool`

- **Parametry:**
  - `q` — lokalna kolejka żądań.
  - `my_req` — wskaźnik na własne żądanie.
  - `M` — całkowita liczba mechaników (z argumentu programu).
  - `K` — liczba doków (do alokacji tablicy `dock_max`).
- **Zwraca:** `true` jeśli W3 spełniony, `false` jeśli nie. Zwraca `false` dla NULL/nieprawidłowych parametrów.
- **Logika krok po kroku:**
  1. Walidacja: `if (!q || !my_req || M <= 0 || K <= 0) return false`.
  2. `int *dock_max = calloc(K+1, sizeof(int))` — tablica indeksowana 1..K. Zerowana przez `calloc`. Dlaczego `K+1`: doki numerowane od 1, indeks 0 nieużywany, rozmiar K+1 pozwala na `dock_max[K]` bez przekroczenia.
  3. Pętla poprzedników:
     ```
     for p = q->head; p != my_req; p = p->next:
         if req_less_m(p->ts, p->pid, my_req->ts, my_req->pid):
             if p->dock in 1..K and p->m > dock_max[p->dock]:
                 dock_max[p->dock] = p->m
     ```
     — dla każdego poprzednika (ściśle mniejszy `(ts,pid)`) aktualizuje per-dok maksimum mechaników.
  4. `total = my_req->m`. Zliczanie: `for d=1..K: total += dock_max[d]`.
  5. `free(dock_max); return total <= M`.
- **Dlaczego per-dok MAX, nie surowa suma:**
  Warunek W2 gwarantuje że na danym doku jednocześnie może być co najwyżej 1 okręt w INSECTION. Spośród wszystkich poprzedników na doku `d`, realnie zasoby mechaników zajmie tylko **jeden** (ten, który wejdzie jako pierwszy spośród poprzedników na doku `d`). Surowę sumowanie m wszystkich poprzedników na tym samym doku zawyżałoby pesymistyczną zajętość i blokowało wejście mimo realnie dostępnych mechaników.
- **Skąd wołana:** `try_enter_section()` w `main.c`, trzecia (ostatnia) w łańcuchu krótkich spięć W1→W2→W3.

---

## src/comm.c / src/comm.h

**Cel modułu:** Cała komunikacja MPI — wysyłanie i odbieranie wiadomości. Ukrywa szczegóły MPI za prostym API.

**Zależy od:** `types.h`, `<mpi.h>`, `<stdbool.h>`.

---

### Zmienna statyczna `WIRE_T`

```c
static MPI_Datatype WIRE_T = MPI_DATATYPE_NULL;
```

Zarejestrowany typ MPI dla `wire_msg_t` (4 ints). Inicjalizowany **leniwie** przy pierwszym wywołaniu `ensure_type()`.

---

### Funkcja pomocnicza `ensure_type()` → `void` (statyczna)

- **Logika:** Jeśli `WIRE_T != MPI_DATATYPE_NULL` — już zainicjalizowany, wyjdź. Inaczej:
  1. Tworzenie tablicy `blocklens[4]={1,1,1,1}`, `types[4]={MPI_INT,MPI_INT,MPI_INT,MPI_INT}`.
  2. `MPI_Get_address` dla każdego pola `wire_msg_t` — oblicza offsety byte-owe niezależnie od paddingu kompilatora. Przenośne między architekturami.
  3. `MPI_Type_create_struct(4, ...)` + `MPI_Type_commit(&WIRE_T)`.
- **Cel:** rejestracja niestandardowego typu MPI umożliwia przesyłanie struktury w jednej operacji MPI_Send/MPI_Recv zamiast 4 osobnych.

---

### Funkcja pomocnicza `send_one(m, to, tag)` → `void` (statyczna)

- **Parametry:**
  - `m` — wskaźnik na `wire_msg_t` do wysłania.
  - `to` — rank odbiorcy.
  - `tag` — typ wiadomości (`MSG_REQ/ACK/RELEASE`).
- **Logika:** `ensure_type(); MPI_Send(m, 1, WIRE_T, to, tag, MPI_COMM_WORLD)`.
- **Uwaga:** `MPI_Send` jest blokujące — zwraca gdy bufor można reużyć (nie gdy wiadomość dotarła do odbiorcy).

---

### Funkcja: `comm_send_req_all(ts, my_pid, m, dock, n_procs)` → `void`

- **Parametry:** `ts` — znacznik, `my_pid` — nadawca, `m` — mechanicy, `dock` — dok, `n_procs` — N.
- **Logika:** tworzy `wire_msg_t = {ts, my_pid, m, dock}`, wysyła `send_one` do każdego `p ∈ [0..n_procs-1]` poza `my_pid`. Łącznie `N-1` wiadomości.
- **Tag:** `MSG_REQ`.
- **Skąd wołana:** `start_request()` na końcu, po ustawieniu `state=TRYING`.

---

### Funkcja: `comm_send_ack(ts, my_pid, to)` → `void`

- **Parametry:** `ts` — znacznik, `my_pid` — nadawca, `to` — odbiorca.
- **Logika:** `wire_msg_t = {ts, my_pid, 0, 0}; send_one(..., MSG_ACK)`.
- **Uwaga:** pola `m=0, dock=0` — odbiorca ACK nie używa tych pól.
- **Skąd wołana:** `handle_message()` w przypadku `MSG_REQ`, po `q_insert` i `clock_tick()`.

---

### Funkcja: `comm_send_release_all(ts, my_pid, n_procs)` → `void`

- **Parametry:** `ts` — znacznik, `my_pid` — nadawca, `n_procs` — N.
- **Logika:** `wire_msg_t = {ts, my_pid, 0, 0}`, wysyła do wszystkich poza `my_pid`. Łącznie `N-1` wiadomości.
- **Tag:** `MSG_RELEASE`.
- **Skąd wołana:** `section_phase()` po usunięciu własnego żądania z Q.

---

### Funkcja: `comm_try_recv(tag, from, msg)` → `bool`

- **Parametry (wyjściowe):** `*tag` — tag odebranej wiadomości, `*from` — rank nadawcy, `*msg` — treść wiadomości.
- **Zwraca:** `true` i wypełnia parametry wyjściowe jeśli coś czekało w skrzynce. `false` jeśli skrzynka pusta.
- **Logika:**
  1. `ensure_type()`.
  2. `MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, ..., &flag, &st)` — **nieblokujące** sprawdzenie. Jeśli `!flag` → `return false`.
  3. `MPI_Recv(msg, 1, WIRE_T, st.MPI_SOURCE, st.MPI_TAG, ..., MPI_STATUS_IGNORE)` — odbiór wiadomości, której obecność właśnie potwierdzono.
  4. Wypełnienie `*tag` i `*from` z `st`.
- **Dlaczego `MPI_Iprobe` przed `MPI_Recv`:** pozwala na nieblokujący polling. Gdyby użyć `MPI_Recv` bezpośrednio z `MPI_ANY_SOURCE`, proces zablokuje się do momentu odebrania — nie możemy wtedy wykonywać nic innego.
- **Skąd wołana:** `drain_inbox()` w pętli `while (comm_try_recv(...))`.

---

## src/log.c / src/log.h

**Cel modułu:** Logowanie zmian stanu z prefiksem `[pid] [tX]`. Obsługa trybu DEBUG.

**Zależy od:** `clock.h`, `<stdarg.h>`, `<stdio.h>`.

---

### Zmienna statyczna

| Zmienna | Typ | Znaczenie |
|---------|-----|-----------|
| `MY` | `int` | Własny rank procesu. Ustawiany przez `log_init`. Domyślnie -1. |

---

### Funkcja: `log_init(my_pid)` → `void`

- **Parametry:** `my_pid` — rank MPI.
- **Logika:** `MY = my_pid`.
- **Skąd wołana:** `main()` raz przy inicjalizacji.

---

### Funkcja: `log_state(fmt, ...)` → `void`

- **Parametry:** `fmt` — format printf, `...` — variadic.
- **Zawsze drukuje** (niezależnie od flagi DEBUG).
- **Format wyjściowy:** `[MY] [tX] <treść>\n` gdzie `X = clock_get()`.
- **Logika:** `fprintf(stdout, "[%d] [t%d] ", MY, clock_get()); vfprintf(stdout, fmt, ap); fputc('\n'); fflush(stdout)`.
- **`fflush`:** wymagane przy MPI — buforowanie stdio może spowodować utratę logów przy przerwaniu programu.
- **Skąd wołana:**
  - `rest_phase()` — "Wracam z walki".
  - `start_request()` — "Rozpoczynam staranie".
  - `try_enter_section()` — "Wszedłem do doku X, mam Y mechaników".
  - `section_phase()` — "Zwalniam dok X i Y mechaników".

---

### Funkcja: `log_debug(fmt, ...)` → `void`

- **Drukuje tylko z `-DDEBUG`** (flaga kompilacji).
- **Format:** `[MY] [t%d] (dbg) <treść>\n`.
- **W trybie release:** statyczny inline no-op — kompilator usuwa wywołania całkowicie, zero overhead.
- **Skąd wołana:** `handle_message()` — logi odbioru REQ/ACK/RELEASE i wysyłania ACK.

---

## src/main.c

**Cel modułu:** Główny automat stanów. Parsowanie argumentów, inicjalizacja MPI i modułów, nieskończona pętla `REST→TRYING→INSECTION→REST`.

**Zależy od:** wszystkich modułów: `clock`, `comm`, `dock`, `log`, `mechanics`, `queue`, `types`.

---

### Zmienne statyczne modułu (runtime config)

| Zmienna | Typ | Źródło | Znaczenie |
|---------|-----|--------|-----------|
| `N` | `int` | `MPI_Comm_size` | Całkowita liczba procesów MPI. |
| `K` | `int` | `argv[1]` | Liczba doków (1..K). |
| `M` | `int` | `argv[2]` | Liczba mechaników. |
| `MY` | `int` | `MPI_Comm_rank` | Własny rank MPI (0..N-1). |

---

### Zmienne stanu bieżącej rundy

| Zmienna | Typ | Stan w REST | Stan w TRYING/INSECTION | Znaczenie |
|---------|-----|-------------|------------------------|-----------|
| `state` | `state_t` | `STATE_REST` | `STATE_TRYING` lub `STATE_INSECTION` | Bieżący stan automatu. |
| `Q` | `queue_t` | może zawierać żądania innych | zawiera nasze + innych | Lokalna kopia kolejki żądań Lamporta. |
| `MY_REQ` | `request_t*` | `NULL` | wskaźnik do wpisu w Q | Wskaźnik na własne żądanie. NULL gdy REST. |
| `MY_TS` | `int` | 0 | znacznik Lamporta żądania | Kiedy wysłaliśmy REQ. |
| `MY_M` | `int` | 0 | 1..M | Ile mechaników żądamy w tej rundzie. |
| `MY_DOCK` | `int` | 0 | 1..K | Któremu dokowi odpowiada to żądanie. |

---

### Funkcja: `handle_message(tag, from, msg)` → `void` (statyczna)

- **Parametry:** `tag` — typ wiadomości, `from` — nadawca, `msg` — treść.
- **Logika (centralna obsługa jednej wiadomości):**
  1. `clock_update(from, msg->ts)` — **ZAWSZE PIERWSZE**. Aktualizacja zegara i `last_seen`.
  2. Switch po `tag`:
     - `MSG_REQ`: `q_insert(&Q, msg->ts, msg->pid, msg->m, msg->dock)`. `ack_ts = clock_tick()`. `comm_send_ack(ack_ts, MY, from)`.
     - `MSG_ACK`: brak akcji poza zegarem (wykonanym w kroku 1).
     - `MSG_RELEASE`: `q_remove_by_pid(&Q, msg->pid)`.
- **Skąd wołana:** `drain_inbox()` dla każdej odebranej wiadomości.

---

### Funkcja: `try_enter_section()` → `void` (statyczna)

- **Logika:**
  1. `if (state != STATE_TRYING) return` — strażnik.
  2. `if (!MY_REQ) return` — strażnik.
  3. `if (!clock_w1_satisfied(MY_TS)) return` — krótkie spięcie: W1 najszybsze.
  4. `if (!dock_w2_satisfied(&Q, MY_REQ)) return` — W2.
  5. `if (!mech_w3_satisfied(&Q, MY_REQ, M, K)) return` — W3 najkosztowniejsze obliczeniowo.
  6. `state = STATE_INSECTION`. `log_state("Wszedłem do doku %d, mam %d mechaników", MY_DOCK, MY_M)`.
- **Skąd wołana:** `trying_phase()` po `drain_inbox()` w każdej iteracji.

---

### Funkcja: `drain_inbox()` → `void` (statyczna)

- **Logika:** `while (comm_try_recv(&tag, &from, &msg)) { handle_message(tag, from, &msg); }`.
- **Cel:** opróżnienie całej skrzynki wiadomości w jednym przebiegu. Nie blokuje — wraca gdy skrzynka pusta.
- **Skąd wołana:** `rest_phase()`, `trying_phase()`, `section_phase()` — w każdej fazie, regularnie.

---

### Funkcja: `rest_phase()` → `void` (statyczna)

- **Logika:** `log_state("Wracam z walki")`. Pętla 20 razy: `drain_inbox(); usleep(50ms)`. Łącznie ~1 sekunda symulowanego odpoczynku/walki.
- **Cel:** symulacja czasu między wejściami do bazy. `drain_inbox()` w każdej iteracji — okręt nie może przestać odpowiadać na wiadomości innych nawet w REST.

---

### Funkcja: `start_request()` → `void` (statyczna)

- **Logika (sekwencja — kolejność krytyczna):**
  1. `MY_M = (rand() % M) + 1` — losowanie 1..M.
  2. `MY_TS = clock_tick()` — inkrementacja zegara, ts żądania.
  3. `MY_DOCK = dock_assign(MY_TS, MY, K)` — deterministyczny dok.
  4. `q_insert(&Q, MY_TS, MY, MY_M, MY_DOCK)` — własne żądanie do lokalnej Q.
  5. `MY_REQ = q_find_by_pid(&Q, MY)` — zachowanie wskaźnika.
  6. `state = STATE_TRYING`.
  7. `log_state(...)`.
  8. `comm_send_req_all(MY_TS, MY, MY_M, MY_DOCK, N)` — broadcast do N-1 procesów.
- **Kolejność 4 przed 8:** własne żądanie jest w Q zanim REQ dotrze do innych. Gdyby było odwrotnie — inny mógłby odebrać RELEASE zanim w ogóle mamy MY_REQ.

---

### Funkcja: `trying_phase()` → `void` (statyczna)

- **Logika:**
  ```
  while state == STATE_TRYING:
      drain_inbox()
      try_enter_section()
      if state != STATE_TRYING: break
      usleep(5ms)
  ```
- **`usleep(5ms)`:** uniknięcie busy-wait 100% CPU. Krótki sleep — kompromis między latencją wejścia do sekcji a zużyciem procesora.

---

### Funkcja: `section_phase()` → `void` (statyczna)

- **Logika:**
  1. Losowanie czasu naprawy: `repair_ms = rand() % 200 + 50` (50..249 ms).
  2. Pętla symulacji: co 10ms `drain_inbox(); usleep(10ms); elapsed += 10`. **Kluczowe:** `drain_inbox()` w każdej iteracji — okręt w sekcji nadal odpowiada ACK na REQ, nie blokuje innych w TRYING.
  3. Po naprawie:
     - `dock_done = MY_DOCK; mech_done = MY_M` (zachowanie dla logu).
     - `q_remove_by_pid(&Q, MY)` — usunięcie własnego żądania.
     - `MY_REQ = NULL`.
     - `rel_ts = clock_tick()`.
     - `log_state("Zwalniam dok %d i %d mechaników", dock_done, mech_done)`.
     - `comm_send_release_all(rel_ts, MY, N)`.
     - `state = STATE_REST`.

---

### Funkcja: `main(argc, argv)` → `int`

- **Argumenty programu:**
  - `argv[1]` = K (liczba doków), obowiązkowy, `K > 0`.
  - `argv[2]` = M (liczba mechaników), obowiązkowy, `M > 0`.
  - `argv[3]` = seed (opcjonalny). Domyślnie `time(NULL) ^ MY` — różne seedy dla różnych procesów przy tym samym globalnym seedzie.
- **Logika:**
  1. `MPI_Init`, `MPI_Comm_size → N`, `MPI_Comm_rank → MY`.
  2. Walidacja `argc >= 3`, parsowanie K, M, seed, `K>0, M>0`.
  3. `srand(seed)`.
  4. `clock_init(N, MY)`, `log_init(MY)`, `q_init(&Q)`.
  5. Pętla nieskończona: `rest_phase(); start_request(); trying_phase(); section_phase()`.
  6. (Po pętli, technicznie nieosiągalne) `q_destroy(&Q); MPI_Finalize()`.

---

## Makefile

**Cel:** Kompilacja projektu przez `mpicc`.

```
make          → kompilacja release: -Wall -Wextra -O2 -std=c11
make debug    → kompilacja debug: + -DDEBUG -g -O0
make clean    → usuwa *.o i binar 'okrety'
```

- Binar: `./okrety`.
- Źródła: `src/main.c src/clock.c src/queue.c src/dock.c src/mechanics.c src/comm.c src/log.c`.
- `-DDEBUG` aktywuje `log_debug()` — dodatkowe logi per-wiadomość (odbiory, wysyłania ACK).

---

## scripts/run.sh

**Cel:** Wrapper uruchomieniowy obsługujący dwa scenariusze: lokalne (bez pliku `hosts`) i wielomaszynowe (z plikiem `hosts`).

**Wywołanie:** `./scripts/run.sh <N> <K> <M> [seed]`

**Logika:**
1. Jeśli brak `hosts` — kompilacja + `mpirun -np N ./okrety K M [seed]`. Proste.
2. Jeśli `hosts` istnieje — kompilacja, odczyt lokalnych IP (`ip addr`), generacja pliku `appfile` dla `mpirun --app`.
3. `appfile` formatu: `-H <host> -np <slots> <binary> <args>`. Dla hosta lokalnego używa `./okrety`, dla zdalnego używa `REMOTE_BIN` (hardcoded ścieżka na macOS: `/Users/kamilek/Desktop/misiaki/Misiaki/okrety`).
4. `mpirun --app "$APPFILE"` — uruchomienie wielomaszynowe.

---

## scripts/verify_logs.py

**Cel:** Automatyczna weryfikacja niezmienników I1 i I2 na podstawie logów z `stdout` programu.

**Wywołanie:** `python3 scripts/verify_logs.py <plik_logów> <M>`

**Niezmienniki sprawdzane:**
- **I1:** W każdej chwili na każdym doku jest ≤ 1 okręt.
- **I2:** W każdej chwili suma m aktywnych okrętów ≤ M.

**Logika:**
1. Parsowanie logów regexpami:
   - `ENTER`: linia `[pid] [tX] Wszedłem do doku D, mam Y mechaników`.
   - `LEAVE`: linia `[pid] [tX] Zwalniam dok D i Y mechaników`.
2. Sortowanie zdarzeń po `(ts, kind_priority, pid)` — LEAVE (kind_priority=0) przed ENTER (=1) przy tym samym ts. Dlaczego: zwolnienie zasobu musi zostać przetworzone przed nową occupancją przy równym timestamp.
3. Symulacja stanu: `docks: dict[dok→pid]` i `mech_per_pid: dict[pid→m]`.
4. Dla każdego zdarzenia ENTER: sprawdzenie `dok not in docks` (I1) i `sum(mech_per_pid.values()) <= M` (I2).
5. Dla LEAVE: sprawdzenie `docks[d] == pid` (poprawny właściciel zwalnia).
6. Wynik: lista naruszeń lub "OK: wszystkie niezmienniki spełnione".
