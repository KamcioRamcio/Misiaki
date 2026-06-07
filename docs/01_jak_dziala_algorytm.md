# Jak działa algorytm — przewodnik dydaktyczny

Dokument tłumaczy algorytm **Lamport-Dual** krok po kroku. Przeznaczony dla autora — do nauki przed obroną. Bez kodu C, za to z diagramami i przykładami numerycznymi.

---

## 1. Po co ten algorytm istnieje

Mamy `N` okrętów (procesów MPI). Każdy co pewien czas wraca z walki i potrzebuje dwóch zasobów jednocześnie:

- **Dok** — jeden z `K` **rozróżnialnych** miejsc cumowania. Doki się różnią, nie można zastąpić doku 3 dokiem 1.
- **Mechanicy** — pewna liczba `m_S` z puli `M` **nierozróżnialnych** specjalistów. Nie obchodzi nas, który mechanik, tylko czy jest ich wystarczająco dużo.

### Dlaczego nie wolno zarządcy ani licznika

Wymagania kursu zakazują:
- **Centralnego zarządcy** — jeden proces decydujący, kto dostaje zasoby (single point of failure i centralny wąskie gardło).
- **Licznika strzeżonego zmienną krytyczną pojemności 1** — np. „zdobądź mutex, potem zmień licznik mechaników". Zakazane, bo redukuje sekcję pojemności M do sekwencji przez pojemność 1 — cała korzyść z równoległości ginie.
- **M osobnych sekcji pojemności 1** (po jednej per mechanik) — zakazane, bo tworzy M niezależnych zasobów z arbitralnym przydziałem.

Dozwolone: **jedna uogólniona sekcja krytyczna pojemności M** + **determinizm w przydziale doków**.

---

## 2. Intuicja w pięciu zdaniach

1. Każdy okręt trzyma lokalnie **jedną posortowaną kolejkę żądań Q** zawierającą żądania wszystkich procesów (aktualnie ubiegających się).
2. Kiedy okręt chce wejść do bazy, oblicza swój **dok deterministycznie** ze znacznika czasu Lamporta i własnego identyfikatora: `dok = ((ts + pid) mod K) + 1`.
3. Okręt rozsyła **REQ** do wszystkich i czeka na spełnienie **trzech warunków jednocześnie**: W1 (klasyczny Lamport — każdy wie o moim żądaniu), W2 (nikt z wyższym priorytetem nie chce tego samego doku), W3 (jest wystarczająco mechaników dla mnie i moich poprzedników).
4. Gdy wszystkie trzy warunki są spełnione, okręt **atomowo** wchodzi do doku i zajmuje mechaników — nie ma osobnego przydzielania każdego zasobu.
5. Po naprawie okręt **atomowo** zwalnia dok i mechaników jednym komunikatem **RELEASE** do wszystkich.

---

## 3. Co każdy okręt trzyma w pamięci

Każdy proces (okręt) utrzymuje lokalnie następujące zmienne:

### Zegar Lamporta `L`

Skalarny licznik całkowity. Rośnie w dwóch przypadkach:
- **`clock_tick()`** — zdarzenie lokalne lub wysyłanie wiadomości: `L := L + 1`. Wołane przy: tworzeniu żądania, wysyłaniu ACK, wysyłaniu RELEASE.
- **`clock_update(from, ts)`** — odbiór wiadomości od procesu `from` z znacznikiem `ts`: `L := max(L, ts) + 1`. Efekt uboczny: `last_seen[from] := ts`.

Dlaczego `max + 1`? Zasada zegarów Lamporta: zdarzenie odbioru musi mieć timestamp większy niż wysłanie. `+1` gwarantuje ścisłą monotoniczność.

### Wektor `last_seen[N]`

Tablica `N` liczb całkowitych. `last_seen[k]` = znacznik Lamporta **ostatniej wiadomości** (dowolnego typu: REQ/ACK/RELEASE) otrzymanej od procesu `k`.

Aktualizowany przy każdym odebraniu wiadomości: `last_seen[from] := ts`.

**Do czego służy:** sprawdzenie warunku W1 — czy wszyscy inni wiedzą o moim żądaniu.

### Kolejka żądań `Q`

Posortowana lista `request_t`, porządek rosnący po parze `(ts, pid)`.

Każde żądanie zawiera:
| Pole   | Znaczenie |
|--------|-----------|
| `ts`   | Znacznik Lamporta przypisany żądaniu w chwili wysyłania REQ |
| `pid`  | Identyfikator procesu — który okręt składa żądanie |
| `m`    | Ile mechaników żąda okręt (losowane z `[1, M]`) |
| `dock` | Który dok (obliczony deterministycznie, niezmienny) |
| `next` | Wskaźnik na następny element listy |

Kolejka jest **lokalna** — każdy okręt trzyma swoją kopię. Po odebraniu REQ od innego wstawia to żądanie do swojej Q. Po odebraniu RELEASE usuwa. Przy odpowiedniej obsłudze wiadomości wszystkie kopie Q są **spójne** co do zbioru elementów (z pewnym opóźnieniem wynikającym z czasu dostarczenia wiadomości).

Porządek sortowania: mniejszy `ts` = wyższy priorytet. Przy remisie `ts` — mniejszy `pid` wygrywa.

### Stan procesu `state`

Jeden z trzech:
- `REST` — okręt walczy lub odpoczywa, brak aktywnego żądania w Q, `MY_REQ = NULL`.
- `TRYING` — okręt wysłał REQ, czeka na spełnienie W1 ∧ W2 ∧ W3.
- `INSECTION` — okręt jest w doku, mechanicy przydzieleni.

### Zmienne aktualnego żądania

| Zmienna    | Znaczenie |
|------------|-----------|
| `MY_REQ`   | Wskaźnik na własny wpis w Q. `NULL` gdy stan to REST. |
| `MY_TS`    | Znacznik Lamporta przypisany własnemu żądaniu (`= L` w chwili `clock_tick()` przed REQ). |
| `MY_M`     | Ile mechaników okręt potrzebuje w tej rundzie (losowane raz przy wejściu do TRYING). |
| `MY_DOCK`  | Który dok okręt ma przypisany (obliczony raz przy wejściu do TRYING, niezmienny). |

---

## 4. Trzy typy wiadomości

Każda wiadomość to czwórka `(ts, pid, m, dock)` przesyłana przez MPI. Pola `m` i `dock` są zerowane dla ACK i RELEASE — używane tylko w REQ.

### REQ(ts, pid, m, dok)

**Kiedy:** przy przejściu REST → TRYING. Rozsyłane do **wszystkich** innych procesów (`N-1` wiadomości).

**Co zawiera:** pełna treść żądania — kiedy (ts), kto (pid), ile mechaników (m), do którego doku (dok).

**Efekt u odbiorcy:**
1. Aktualizacja zegara Lamporta i `last_seen[pid]`.
2. Wstawienie `(ts, pid, m, dok)` do lokalnej Q.
3. Odesłanie ACK do nadawcy.

### ACK(ts, pid)

**Kiedy:** natychmiast po odebraniu REQ. Wysyłany zawsze, bez względu na własny stan (nie ma deferred-ACK jak w Ricart-Agrawali).

**Co zawiera:** tylko `ts` (bieżący zegar Lamporta nadawcy) i `pid` nadawcy. Pola `m=0, dock=0`.

**Efekt u odbiorcy:**
1. Aktualizacja zegara i `last_seen[pid]`.
2. Może domknąć warunek W1 dla k=pid.

### RELEASE(ts, pid)

**Kiedy:** po wyjściu z sekcji krytycznej (po skończonej naprawie). Rozsyłany do wszystkich innych.

**Co zawiera:** `ts` (nowy tick zegara), `pid` nadawcy. Pola `m=0, dock=0`.

**Efekt u odbiorcy:**
1. Aktualizacja zegara i `last_seen[pid]`.
2. Usunięcie żądania `pid` z lokalnej Q.
3. Może odblokować poprzednio niespełnione W2 lub W3 u procesów w TRYING.

---

## 5. Reguła dock_assign — dlaczego tak

```
dok = ((ts + pid) mod K) + 1
```

Obliczana **raz** przy wejściu do TRYING. Wynik niezmienny przez całe życie żądania.

**Deterministyczność:** każdy proces, który zna `(ts, pid, K)` żądania, oblicza ten sam dok. Nie ma potrzeby żadnej dodatkowej komunikacji — dok jest widoczny w treści REQ i każdy może go zweryfikować lokalnie.

**Balansowanie obciążenia:** suma `ts + pid` jest różna dla każdego żądania (różne ts lub różne pid). Modulo K równomiernie rozkłada żądania po dokach przy dużej liczbie żądań.

**Jedna sekcja, nie n-1:** okręt kontensuje tylko o **jeden** konkretny dok (ten wyznaczony przez hash). Nie musi „wygrywać" K sekcji krytycznych żeby znaleźć wolny dok. Przy najgorszym priorytecie wystarczy poczekać aż **poprzednicy zainteresowani tym samym dokiem** wyjdą — nie wszyscy poprzednicy.

**Ograniczenie:** okręt może czekać na dok `d`, nawet jeśli doki `1..d-1` są wolne. Jest to dopuszczalne wg treści zadania i wymagane dla braku n-1 sekcji.

---

## 6. Trzy warunki wejścia krok po kroku

Warunki sprawdzane po **każdej** odebranej wiadomości (funkcja `try_enter_section()`). Krótkie spięcie: najpierw W1, potem W2, potem W3.

### W1 — Warunek Lamporta (każdy wie o moim żądaniu)

```
∀k ≠ i : last_seen[k] > MY_TS
```

**Co to znaczy:** od każdego innego procesu `k` otrzymaliśmy co najmniej jedną wiadomość ze znacznikiem większym niż `MY_TS`.

**Dlaczego to gwarantuje wiedzę:** kanały są FIFO i niezawodne. Mój REQ miał znacznik `MY_TS`. Każdy odbiorca po odebraniu REQ odesłał ACK z `ts(ACK) > MY_TS` (zasada zegarów Lamporta). Skoro `last_seen[k] > MY_TS`, ACK od `k` dotarł → a skoro kanały FIFO, mój REQ dotarł do `k` wcześniej niż ACK do mnie. Więc `k` zna moje żądanie.

**Przykład na 3 procesach (P0, P1, P2), K=2, M=3:**
```
P0 wysyła REQ z ts=5 do P1 i P2.
P1 odbiera REQ(ts=5), wysyła ACK(ts=6) do P0.
P2 odbiera REQ(ts=5), wysyła ACK(ts=7) do P0.
Po odebraniu ACK od P1: last_seen[1]=6 > 5 ✓ (ale last_seen[2]=0 ≤ 5 ✗)
Po odebraniu ACK od P2: last_seen[2]=7 > 5 ✓
Teraz W1 spełniony (dla k=1 i k=2: 6>5 i 7>5). Sprawdzamy W2.
```

### W2 — Warunek doku (nikt ważniejszy nie czeka na mój dok)

```
∀p ∈ Q : (ts_p, pid_p) < (ts_i, i) ∧ p.dok == MY_DOCK  →  FAŁSZ
```

Czyli: żaden **poprzednik** `my_req` w Q (element z wyższym priorytetem, tzn. `(ts_p,pid_p) < (MY_TS, MY)`) nie ma `dock == MY_DOCK`.

**Poprzednik** to żądanie, które jest **przed** naszym w kolejce — tzn. ma mniejszy `(ts, pid)` i przez to wyższy priorytet.

**Dlaczego patrzymy tylko na poprzedników, nie na wszystkich:** następniki (o niższym priorytecie) będą czekać na nas. My musimy ustąpić tylko tym, którzy mają wyższy priorytet i chcą tego samego doku.

**Kiedy W2 staje się fałszywy:** gdy poprzednik z tym samym dokiem jest w Q. Czekamy aż wyśle RELEASE i zniknie z Q — wtedy warunek może się domknąć.

**Przykład:**
```
Q = [(ts=3,pid=1,m=2,dok=2), (ts=5,pid=0,m=1,dok=2)]
MY = P0 (ts=5, dok=2)
Poprzednik: P1 (ts=3 < 5, ten sam dok=2) → W2 FAŁSZYWY. Czekamy.
P1 kończy naprawę, wysyła RELEASE.
Q = [(ts=5,pid=0,m=1,dok=2)]
Poprzednik z dok=2: brak → W2 PRAWDZIWY. Sprawdzamy W3.
```

### W3 — Warunek mechaników (wystarczająco zasobów)

```
(Σ_{d=1}^{K} max{p.m : p ∈ pred(my_req), p.dok = d}) + MY_M ≤ M
```

**Gdzie `pred(my_req)`** = zbiór poprzedników w Q (elementy z `(ts,pid) < (MY_TS, MY)`).

**Krok po kroku obliczenia:**
1. Dla każdego doku `d` od 1 do K: znajdź maksymalne `m` wśród poprzedników mających `dok = d`. Zapisz jako `dock_max[d]`.
2. Zsumuj `dock_max[1] + dock_max[2] + ... + dock_max[K]`. To pesymistyczna liczba mechaników zajętych przez poprzedników, gdy wszyscy wchodzą.
3. Dodaj `MY_M`.
4. Jeśli suma ≤ M → W3 spełniony.

**Dlaczego per-dok MAX, a nie suma wszystkich m poprzedników:**
Warunek W2 gwarantuje, że **na tym samym doku nie mogą przebywać jednocześnie dwa okręty w sekcji krytycznej**. Spośród wszystkich poprzedników na doku `d`, realnie w sekcji będzie co najwyżej jeden — ten o najwyższym priorytecie. Zajmie on co najwyżej `max{m : dok=d}` mechaników. Sumowanie surowych m wszystkich poprzedników na tym samym doku byłoby zawyżaniem — blokowałoby wejście mimo realnie dostępnych mechaników.

**Przykład (M=5, K=2):**
```
Poprzednicy w Q:
  P3: ts=1, dok=1, m=2
  P1: ts=2, dok=2, m=3
  P4: ts=3, dok=2, m=4  ← na tym samym doku 2 co P1
MY: ts=7, dok=1, m=1

dock_max[1] = max{2} = 2    (tylko P3 na doku 1)
dock_max[2] = max{3,4} = 4  (P1 i P4 na doku 2, max to 4)

Suma = 2 + 4 + MY_M(1) = 7 > M(5) → W3 FAŁSZYWY. Czekamy.

P4 dostaje RELEASE (był najwyższy priorytet na doku 2). Q bez P4.
dock_max[2] = max{3} = 3
Suma = 2 + 3 + 1 = 6 > 5 → nadal FAŁSZYWY.

P1 wychodzi. dock_max[2] = 0.
Suma = 2 + 0 + 1 = 3 ≤ 5 → W3 PRAWDZIWY. Wchodzimy!
```

---

## 7. Diagram flow — cykl jednego okrętu

```
                         ┌─────────────────────────────────────────┐
                         │                  REST                   │
                         │  - brak MY_REQ w Q                      │
                         │  - obsługa wiadomości (drain_inbox)     │
                         │  - symulacja walki ~1 sekunda            │
                         └──────────────┬──────────────────────────┘
                                        │
                          decyzja powrotu z walki
                                        │
                         ┌──────────────▼──────────────────────────┐
                         │         start_request()                 │
                         │  1. MY_M := rand(1..M)                  │
                         │  2. clock_tick() → MY_TS               │
                         │  3. dock_assign(MY_TS, MY, K) → MY_DOCK│
                         │  4. q_insert(Q, MY_TS, MY, MY_M, MY_DOCK)│
                         │  5. MY_REQ := q_find_by_pid(Q, MY)     │
                         │  6. state := TRYING                     │
                         │  7. broadcast REQ(MY_TS,MY,MY_M,MY_DOCK)│
                         └──────────────┬──────────────────────────┘
                                        │
                         ┌──────────────▼──────────────────────────┐
                         │                TRYING                   │
                         │  pętla:                                 │
                         │    drain_inbox()                        │
                         │    try_enter_section()                  │
                         │      ├─ W1 fałsz → czekaj dalej        │
                         │      ├─ W2 fałsz → czekaj dalej        │
                         │      └─ W3 fałsz → czekaj dalej        │
                         │    usleep(5ms)                          │
                         └──────────────┬──────────────────────────┘
                                        │ W1 ∧ W2 ∧ W3 spełnione
                         ┌──────────────▼──────────────────────────┐
                         │              INSECTION                  │
                         │  - log "Wszedłem do doku MY_DOCK"       │
                         │  - symulacja naprawy 50–249 ms          │
                         │  - drain_inbox() w każdej iteracji      │
                         │    (odpowiada ACK na REQ od innych!)    │
                         └──────────────┬──────────────────────────┘
                                        │
                          koniec naprawy: section_phase() wyjście
                                        │
                         ┌──────────────▼──────────────────────────┐
                         │         wyjście z sekcji                │
                         │  1. q_remove_by_pid(Q, MY)              │
                         │  2. MY_REQ := NULL                      │
                         │  3. clock_tick() → rel_ts              │
                         │  4. broadcast RELEASE(rel_ts, MY)      │
                         │  5. state := REST                       │
                         └──────────────┬──────────────────────────┘
                                        │
                                    (powrót do REST)
```

---

## 8. Diagram flow — obsługa jednej odebranej wiadomości

```
Odebrano wiadomość (tag, from, msg)
         │
         ▼
clock_update(from, msg.ts)
  → L := max(L, msg.ts) + 1
  → last_seen[from] := msg.ts
         │
         ├─── tag == MSG_REQ ──────────────────────────────────────┐
         │                                                         │
         │    q_insert(Q, msg.ts, msg.pid, msg.m, msg.dock)       │
         │    ack_ts := clock_tick()                               │
         │    comm_send_ack(ack_ts, MY, from)                     │
         │                                                         │
         ├─── tag == MSG_ACK ──────────────────────────────────────┤
         │                                                         │
         │    (nic poza clock_update wykonanym wcześniej)          │
         │                                                         │
         └─── tag == MSG_RELEASE ──────────────────────────────────┘
                                                                   │
              q_remove_by_pid(Q, msg.pid)                         │
                                                                   │
         ◄─────────────────────────────────────────────────────────┘
         │
         ▼
[jeśli state == TRYING]:
  try_enter_section()
    → sprawdź W1 (clock_w1_satisfied)
    → sprawdź W2 (dock_w2_satisfied)
    → sprawdź W3 (mech_w3_satisfied)
    → jeśli wszystkie true: state := INSECTION
```

**Kluczowy szczegół:** `clock_update` wołane jako **pierwsze** przed jakąkolwiek inną akcją. Gwarantuje, że `last_seen` jest zaktualizowane zanim W1 zostanie sprawdzone.

---

## 9. Przykład end-to-end na 3 procesach

**Parametry:** N=3, K=2, M=4. Procesy P0, P1, P2.

Śledzenie stanu z perspektywy P0.

```
Zdarzenie                | P0                          | Q(P0)           | last_seen(P0)
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
START                    | REST, L=0                   | {}              | [0, 0, 0]
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
P1 wysyła REQ(ts=2,pid=1,| P0 odbiera, clock_update:   | {(2,P1,m=2,     | [0, 2, 0]
m=2,dok=1)               | L:=max(0,2)+1=3             |   dok=1)}       |
                         | q_insert(2,P1,2,1)          |                 |
                         | tick→L=4, send ACK(4) do P1 |                 |
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
P0 decyduje się ubiegać  | MY_M=1 (losowe)             | {(2,P1,2,1),    | [0, 2, 0]
                         | tick→L=5, MY_TS=5           |  (5,P0,1,1)}    |
                         | dock_assign(5,0,2)=(5+0)%2  |                 |
                         |  +1 = 5%2+1 = 1+1 = 2? nie, |                 |
                         |  (5+0)%2=1, +1=2 → dok=2?   |                 |
                         | Hmm: (5+0) mod 2 = 1, +1=2  | MY_DOCK=2       |
                         | q_insert(5,P0,1,2), MY_REQ  |                 |
                         | state=TRYING                |                 |
                         | broadcast REQ(5,P0,1,2)     |                 |
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
P2 wysyła ACK(ts=6,pid=2)| clock_update: L=max(5,6)+1=7| bez zmiany Q    | [0, 2, 6]
do P0                    | last_seen[2]=6              |                 |
                         | W1: ls[1]=2≤5 ✗ → nie wchodzimy               |
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
P1 wysyła ACK(ts=8,pid=1)| clock_update: L=max(7,8)+1=9| bez zmiany Q    | [0, 8, 6]
do P0                    | last_seen[1]=8              |                 |
                         | W1: ls[1]=8>5 ✓, ls[2]=6>5 ✓ → W1 spełnione!  |
                         | W2: poprzednicy z dok=2?    |                 |
                         |  Poprzednik = (ts,pid)<(5,0)|                 |
                         |  (2,P1): dok=1 ≠ 2 → OK     |                 |
                         |  Brak poprzednika z dok=2 → W2 ✓              |
                         | W3: pred={P1(ts=2,dok=1,m=2)}                 |
                         |  dock_max[1]=2, dock_max[2]=0                 |
                         |  total=2+0+MY_M(1)=3 ≤ M(4) → W3 ✓            |
                         | state := INSECTION !!!      |                 |
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
P0 w sekcji              | log "Wszedłem do doku 2, mam 1 mechanika"     |
                         | naprawia się (50–249ms)     |                 |
                         | nadal drain_inbox — odpowiada na REQ od P1    |
─────────────────────────┼─────────────────────────────┼─────────────────┼──────────────
P0 wychodzi              | q_remove_by_pid(Q, P0)      | {(2,P1,2,1)}    |
                         | tick→L=10, RELEASE(10,P0)   |                 |
                         | state=REST                  |                 |
```

**Co widzimy:** P0 czekał na ACK od P1 (żeby W1 się domknął). P1 miał wcześniejszy ts=2, ale chciał doku 1, więc W2 nie blokowało P0. W3 też przeszło — mechanicy wystarczyły.

---

## 10. Co gwarantują warunki — niezmienniki

### I1 — Co najwyżej 1 okręt na danym doku jednocześnie

**Dowód:** Niech A i B wejdą jednocześnie do sekcji z tym samym dokiem d. Bez straty ogólności `(ts_A, A) < (ts_B, B)` (A ma wyższy priorytet).

Z W1 dla B: `last_seen_B[A] > ts_B > ts_A`. Kanały FIFO, więc REQ od A dotarło do B przed ACK (ts>ts_B) od B do A. Zatem w chwili, gdy B sprawdzało W2, REQ od A było już w `Q_B`.

REQ od A ma `dok = d = dock`. W2 sprawdza, czy poprzednik z tym samym dokiem istnieje. A jest poprzednikiem B (bo `(ts_A,A) < (ts_B,B)`). Więc W2 dla B byłoby **fałszywe**. Sprzeczność — B nie mogło wejść. ∎

### I2 — Suma m aktywnych okrętów ≤ M

Z I1 wiemy, że na każdym doku jest co najwyżej jeden aktywny okręt. Niech `S` = zbiór okrętów aktualnie w INSECTION.

Dla każdego okrętu `s ∈ S` z dokiem `d_s` mamy: `m_s ≤ dock_max[d_s]` w chwili gdy `s` sprawdzało W3 (bo `s` jest poprzednikiem późniejszych, a nie wcześniejszych; w chwili wejścia `s` do sekcji, w `pred(s)` nie było `s` samego).

Formalnie: gdy okręt `i` wchodził do sekcji, W3 gwarantowało `Σ dock_max[d] + m_i ≤ M`, gdzie `dock_max[d]` uwzględnia tylko poprzedników `i`. Po wejściu `i`, jego `m_i` jest jednym z tych poprzednikowych max dla kolejnych, co jest spójne. Więc łączna suma wszystkich aktywnych `m_k ≤ M`. ∎

### I3 — Brak zakleszczenia

Rozważmy żądanie o **globalnie najniższym** `(ts, pid)` — jest na szczycie Q. Ma pustą listę poprzedników. W3: `total = 0 + m_i ≤ M` (bo `m_i ∈ [1,M]`) — spełnione. W2: brak poprzedników z żadnym dokiem — spełnione. Musi jedynie poczekać na W1 (ACK od wszystkich). Każdy inny proces musi odpowiedzieć ACK — robi to zawsze, natychmiast po odebraniu REQ. Więc W1 domknie się w skończonym czasie. Po wejściu i wyjściu minimum globalnie wysyła RELEASE — odblokowuje innych. Indukcja po `(ts,pid)`. ∎

### I4 — Brak głodzenia (liveness)

Zegar Lamporta rośnie monotonicznie. Każde żądanie w Q ma skończoną liczbę poprzedników — nie można tworzyć nieskończonej liczby żądań z mniejszym `(ts,pid)` niż istniejące (bo ts zawsze rośnie). Każdy poprzednik eventualnie wchodzi do sekcji i wysyła RELEASE (z I3 — żaden nie zakleszcza się). Po usunięciu wszystkich poprzedników z tym samym dokiem (W2) i dostatecznym zwolnieniu mechaników (W3) — żądanie wejdzie. ∎

---

## 11. Złożoność

| Miara | Wartość | Wyjaśnienie |
|-------|---------|-------------|
| **Czasowa** | 3 rundy | REQ → ACK → RELEASE. Trzy kroki do zakończenia jednego wejścia-wyjścia. Zgodna z klasycznym Lamportem. |
| **Komunikacyjna** | 3(N−1) wiadomości | N−1 REQ + N−1 ACK + N−1 RELEASE. |
| **Koszt W2, W3** | 0 dodatkowych rund | Czysto lokalne obliczenia na Q — nie generują nowych typów wiadomości ani rund. |

Porównanie z Ricart-Agrawali: RA ma 2 rundy i 2(N−1) komunikatów (REQ + ACK, bez RELEASE). Nie użyliśmy RA, bo generalizacja do pojemności M jest trudniejsza przy deferred-ACK — nie wiadomo ile ACK „wystarczy" przy sekcji pojemności > 1.

---

## 12. FAQ — pytania prowadzącego

**Q: Dlaczego nie zarządca zasobów?**
A: Wymagania kursu (`wymagania.md` l. 89) explicite zakazują. Poza tym: centralny zarządca = wąskie gardło i single point of failure. Nasz algorytm jest w pełni rozproszony — wszystkie decyzje lokalne.

**Q: Dlaczego dok statyczny (przydzielony raz), a nie wybierany dynamicznie?**
A: Dynamiczny wybór wymagałby albo losowania (nieoptymalny per wymagania — można przegrać wiele sekcji), albo kolejnej rundy komunikacji. Statyczny hash `((ts+pid) mod K)+1` jest deterministyczny — każdy proces może obliczyć dok dowolnego żądania bez pytania kogokolwiek.

**Q: Co jeśli dwa procesy mają ten sam ts Lamporta?**
A: Tie-break po `pid`: mniejszy `pid` = wyższy priorytet. Para `(ts, pid)` jest zawsze unikalna (nie ma dwóch procesów o tym samym `pid`). Zatem porządek jest ścisły i totalny.

**Q: Co jeśli M=1?**
A: W3 redukuje się: `max{m : pred, dok=d} ≤ m_i ≤ 1` — jednocześnie może być w sekcji tylko 1 okręt. Razem z W2 (który i tak nie wpuszcza dwóch na ten sam dok) algorytm degeneruje do klasycznej sekcji krytycznej pojemności 1.

**Q: Dlaczego nie Ricart-Agrawala (deferred ACK)?**
A: Ricart-Agrawala opiera się na „zebrałem N−1 ACK = wszyscy dali mi zgodę". Przy uogólnionej sekcji pojemności M trudno określić co oznacza „wystarczająca liczba ACK" — każde ACK może reprezentować różną ilość zasobów. Lamport z warunkami W1/W2/W3 jest czytelniejszy i poprawniejszy dla tego problemu. Kosztem jest dodatkowa runda (RELEASE).

**Q: Co robi okręt w INSECTION kiedy dostaje REQ od innych?**
A: Wstawia REQ do Q (`q_insert`) i odsyła ACK. To kluczowe — gdyby nie odpowiadał, inne okręty czekałyby na W1 w nieskończoność. Okręt w sekcji nie blokuje postępu innych.

**Q: Dlaczego `usleep(5ms)` w pętli TRYING?**
A: Busy-wait bez sleep zjadałby 100% CPU. 5ms to kompromis między latencją (czas od spełnienia warunków do wejścia) a zużyciem zasobów.

**Q: Jak uruchomić na dwóch maszynach?**
A: `mpirun --hostfile hosts -np N ./okrety K M [seed]`. Plik `hosts` zawiera adresy i liczbę slotów per maszyna. Skrypt `scripts/run.sh` obsługuje różne ścieżki binaru na różnych maszynach.
