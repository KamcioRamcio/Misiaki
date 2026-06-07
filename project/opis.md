# Bardzo szczegółowy opis algorytmu Lamport-Dual (okręty, doki, mechanicy)

Ten dokument jest rozszerzoną wersją opisu algorytmu i odpowiada bezpośrednio
aktualnej implementacji w katalogu `src/`.

Cel dokumentu:

1. opisać algorytm w formacie wymaganym na obronę,
2. wyjaśnić dokładnie, co oznaczają wszystkie istotne zmienne,
3. pokazać "co i kiedy" dzieje się w czasie działania programu,
4. ułatwić analizę logów i debugowanie.

---

## 1) OPIS OGÓLNY

Poniższy opis (numerowane linie) jest częścią "pod obronę".

L01. Każdy proces MPI reprezentuje jeden okręt.
L02. Proces działa w pętli nieskończonej: `REST -> TRYING -> INSECTION -> REST`.
L03. Okręt po powrocie z walki losuje zapotrzebowanie `m` mechaników z zakresu
     `1..M`.
L04. Okręt tworzy żądanie `REQ(ts, pid, m, dock)` i rozsyła je do wszystkich
     pozostałych procesów.
L05. `ts` to znacznik Lamporta z chwili utworzenia żądania.
L06. `dock` jest wyliczany deterministycznie: `dock = ((ts + pid) mod K) + 1`.
L07. Każdy proces utrzymuje lokalną kopię globalnej kolejki żądań `Q`
     posortowaną po `(ts, pid)` rosnąco.
L08. Na `REQ` odbiorca zawsze odpowiada `ACK`.
L09. Proces może wejść do sekcji krytycznej tylko wtedy, gdy równocześnie
     spełnione są trzy warunki: `W1` i `W2` i `W3`.
L10. `W1` (Lamport): od każdego innego procesu odebrano wiadomość ze
     znacznikiem większym niż `my_ts`.
L11. `W2` (dok): wśród poprzedników mojego żądania w `Q` nie ma żądania na
     ten sam dok.
L12. `W3` (mechanicy): suma `m` poprzedników w `Q` plus moje `m` nie
     przekracza `M`.
L13. Gdy warunki są spełnione, proces przechodzi do `INSECTION` i "trzyma"
     atomowo oba zasoby: dok i mechaników.
L14. Po naprawie proces usuwa swoje żądanie z `Q` i rozsyła `RELEASE`.
L15. `RELEASE` zwalnia jednocześnie dok oraz mechaników.
L16. Brak centralnego koordynatora: wszystkie decyzje są podejmowane lokalnie
     na podstawie lokalnej `Q`, zegara Lamporta i odebranych wiadomości.

---

## 2) STANY PROCESÓW

L17. `STATE_REST`
     Proces nie ubiega się o zasoby. Odbiera i obsługuje wiadomości od innych,
     ale sam nie ma aktywnego żądania.

L18. `STATE_TRYING`
     Proces ma własne żądanie w kolejce i czeka na spełnienie `W1 && W2 && W3`.

L19. `STATE_INSECTION`
     Proces jest w sekcji krytycznej: używa konkretnego doku oraz dokładnie
     tyle mechaników, ile zadeklarował w swoim żądaniu.

---

## 3) TYPY WIADOMOŚCI

L20. `REQ(ts, pid, m, dock)`
     Prośba o dostęp do sekcji krytycznej. Niesie pełną treść żądania.

L21. `ACK(ts, pid)`
     Potwierdzenie przyjęcia `REQ` przez odbiorcę.

L22. `RELEASE(ts, pid)`
     Informacja o zwolnieniu zasobów przez `pid`; odbiorcy usuwają żądanie
     tego procesu z `Q`.

L23. Format na "drucie" (`wire_msg_t`) zawsze ma 4 pola całkowite:
     `ts`, `pid`, `m`, `dock`.
L24. Dla `ACK` i `RELEASE` pola `m` i `dock` nie są używane (ustawiane na 0).

---

## 4) OPIS SZCZEGÓŁOWY (REAKCJE W STANACH)

Poniżej znajduje się dokładny opis, jak proces reaguje na każdą wiadomość
w każdym stanie.

### 4.1 Stan `STATE_REST`

L25. Proces jest poza sekcją i okresowo wykonuje "czas walki".
L26. W tym czasie stale opróżnia skrzynkę odbiorczą (`drain_inbox`).
L27. Po decyzji o powrocie do bazy:
     1) losuje `MY_M`,
     2) wykonuje `MY_TS = clock_tick()`,
     3) wyznacza `MY_DOCK = dock_assign(MY_TS, MY, K)`,
     4) wstawia swoje żądanie do `Q`,
     5) zapisuje wskaźnik `MY_REQ` na własny wpis,
     6) przechodzi do `STATE_TRYING`,
     7) rozsyła `REQ` do wszystkich.

Obsługa wiadomości przychodzących w `REST`:

L28. `REQ`: aktualizacja zegara (`clock_update`), wstawienie do `Q`, wysłanie
     `ACK`.
L29. `ACK`: tylko aktualizacja zegara.
L30. `RELEASE`: aktualizacja zegara i usunięcie wpisu nadawcy z `Q`.

### 4.2 Stan `STATE_TRYING`

L31. Proces ma własny wpis w `Q` i czeka na wejście.
L32. W pętli:
     1) odbiera wszystkie dostępne wiadomości,
     2) po każdej partii sprawdza `W1`, `W2`, `W3`.
L33. Gdy wszystkie warunki prawdziwe, stan zmienia się na `STATE_INSECTION`.

Obsługa wiadomości przychodzących w `TRYING`:

L34. `REQ`: `clock_update`, dopisanie do `Q`, natychmiast `ACK`.
L35. `ACK`: `clock_update`; wiadomość może domknąć `W1` dla danego nadawcy.
L36. `RELEASE`: `clock_update`, usunięcie nadawcy z `Q`; może odblokować
     `W2` i/lub `W3`.

### 4.3 Stan `STATE_INSECTION`

L37. Proces realizuje "naprawę" przez losowy czas (50-249 ms w implementacji).
L38. Nawet w sekcji krytycznej proces nadal odbiera wiadomości i odpowiada
     na `REQ` przez `ACK`, aby nie blokować postępu innych.
L39. Po zakończeniu naprawy:
     1) usuwa swój wpis z `Q`,
     2) zeruje `MY_REQ`,
     3) zwiększa zegar (`clock_tick`) dla `RELEASE`,
     4) rozsyła `RELEASE` do wszystkich,
     5) wraca do `STATE_REST`.

Obsługa wiadomości przychodzących w `INSECTION`:

L40. `REQ`: `clock_update`, dopisanie do `Q`, `ACK`.
L41. `ACK`: `clock_update`.
L42. `RELEASE`: `clock_update`, usunięcie wpisu z `Q`.

---

## 5) SŁOWNIK ZMIENNYCH I STRUKTUR (CO OZNACZA KAŻDA)

Ta sekcja mapuje 1:1 na kod w `src/`.

### 5.1 Zmienne globalne w `src/main.c`

1. `N`
   Liczba procesów MPI (okrętów). Ustawiana przez `MPI_Comm_size`.

2. `K`
   Liczba doków (zasób rozróżnialny), podawana jako argument programu.

3. `M`
   Liczba mechaników (zasób nierozróżnialny), podawana jako argument programu.

4. `MY`
   Własny identyfikator procesu (`rank` MPI), ustawiany przez `MPI_Comm_rank`.

5. `state`
   Bieżący stan automatu procesu: `STATE_REST`, `STATE_TRYING`,
   `STATE_INSECTION`.

6. `Q`
   Lokalna kopia kolejki żądań Lamporta.
   Typ: `queue_t` (lista jednokierunkowa, sortowanie po `(ts, pid)`).

7. `MY_REQ`
   Wskaźnik na własny element kolejki `Q`.
   Używany przy sprawdzaniu `W2/W3`.
   Ustawiany po `q_insert` w `start_request`, zerowany po wyjściu z sekcji.

8. `MY_TS`
   Znacznik Lamporta przypisany do bieżącego własnego żądania.

9. `MY_M`
   Liczba mechaników żądana przez proces w bieżącej iteracji.

10. `MY_DOCK`
    Dok przypisany deterministycznie do bieżącego żądania.

### 5.2 Zmienne modułu zegara `src/clock.c`

1. `L`
   Lokalny zegar Lamporta procesu.

2. `last_seen[]`
   Tablica rozmiaru `N`.
   `last_seen[p]` to znacznik `ts` z ostatniej wiadomości odebranej od `p`.

3. `N`, `MY`
   Lokalna kopia liczby procesów i własnego `pid` do sprawdzania `W1`.

### 5.3 Struktury danych z `src/types.h` i `src/queue.h`

1. `request_t`
   Jedno żądanie w kolejce:
   - `ts`: priorytet Lamporta,
   - `pid`: nadawca,
   - `m`: liczba mechaników,
   - `dock`: przypisany dok,
   - `next`: wskaźnik listy.

2. `queue_t`
   Kontener kolejki:
   - `head`: początek listy,
   - `size`: liczba elementów.

3. `wire_msg_t`
   Struktura wiadomości MPI: `ts`, `pid`, `m`, `dock`.

### 5.4 Zmienna komunikacyjna w `src/comm.c`

1. `WIRE_T`
   Zarejestrowany typ MPI odpowiadający strukturze `wire_msg_t`.
   Inicjalizowany leniwie (`ensure_type`).

### 5.5 Zmienna logowania w `src/log.c`

1. `MY`
   Kopia `pid` do prefiksowania logów: `[pid] [tX] ...`.

---

## 6) KLUCZOWE FUNKCJE (CO ROBIĄ I KIEDY SĄ WYWOŁYWANE)

### 6.1 `main.c`

1. `handle_message(tag, from, msg)`
   Centralna obsługa pojedynczej wiadomości:
   - zawsze najpierw `clock_update(from, msg->ts)`,
   - `REQ`: wpis do `Q` i `ACK`,
   - `ACK`: tylko efekt na zegarze,
   - `RELEASE`: usunięcie nadawcy z `Q`.

2. `try_enter_section()`
   Sprawdza po kolei:
   - czy stan to `TRYING`,
   - czy `MY_REQ != NULL`,
   - `clock_w1_satisfied(MY_TS)`,
   - `dock_w2_satisfied(&Q, MY_REQ)`,
   - `mech_w3_satisfied(&Q, MY_REQ, M)`.
   Jeśli wszystkie prawdziwe: `state = STATE_INSECTION`.

3. `drain_inbox()`
   Odbiera wszystkie aktualnie dostępne wiadomości (`MPI_Iprobe` + `MPI_Recv`)
   i przekazuje je do `handle_message`.

4. `rest_phase()`
   Symuluje czas poza bazą (20 iteracji po 50 ms), ale cały czas odbiera
   wiadomości.

5. `start_request()`
   Tworzy nowe lokalne żądanie i rozsyła `REQ`.

6. `trying_phase()`
   Aktywny etap czekania na `W1 && W2 && W3`.

7. `section_phase()`
   Symuluje naprawę, następnie usuwa własne żądanie i rozsyła `RELEASE`.

### 6.2 `clock.c`

1. `clock_tick()`
   Zdarzenie wewnętrzne / wysyłka: `L = L + 1`.

2. `clock_update(from, ts)`
   Odbiór wiadomości: `L = max(L, ts) + 1`, `last_seen[from] = ts`.

3. `clock_w1_satisfied(my_ts)`
   Sprawdza warunek Lamporta:
   dla każdego `k != MY` musi być `last_seen[k] > my_ts`.

### 6.3 `queue.c`

1. `q_insert(...)`
   Wstawia wpis w porządku `(ts, pid)`.
   Zawiera ochronę przed duplikatem identycznego `(ts, pid)`.

2. `q_remove_by_pid(pid)`
   Usuwa pierwszy wpis procesu `pid`.

3. `q_find_by_pid(pid)`
   Służy do uzyskania `MY_REQ` po dodaniu własnego żądania.

4. `q_predecessors(q, my_req, &count, &m_sum)`
   Oblicza liczbę i sumę `m` poprzedników `my_req` w kolejce.

### 6.4 `dock.c`

1. `dock_assign(ts, pid, K)`
   Czysta funkcja przypisania doku; każdy proces oblicza ten sam wynik.

2. `dock_w2_satisfied(q, my_req)`
   Przegląda poprzedników `my_req`; jeśli którykolwiek ma ten sam `dock`,
   zwraca fałsz.

### 6.5 `mechanics.c`

1. `mech_w3_satisfied(q, my_req, M)`
   Liczy `m_sum` poprzedników i sprawdza `m_sum + my_req->m <= M`.

### 6.6 `comm.c`

1. `comm_send_req_all(...)`
   Rozsyła `REQ` do wszystkich oprócz siebie.

2. `comm_send_ack(...)`
   Wysyła `ACK` do jednego procesu.

3. `comm_send_release_all(...)`
   Rozsyła `RELEASE` do wszystkich oprócz siebie.

4. `comm_try_recv(...)`
   Nieblokująco sprawdza skrzynkę i odbiera jedną wiadomość, jeśli jest.

---

## 7) WARUNKI WEJŚCIA W1/W2/W3 - INTUICJA I SKUTEK

### 7.1 W1 - porządek Lamporta

Definicja: od każdego procesu musi przyjść wiadomość ze znacznikiem
większym niż `my_ts`.

Znaczenie praktyczne:

1. mam pewność, że "świat" widział już moje żądanie lub zdarzenia późniejsze,
2. nie wejdę zbyt wcześnie względem starszych priorytetowo żądań,
3. lokalna decyzja o wejściu jest bezpieczna globalnie.

### 7.2 W2 - wykluczanie na doku

Definicja: żaden poprzednik `my_req` w kolejce nie może mieć tego samego doku.

Skutek:

1. w danym momencie co najwyżej jeden proces może legalnie wejść na konkretny
   dok,
2. okręt może czekać na "swój" dok mimo wolnych innych doków - to celowe
   zachowanie wynikające z deterministycznego przypisania.

### 7.3 W3 - pojemność mechaników

Definicja: suma zapotrzebowań poprzedników plus moje zapotrzebowanie
nie przekracza `M`.

Skutek:

1. mechanicy są modelowani jako jedna uogólniona sekcja krytyczna
   o pojemności `M`,
2. nigdy nie nastąpi przekroczenie dostępnej liczby mechaników,
3. nie potrzeba centralnego licznika ani dodatkowej sekcji krytycznej.

---

## 8) CO KIEDY SIĘ DZIEJE - PEŁNY CYKL JEDNEGO OKRĘTU

Poniżej pełna sekwencja w kolejności czasowej.

1. Inicjalizacja:
   - `MPI_Init`,
   - odczyt `N`, `MY`, argumentów `K`, `M`, opcjonalnie `seed`,
   - `clock_init`, `log_init`, `q_init`.

2. Wejście do `REST`:
   - log "Wracam z walki",
   - przez ok. 1 sekundę proces odbiera wiadomości i odpowiada na nie.

3. Start nowego żądania (`start_request`):
   - `MY_M = rand() % M + 1`,
   - `MY_TS = clock_tick()`,
   - `MY_DOCK = dock_assign(MY_TS, MY, K)`,
   - dopisanie do `Q`,
   - `MY_REQ = q_find_by_pid(&Q, MY)`,
   - `state = TRYING`,
   - log rozpoczęcia starania,
   - broadcast `REQ`.

4. Oczekiwanie (`trying_phase`):
   - proces stale odbiera wiadomości,
   - każda wiadomość może zmienić `Q` lub `last_seen`,
   - po obsłudze wykonywane jest `try_enter_section`.

5. Wejście do sekcji:
   - jeśli `W1 && W2 && W3`, stan zmienia się na `INSECTION`,
   - log: "Wszedłem do doku d, mam m mechaników".

6. Naprawa (`section_phase`):
   - losowy czas 50-249 ms,
   - proces nadal obsługuje skrzynkę (ważne dla postępu systemu).

7. Wyjście z sekcji:
   - zapamiętanie `dock_done`, `mech_done` do logu,
   - usunięcie swojego wpisu z `Q`,
   - `MY_REQ = NULL`,
   - `rel_ts = clock_tick()`,
   - log zwolnienia,
   - broadcast `RELEASE`,
   - `state = REST`.

8. Powrót do punktu 2.

---

## 9) JAK ROŚNIE ZEGAR LAMPORTA (BARDZO KONKRETNIE)

`L` rośnie w dwóch sytuacjach:

1. Zdarzenie lokalne / wysyłka (`clock_tick`):
   - start nowego żądania (`MY_TS`),
   - wysłanie `RELEASE` po naprawie,
   - wysłanie `ACK` po odebranym `REQ`.

2. Odbiór wiadomości (`clock_update(from, ts)`):
   - `L = max(L, ts) + 1`,
   - `last_seen[from] = ts`.

Wniosek:

1. każde odebrane `REQ/ACK/RELEASE` przesuwa wiedzę o nadawcy,
2. warunek `W1` opiera się dokładnie o `last_seen[]`.

---

## 10) DLACZEGO TO DZIAŁA (NIEZMIENNIKI)

### I1: na jednym doku nie ma dwóch okrętów naraz

Powód: `W2` eliminuje wejście procesu, jeśli ma starszego poprzednika na ten
sam dok.

### I2: suma zajętych mechaników nigdy nie przekracza `M`

Powód: `W3` pilnuje pojemności na podstawie wszystkich poprzedników.

### I3: brak zakleszczenia

Powód: zawsze istnieje minimalne żądanie w porządku `(ts, pid)`, które po
domknięciu `W1` może wejść; po wyjściu wysyła `RELEASE`, więc kolejni mogą
postępować.

### I4: brak głodzenia

Powód: porządek Lamporta daje skończoną liczbę poprzedników dla każdego
żądania; poprzednicy opuszczają sekcję i są usuwani z kolejki.

Dodatkowo praktyczna walidacja:

1. `scripts/verify_logs.py` sprawdza z logów dokładnie I1 i I2.

---

## 11) ZŁOŻONOŚĆ

1. Czas formalny: 3 etapy (`REQ -> ACK -> RELEASE`) jak w klasycznym
   Lamportcie.
2. Komunikacja formalna: `3 * (N - 1)` wiadomości na jedno własne wejście.
3. Rozszerzenie o `W2/W3` nie dodaje dodatkowych typów wiadomości - to
   wyłącznie obliczenia lokalne na `Q`.

---

## 12) INTERPRETACJA LOGÓW (CO OZNACZAJĄ LINIE)

Format:

1. `[pid] [tX] Wracam z walki`
   Proces jest w `REST`.

2. `[pid] [tX] Rozpoczynam staranie o sekcję (m=..., dock=..., ts=...)`
   Utworzono własne żądanie i rozesłano `REQ`.

3. `[pid] [tX] Wszedłem do doku d, mam m mechaników`
   Spełnione `W1/W2/W3`, proces jest w `INSECTION`.

4. `[pid] [tX] Zwalniam dok d i m mechaników`
   Koniec sekcji, rozsyłany `RELEASE`.

W trybie `DEBUG` dochodzą logi per-wiadomość (`recv REQ`, `send ACK`, itd.).

---

## 13) NAJWAŻNIEJSZE RZECZY DO ZAPAMIĘTANIA NA OBRONĘ

1. Jedna wspólna kolejka Lamporta `Q` porządkuje wszystkie żądania.
2. Dok jest przypisany deterministycznie z `(ts, pid, K)` i nie zmienia się
   aż do `RELEASE`.
3. Mechanicy są modelowani jako jedna sekcja krytyczna pojemności `M`
   (nie licznik i nie `M` osobnych mutexów).
4. Wejście do sekcji to zawsze jednoczesne spełnienie `W1 && W2 && W3`.
5. `RELEASE` zwalnia oba zasoby atomowo i odblokowuje kolejnych.

---

## 14) Jednozdaniowe streszczenie algorytmu

Każdy okręt publikuje swoje żądanie `(ts, pid, m, dock)`, utrzymuje lokalną
kopię kolejki wszystkich żądań i wchodzi do sekcji dopiero wtedy, gdy ma
potwierdzoną wiedzę czasową (`W1`), brak konfliktu na przypisanym doku (`W2`)
oraz zachowaną pojemność mechaników (`W3`).
