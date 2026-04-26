# Plan Projektu: Rozproszony System Dostępu do Doków i Mechaników

## Opis Scenariusza

N okrętów wraca z walk i ubiega się o dostęp do **K rozróżnialnych doków** oraz **M nierozróżnialnych mechaników**. Mechanicy są potrzebni w różnych ilościach, zależnie od stopnia zniszczeń okrętu. Doki powinny być wybierane deterministycznie, aby równoważyć ich obciążenie.

---

## 1. Cele i Założenia Projektowe

### 1.1 Środowisko

- Pełne asynchroniczność komunikacji
- Kanały niezawodne i FIFO
- Procesy nie ulegają awarii
- Komunikacja tylko przez `MPI_Send` / `MPI_Recv` (lub asynchroniczne odpowiedniki)
- Funkcje grupowe/synchronizacyjne dozwolone **wyłącznie** do inicjalizacji

### 1.2 Zasoby

| Zasób | Typ | Pojemność |
|-------|-----|-----------|
| Doki | Rozróżnialne (K sztuk) | 1 okręt na dok w danej chwili |
| Mechanicy | Nierozróżnialni (M sztuk) | Sekcja krytyczna pojemności M (ale okręt może potrzebować wielu) |

### 1.3 Wymagania algorytmiczne

- **Brak centralnego zarządcy** — pełne rozproszenie
- **Brak głodzenia** procesów
- **Minimalna złożoność czasowa** (priorytet) i komunikacyjna
- Determinizm przy wyborze doku (równoważenie obciążenia)
- Możliwość konfigurowania N, K, M przez parametry uruchomieniowe

---

## 2. Koncepcja Algorytmu

### 2.1 Podział zasobów

#### Doki (rozróżnialne, K sztuk)
Każdy dok to odrębna sekcja krytyczna o pojemności 1. Do wyboru doku stosujemy **zmodyfikowany algorytm Ricarta-Agrawali** z deterministyczną regułą przydziału:

> Okręt z danym priorytetem (zegar Lamporta) deterministycznie wybiera dok o najniższym aktualnym obciążeniu. Obciążenie doku wyznaczane jest na podstawie znanych lokalnie informacji o przydziałach.

Dopuszczalne jest, że okręt czeka na dok `i`, mimo że doki `1..(i-1)` są puste — jeśli wynika to z reguły deterministycznej.

#### Mechanicy (nierozróżnialni, M sztuk)
Mechanicy stanowią sekcję krytyczną o pojemności `r`, gdzie `r` to liczba mechaników potrzebnych danemu okrętowi (1 ≤ r ≤ M). Stosujemy **uogólniony algorytm Ricarta-Agrawali z pojemnością M**.

### 2.2 Algorytm — opis ogólny

**Podstawa:** Ricart-Agrawala z zegarem Lamporta jako priorytetem i identyfikatorem procesu jako tiebreakerem.

**Fazy działania okrętu:**
1. **IDLE** — okręt walczy/odpoczywa
2. **REQUESTING** — okręt ubiega się o dok i mechaników
3. **IN_DOCK** — okręt w sekcji krytycznej (dok + mechanicy)
4. **RELEASING** — zwolnienie zasobów

### 2.3 Opis stanów

```
BEGIN:
  Stan początkowy. Okręt losuje czas walki i zapotrzebowanie na mechaników (r).

IDLE:
  Okręt walczy. Po zakończeniu walki przechodzi do REQUESTING.

REQUESTING:
  Okręt wysyła REQ_DOCK oraz REQ_MECH do wszystkich innych procesów.
  Czeka na ACK od wszystkich procesów dla obu zasobów.
  Po zebraniu wszystkich ACK przechodzi do IN_DOCK.

IN_DOCK:
  Okręt naprawia się przez losowy czas.
  Loguje: identyfikator + zegar Lamporta + numer doku + liczba mechaników.
  Po zakończeniu naprawy przechodzi do RELEASING.

RELEASING:
  Wysyła REL_DOCK i REL_MECH do wszystkich.
  Odpowiada odroczonymi ACK z kolejki oczekujących.
  Przechodzi do IDLE.
```

### 2.4 Typy wiadomości

| Typ | Zawartość | Opis |
|-----|-----------|------|
| `REQ_DOCK` | `(ts, pid, dock_id)` | Żądanie dostępu do doku o wybranym id |
| `REQ_MECH` | `(ts, pid, r)` | Żądanie r mechaników |
| `ACK_DOCK` | `(ts, pid)` | Zgoda na dostęp do doku |
| `ACK_MECH` | `(ts, pid)` | Zgoda na dostęp do mechaników |
| `REL_DOCK` | `(ts, pid, dock_id)` | Zwolnienie doku |
| `REL_MECH` | `(ts, pid)` | Zwolnienie mechaników |

### 2.5 Szczegółowy opis algorytmu — obsługa wiadomości

#### Stan REQUESTING (ubiega się o dok i-ty oraz r mechaników)

```
case REQ_DOCK(ts_j, pid_j, dock_j):
  Aktualizuj zegar Lamporta.
  Jeżeli nie ubiegamy się o dok LUB nasz priorytet (ts, pid) > (ts_j, pid_j):
    wyślij ACK_DOCK do pid_j
  W przeciwnym razie:
    dodaj pid_j do kolejki oczekujących na dok (defer_dock[])

case REQ_MECH(ts_j, pid_j, r_j):
  Aktualizuj zegar Lamporta.
  Niech suma = suma mechaników procesów z wyższym priorytetem niż my + r_j
  Jeżeli nie ubiegamy się o mech LUB nasz priorytet > (ts_j, pid_j):
    wyślij ACK_MECH do pid_j
  W przeciwnym razie:
    dodaj pid_j do kolejki oczekujących na mech (defer_mech[])

case ACK_DOCK(ts_j, pid_j):
  Aktualizuj zegar. ack_dock++.
  Jeżeli ack_dock == N-1: sekcja doku zdobyta.

case ACK_MECH(ts_j, pid_j):
  Aktualizuj zegar. ack_mech++.
  Jeżeli ack_mech == N-1: sekcja mechaników zdobyta.

case REL_DOCK / REL_MECH:
  Aktualizuj zegar. Usuń z lokalnej wiedzy o zajętości.

Przejście do IN_DOCK gdy: sekcja doku zdobyta AND sekcja mechaników zdobyta.
```

#### Stan IN_DOCK

```
Kontynuuj odbieranie i obsługę REQ (odpowiadaj ACK na bieżąco lub odkładaj).
Po zakończeniu naprawy: przejdź do RELEASING.
```

#### Stan RELEASING

```
Wyślij REL_DOCK do wszystkich.
Wyślij REL_MECH do wszystkich.
Dla każdego pid w defer_dock[]: wyślij ACK_DOCK.
Dla każdego pid w defer_mech[]: wyślij ACK_MECH.
Wyczyść kolejki. Przejdź do IDLE.
```

### 2.6 Wybór doku — reguła deterministyczna

Zamiast losowania, okręt wybiera dok na podstawie **lokalnie znanych informacji o historii przydziałów**:

- Każdy proces utrzymuje wektor `dock_usage[K]` — liczba wizyt każdego doku.
- Wektor aktualizowany jest przez wiadomości `REL_DOCK` (każde zwolnienie informuje wszystkich).
- Okręt ubiegający się o dok wybiera dok z **minimalnym `dock_usage[i]`**, a w przypadku remisu — dok o najniższym indeksie.

> W ten sposób wybór jest deterministyczny i globalnie spójny, bo wszystkie procesy mają te same informacje (kanały FIFO).

---

## 3. Złożoność

| Metryka | Wartość |
|---------|---------|
| Złożoność czasowa (dok) | 2 rundy |
| Złożoność czasowa (mech) | 2 rundy |
| Złożoność komunikacyjna (dok) | 2(N-1) wiadomości |
| Złożoność komunikacyjna (mech) | 2(N-1) wiadomości |
| Łącznie na wejście do sekcji | ~4(N-1) |

Optymalizacja Ricarta-Agrawali: jeśli dwa procesy jednocześnie wysłały REQ i wiemy o wyższym priorytecie przeciwnika — nie musimy czekać na jego ACK. Redukuje to liczbę wiadomości w typowych przypadkach.

---

## 4. Struktura Plików Projektu

```
projekt/
├── src/
│   ├── main.c              # Punkt wejścia, inicjalizacja MPI, pętla główna
│   ├── state.c / state.h   # Definicje stanów, przejścia między stanami
│   ├── lamport.c / .h      # Zegar Lamporta, aktualizacja
│   ├── dock.c / .h         # Logika sekcji krytycznej dla doków
│   ├── mechanic.c / .h     # Logika sekcji krytycznej dla mechaników
│   ├── messages.c / .h     # Definicje i obsługa typów wiadomości, send/recv
│   └── logger.c / .h       # Logowanie z [pid][ts], makro DEBUG
├── include/
│   └── config.h            # Stałe: N, K, M, czasy, DEBUG flag
├── Makefile
├── run.sh                  # Skrypt uruchamiający mpirun z parametrami
├── sprawozdanie/
│   └── algorytm.md         # Opis algorytmu wg szablonu prowadzącego
└── README.md
```

---

## 5. Implementacja — Kluczowe Elementy Kodu

### 5.1 Struktura procesu (C + MPI)

```c
// config.h
#define N 5        // liczba okrętów (procesów)
#define K 3        // liczba doków
#define M 4        // liczba mechaników
#define DEBUG 0    // 1 = szczegółowe logi

// messages.h
typedef struct {
    int ts;        // znacznik czasowy Lamporta
    int pid;       // identyfikator nadawcy
    int extra;     // dock_id lub liczba mechaników r
} Message;

#define TAG_REQ_DOCK 1
#define TAG_REQ_MECH 2
#define TAG_ACK_DOCK 3
#define TAG_ACK_MECH 4
#define TAG_REL_DOCK 5
#define TAG_REL_MECH 6
```

### 5.2 Zegar Lamporta

```c
// lamport.h
int lamport_clock = 0;

void lamport_send()    { lamport_clock++; }
void lamport_recv(int received_ts) {
    lamport_clock = (received_ts > lamport_clock ? received_ts : lamport_clock) + 1;
}
```

### 5.3 Pętla główna

```c
// main.c (pseudokod)
while (1) {
    // IDLE: losuj czas walki i zapotrzebowanie
    sleep(rand_time());
    int r = rand_range(1, M);  // potrzebne mechaniki

    // REQUESTING
    choose_dock();              // deterministyczny wybór doku
    send_req_dock_all();
    send_req_mech_all(r);
    wait_for_acks();            // nieblokujące — w pętli odbierania

    // IN_DOCK
    log_entry();
    sleep(repair_time());
    log_exit();

    // RELEASING
    send_rel_all();
    flush_deferred_acks();
}
```

### 5.4 Wątek/pętla odbioru wiadomości

Ponieważ MPI jest używane w trybie synchronicznym z `MPI_Recv`, proces musi być w stanie odbierać wiadomości **nawet będąc w sekcji krytycznej** (odpowiada na REQ innych procesów). Zalecana architektura: **jeden wątek główny** z nieblokującą pętlą przy użyciu `MPI_Irecv` lub `MPI_Probe` + `MPI_Recv`, albo dwa wątki (główny + odbiorczy) z synchronizacją przez mutex.

Dla prostoty w C+MPI — zalecamy podejście z `MPI_Irecv` i regularnym sprawdzaniem `MPI_Test`.

---

## 6. Logowanie

Format logowania (obowiązkowy):

```
[pid] [tXXXXXX] Śpię (walczę)
[pid] [tXXXXXX] Ubiegam się o dok D i R mechaników
[pid] [tXXXXXX] Wszedłem do doku D z R mechanikami
[pid] [tXXXXXX] Opuszczam dok D, zwalniam R mechaników
```

Przykład:
```
[2] [t000042] Śpię
[2] [t000043] Ubiegam się o dok 1 i 2 mechaników
[2] [t000089] Wszedłem do doku 1 z 2 mechanikami
[2] [t000112] Opuszczam dok 1, zwalniam 2 mechaników
```

Makro DEBUG (opcjonalne, do wyłączenia przy kompilacji):
```c
#if DEBUG
  #define LOG_DEBUG(fmt, ...) printf("[%d] [t%06d] DEBUG: " fmt "\n", my_pid, lamport_clock, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(fmt, ...)
#endif
```

---

## 7. Uruchomienie i Testowanie

### 7.1 Kompilacja i uruchomienie

```bash
# Kompilacja
make

# Uruchomienie lokalnie (5 procesów)
mpirun -np 5 ./projekt

# Uruchomienie na dwóch maszynach (wymagane na obronie)
mpirun -np 5 --hostfile hosts.txt ./projekt
```

`hosts.txt`:
```
maszyna1 slots=3
maszyna2 slots=2
```

### 7.2 Parametryzacja

Program powinien przyjmować N, K, M jako argumenty wiersza poleceń lub przez `config.h`:

```bash
mpirun -np 5 ./projekt 3 4   # K=3 doki, M=4 mechanicy
```

### 7.3 Scenariusze testowe

| Test | N | K | M | Cel |
|------|---|---|---|-----|
| Podstawowy | 3 | 2 | 3 | Weryfikacja poprawności |
| Duże N | 8 | 3 | 5 | Wydajność i brak głodzenia |
| M=1 | 4 | 2 | 1 | Graniczny przypadek mechaników |
| K=1 | 4 | 1 | 4 | Graniczny przypadek doku |
| N=K | 3 | 3 | 6 | Każdy okręt może dostać swój dok |

---

## 8. Sprawozdanie — Struktura

Sprawozdanie (plik `sprawozdanie/algorytm.md`) powinno zawierać — zgodnie z szablonem prowadzącego:

1. **Opis ogólny** — schemat działania w 3-5 zdaniach
2. **Stany procesów** — lista stanów z opisem
3. **Typy wiadomości** — lista z zawartością każdej wiadomości
4. **Opis szczegółowy** — dla każdego stanu: reakcja na każdy typ wiadomości (numerowane linie!)
5. **Dowód poprawności** (opcjonalnie) — brak głodzenia, wzajemne wykluczenie
6. **Złożoność** — czasowa i komunikacyjna

> Opis musi być na tyle kompletny, by **dowolna osoba mogła zaimplementować algorytm** bez dodatkowych wyjaśnień. Linie numerowane.

---

## 9. Plan Pracy (Timeline)

| Etap | Zawartość | Priorytet |
|------|-----------|-----------|
| 1. Algorytm | Opracowanie i prezentacja prowadzącemu | 🔴 Krytyczny |
| 2. Poprawki | Naniesienie uwag prowadzącego | 🔴 Krytyczny |
| 3. Szkielet kodu | Inicjalizacja MPI, zegar Lamporta, logowanie | 🟠 Wysoki |
| 4. Sekcja dok | Implementacja REQ/ACK/REL dla doków | 🟠 Wysoki |
| 5. Sekcja mech | Implementacja sekcji mechaników z pojemnością | 🟠 Wysoki |
| 6. Integracja | Połączenie obu sekcji, wybór doku | 🟡 Średni |
| 7. Testy | Scenariusze testowe, weryfikacja braku głodzenia | 🟡 Średni |
| 8. Sprawozdanie | Finalizacja dokumentu algorytmu | 🟡 Średni |
| 9. Obrona | Uruchomienie na 2 maszynach, pytania | 🔴 Krytyczny |

---

## 10. Kwestie Do Rozstrzygnięcia i Pułapki

### Pułapki implementacyjne
- **Zakleszczenie** przy jednoczesnym ubieganiu się o dok i mechaników — należy upewnić się, że kolejność wysyłania REQ jest spójna lub oba REQ wysyłane jednocześnie przed oczekiwaniem na ACK.
- **Odroczone ACK** muszą być wysyłane **po** wyjściu z sekcji, nie przed.
- **Wątek odbiorczy** — proces musi odpowiadać na wiadomości także w trakcie przebywania w sekcji krytycznej.
- **Aktualizacja dock_usage** — każdy REL_DOCK musi być rozsyłany do wszystkich, żeby wektor był spójny.

### Kwestie do decyzji zespołu
- Czy ubiegamy się o dok i mechaników **jednocześnie** (wysyłamy oba REQ naraz) czy **sekwencyjnie** (najpierw dok, potem mechanicy)? Równoczesne jest wydajniejsze, ale trudniejsze do zaimplementowania.
- Ile wątków? Jeden wątek z `MPI_Irecv` vs. dwa wątki z mutexem.
- Język implementacji: C + MPI (rekomendowany i wspierany przez prowadzących).

