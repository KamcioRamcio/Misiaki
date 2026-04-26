# Opis algorytmu Lamport-Dual (okręty, doki, mechanicy)

Algorytm rozszerza algorytm Lamporta z `rozproszona_sekcja_krytyczna.md`
o równoczesny przydział doku rozróżnialnego oraz mechaników nierozróżnialnych
w jednej, wspólnej kolejce żądań.

## Struktury i zmienne

1. `Q` — globalna kolejka żądań, każda pozycja zawiera `{ts, pid, m, dok}`,
   posortowana rosnąco po `(ts, pid)`. Początkowo pusta.
2. `L` — skalarny zegar logiczny Lamporta procesu, początkowo `0`.
3. `last_seen[N]` — wektor znaczników ostatnio odebranych wiadomości
   od poszczególnych procesów; potrzebny do warunku `W1`. Początkowo zera.
4. `state` — bieżący stan procesu (`REST` / `TRYING` / `INSECTION`),
   początkowo `REST`.
5. `my_req` — wskaźnik na własne żądanie w `Q`, gdy `state ≠ REST`.
6. `n` — liczba procesów (okrętów).
7. `K` — liczba doków (rozróżnialnych).
8. `M` — liczba mechaników (nierozróżnialnych).

## Wiadomości

Wszystkie wiadomości są podbite znacznikiem czasowym, modyfikowanym zgodnie
z zasadami skalarnego zegara Lamporta.

1. `REQ(ts, pid, m, dok)` — żądanie dostępu; nadawca zgłasza zapotrzebowanie
   na `m` mechaników; pole `dok = ((ts + pid) mod K) + 1` jest deterministycznym
   przydziałem doku, niezmiennym do `RELEASE`.
2. `ACK(ts)` — potwierdzenie odbioru `REQ`. Wysyłane natychmiast po wstawieniu
   `REQ` do `Q`. Spełnia `ts(ACK) > ts(REQ)`.
3. `RELEASE(ts, pid)` — zwolnienie zasobów (doku i mechaników), wysyłane po
   wyjściu z sekcji krytycznej. Powoduje usunięcie żądania nadawcy z `Q`.

## Stany

Stanem początkowym procesu jest `REST`.

1. `REST` — okręt walczy lub odpoczywa; nie ubiega się o dostęp.
2. `TRYING` — okręt wysłał `REQ` i czeka, aż spełnione będą wszystkie warunki
   wejścia (`W1 ∧ W2 ∧ W3`).
3. `INSECTION` — okręt jest naprawiany w doku; trzyma dok i `m` mechaników.

## Szkic algorytmu

1. Proces `i`, decydując się na powrót do bazy, oblicza `m_i := Uniform[1, M]`,
   pobiera nowy znacznik czasowy `ts_i := L+1`, wyznacza dok
   `d_i := ((ts_i + i) mod K) + 1`, wstawia własne żądanie do `Q` i wysyła
   `REQ(ts_i, i, m_i, d_i)` do wszystkich pozostałych procesów. Następnie
   przechodzi w stan `TRYING`.
2. Każdy odbiorca `REQ` wstawia żądanie do swojej kopii `Q` i odsyła `ACK`.
3. Proces wchodzi do sekcji (`INSECTION`), gdy spełnione są jednocześnie:
   1. **W1** — od każdego innego procesu otrzymał wiadomość ze znacznikiem
      ściśle większym niż `ts_i` (klasyczny warunek Lamporta);
   2. **W2** — w `Q` żadne żądanie poprzedzające `my_req` w sortowaniu
      `(ts, pid)` nie ma tego samego doku co `my_req`;
   3. **W3** — suma `m_j` po wszystkich poprzednikach `my_req` w `Q`
      plus `m_i` nie przekracza `M`.
4. Po zakończeniu naprawy proces usuwa własne żądanie z `Q` i wysyła
   `RELEASE(L, i)` do wszystkich pozostałych. Atomowo zwalnia jednocześnie
   dok i mechaników.
5. Przydział doku jest deterministyczną funkcją `(ts_i, i, K)` i nie zmienia
   się w czasie życia żądania — każdy proces, który zna `(ts, pid, K)`
   żądania, oblicza ten sam dok bez dodatkowej komunikacji.

## Opis szczegółowy algorytmu dla procesu `i`

### `REST`

1. Stan początkowy. Proces nie ma żądania w `Q`.
2. Proces przebywa w `REST` do chwili podjęcia decyzji o powrocie z walki.
   Wówczas: `L := L+1`, `ts_i := L`, `m_i := Uniform[1, M]`,
   `d_i := ((ts_i + i) mod K) + 1`; wstawia `{ts_i, i, m_i, d_i}` do `Q`,
   wysyła `REQ(ts_i, i, m_i, d_i)` do wszystkich innych procesów i przechodzi
   do `TRYING`.
3. Reakcje na wiadomości:
   1. `REQ(ts, j, m, d)` — wstawia żądanie do `Q`, aktualizuje zegar
      (`L := max(L, ts) + 1`, `last_seen[j] := ts`), wysyła `ACK(L)` do `j`.
   2. `ACK(ts)` — aktualizuje zegar i `last_seen[j]`. Brak innych skutków.
   3. `RELEASE(ts, j)` — aktualizuje zegar i `last_seen[j]`, usuwa żądanie
      procesu `j` z `Q`.

### `TRYING`

1. Ubieganie się o sekcję krytyczną. Proces ma własne żądanie w `Q`.
2. Po obsłudze każdej przychodzącej wiadomości proces sprawdza warunki
   `W1 ∧ W2 ∧ W3` (definicje w *Szkicu algorytmu*, punkt 3). Gdy są spełnione
   równocześnie, przechodzi do stanu `INSECTION` i rozpoczyna naprawę
   w doku `d_i` z `m_i` mechanikami.
3. Reakcje na wiadomości:
   1. `REQ(ts, j, m, d)` — wstawia żądanie do `Q`, aktualizuje zegar
      i `last_seen[j]`, wysyła `ACK(L)` do `j`. Następnie sprawdza warunki
      wejścia.
   2. `ACK(ts)` — aktualizuje zegar i `last_seen[j]` (to ta wiadomość
      gwarantuje `W1` dla procesu `j`). Następnie sprawdza warunki wejścia.
   3. `RELEASE(ts, j)` — aktualizuje zegar i `last_seen[j]`, usuwa żądanie
      procesu `j` z `Q`. Następnie sprawdza warunki wejścia (usunięcie
      poprzednika może odblokować `W2` lub `W3`).

### `INSECTION`

1. Przebywanie w sekcji krytycznej — okręt jest w doku `d_i` i zajmuje `m_i`
   mechaników.
2. Proces przebywa w `INSECTION` do czasu zakończenia naprawy. Po jej
   zakończeniu usuwa własne żądanie z `Q`, wykonuje `L := L+1`, wysyła
   `RELEASE(L, i)` do wszystkich pozostałych procesów i przechodzi do `REST`.
3. Reakcje na wiadomości:
   1. `REQ(ts, j, m, d)` — wstawia żądanie do `Q`, aktualizuje zegar
      i `last_seen[j]`, wysyła `ACK(L)` do `j` (proces nadal odpowiada,
      by nie blokować innych w stanie `TRYING`).
   2. `ACK(ts)` — aktualizuje zegar i `last_seen[j]`.
   3. `RELEASE(ts, j)` — aktualizuje zegar i `last_seen[j]`, usuwa żądanie
      procesu `j` z `Q`.

## Poprawność (skrót)

1. **Wzajemne wykluczanie na doku.** Jeśli dwa procesy `A` i `B` byłyby
   równocześnie `INSECTION` na tym samym doku `d`, bez utraty ogólności
   `(ts_A, pid_A) < (ts_B, pid_B)`. Z `W1` wynika `last_seen_B[A] > ts_B`,
   więc `B` widział wiadomość od `A` ze znacznikiem większym od `ts_B`.
   Z FIFO i monotoniczności znaczników wynika, że `REQ_A` jest w `Q_B`
   przy sprawdzaniu `W2`. Ponieważ `(ts_A, pid_A) < (ts_B, pid_B)` i
   `dok_A = dok_B = d`, warunek `W2` dla `B` jest fałszywy — sprzeczność.
2. **Niezmiennik mechaników.** Analogicznie, każdy poprzednik `B` jest w `Q_B`
   przy sprawdzaniu `W3`. Suma `m_j` poprzedników plus `m_B` nie przekracza
   `M`. Następnicy nie wpływają na ten warunek (nie wejdą przed `B`).
3. **Brak zakleszczenia.** Istnieje jedna wspólna kolejka i jeden zestaw
   warunków wejścia. Najwyżej priorytetowe żądanie ma puste poprzedniki,
   więc `W2` i `W3` są spełnione trywialnie; po `W1` wchodzi do `INSECTION`.
   Brak źródła zakleszczenia.
4. **Brak głodzenia.** Zegary Lamporta są monotoniczne, więc żądanie ma przed
   sobą skończoną liczbę poprzedników; każdy z nich ostatecznie się zwolni.

## Złożoność

1. **Czasowa**: 3 rundy (`REQ → ACK → RELEASE`), tak jak w klasycznym
   algorytmie Lamporta.
2. **Komunikacyjna**: `3·(N−1)` wiadomości na pojedyncze wejście do sekcji
   krytycznej.

Algorytm pozostaje w klasie złożoności klasycznego algorytmu Lamporta —
rozszerzenie o `W2` i `W3` jest lokalne i nie wymaga dodatkowej komunikacji.
