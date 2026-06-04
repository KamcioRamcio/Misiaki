# Opis algorytmu Lamport-Dual (okręty, doki, mechanicy)

Algorytm rozszerza algorytm Lamporta z `project/rozproszona_sekcja_krytyczna.md`
o równoczesny przydział doku rozróżnialnego oraz mechaników nierozróżnialnych
w jednej, wspólnej kolejce żądań.

---

## OPIS OGÓLNY

```
L01. Każdy proces MPI reprezentuje jeden okręt działający w pętli
     REST → TRYING → INSECTION → REST.
L02. Przy powrocie z walki okręt i losuje zapotrzebowanie m_i ∈ [1, M]
     mechaników oraz oblicza dok d_i := ((ts_i + i) mod K) + 1,
     gdzie ts_i to bieżący zegar Lamporta.
L03. Okręt wysyła REQ(ts_i, i, m_i, d_i) do wszystkich pozostałych
     procesów i wstawia własne żądanie do lokalnej kolejki Q.
L04. Każdy odbiorca REQ wstawia żądanie do swojej kopii Q i natychmiast
     odsyła ACK. Odpowiada na każde REQ niezależnie od własnego stanu.
L05. Każdy proces utrzymuje lokalną kopię globalnej kolejki żądań Q
     posortowaną po (ts, pid) rosnąco oraz wektor last_seen[N]
     znaczników ostatnio odebranych wiadomości od każdego procesu.
L06. Okręt wchodzi do sekcji krytycznej (INSECTION), gdy spełnione są
     jednocześnie trzy warunki:
       W1 — od każdego innego procesu odebrano wiadomość ze znacznikiem
            ściśle większym niż ts_i (klasyczny warunek Lamporta),
       W2 — żaden poprzednik my_req w Q (w sensie porządku (ts, pid))
            nie ma tego samego doku co my_req,
       W3 — suma per-dok maxów m_j po poprzednikach plus m_i ≤ M
            (definicja w L12 poniżej).
L07. Po zakończeniu naprawy okręt usuwa własne żądanie z Q i wysyła
     RELEASE do wszystkich. Atomowo zwalnia dok i mechaników.
L08. Przydział doku jest deterministyczną funkcją (ts_i, i, K) —
     każdy proces zna dok żądania bez dodatkowej komunikacji.
L09. Brak centralnego koordynatora; wszystkie decyzje lokalne.
```

---

## STANY PROCESÓW

```
L10. REST     — okręt walczy lub odpoczywa; nie ubiega się o zasoby;
               nie ma własnego żądania w Q.
L11. TRYING   — okręt wysłał REQ i czeka aż W1 ∧ W2 ∧ W3 będą
               spełnione równocześnie.
L12. INSECTION — okręt jest naprawiany w doku d_i, zajmuje m_i
               mechaników; trzyma oba zasoby atomowo.
```

---

## TYPY WIADOMOŚCI

```
L13. REQ(ts, pid, m, dok)
     Prośba o dostęp. Niesie pełną treść żądania: znacznik czasowy ts,
     nadawcę pid, zapotrzebowanie m mechaników i przypisany dok.
     Wysyłana do wszystkich pozostałych procesów przy przejściu
     REST → TRYING.

L14. ACK(ts, pid)
     Potwierdzenie odbioru REQ. Znacznik ACK spełnia ts(ACK) > ts(REQ).
     Wysyłana natychmiast po wstawieniu odebranego REQ do Q.
     Spełnia warunek W1 dla nadawcy ACK po stronie odbiorcy.

L15. RELEASE(ts, pid)
     Informacja o zwolnieniu zasobów przez pid. Powoduje usunięcie
     żądania nadawcy z lokalnej kopii Q. Zwalnia jednocześnie dok
     i mechaników (brak osobnych wiadomości per zasób).
```

---

## OPIS SZCZEGÓŁOWY

### Stan REST

```
L16. Stan początkowy.
L17. Proces przebywa w REST do czasu podjęcia decyzji o powrocie z walki.
     Wówczas:
       L := L + 1,
       ts_i := L,
       m_i := Uniform[1, M],
       d_i := ((ts_i + i) mod K) + 1,
     wstawia {ts_i, i, m_i, d_i} do Q, wysyła REQ(ts_i, i, m_i, d_i)
     do wszystkich innych procesów, przechodzi do TRYING.
L18. Reakcje na wiadomości:
     case REQ(ts, j, m, d):
       L := max(L, ts) + 1; last_seen[j] := ts;
       wstaw {ts, j, m, d} do Q;
       wyślij ACK(L, i) do j.
     case ACK(ts, j):
       L := max(L, ts) + 1; last_seen[j] := ts.
     case RELEASE(ts, j):
       L := max(L, ts) + 1; last_seen[j] := ts;
       usuń żądanie j z Q.
```

### Stan TRYING

```
L19. Proces ma własne żądanie my_req w Q i czeka na wejście.
L20. Po obsłudze każdej odebranej wiadomości sprawdza W1 ∧ W2 ∧ W3:
     W1: ∀k ≠ i : last_seen[k] > ts_i
     W2: ∀p ∈ Q : (ts_p, pid_p) < (ts_i, i) ∧ p.dok = d_i  →  false
     W3: (Σ_{d=1}^{K} max{p.m : p ∈ pred(my_req), p.dok = d})
         + m_i ≤ M,
         gdzie pred(my_req) = {p ∈ Q : (ts_p, pid_p) < (ts_i, i)}.
     Gdy wszystkie prawdziwe, przechodzi do INSECTION.
L21. Reakcje na wiadomości (identyczne z REST, z dodatkowym sprawdzeniem):
     case REQ(ts, j, m, d):
       L := max(L, ts) + 1; last_seen[j] := ts;
       wstaw {ts, j, m, d} do Q;
       wyślij ACK(L, i) do j;
       sprawdź W1 ∧ W2 ∧ W3.
     case ACK(ts, j):
       L := max(L, ts) + 1; last_seen[j] := ts;
       sprawdź W1 ∧ W2 ∧ W3.
     case RELEASE(ts, j):
       L := max(L, ts) + 1; last_seen[j] := ts;
       usuń żądanie j z Q;
       sprawdź W1 ∧ W2 ∧ W3.
```

### Stan INSECTION

```
L22. Okręt jest w doku d_i i zajmuje m_i mechaników.
L23. Nadal odbiera wiadomości i odpowiada ACK na REQ, aby nie blokować
     postępu innych procesów w stanie TRYING.
L24. Po zakończeniu naprawy:
       usuń my_req z Q,
       L := L + 1,
       wyślij RELEASE(L, i) do wszystkich innych procesów,
       przejdź do REST.
L25. Reakcje na wiadomości:
     case REQ(ts, j, m, d):
       L := max(L, ts) + 1; last_seen[j] := ts;
       wstaw {ts, j, m, d} do Q;
       wyślij ACK(L, i) do j.
     case ACK(ts, j):
       L := max(L, ts) + 1; last_seen[j] := ts.
     case RELEASE(ts, j):
       L := max(L, ts) + 1; last_seen[j] := ts;
       usuń żądanie j z Q.
```

---

## POPRAWNOŚĆ (SKRÓT)

```
L26. Wzajemne wykluczanie na doku (I1).
     Gdyby A i B były równocześnie w INSECTION na doku d, BOOG:
     niech (ts_A, A) < (ts_B, B). Z W1 mamy last_seen_B[A] > ts_B > ts_A,
     więc ze względu na FIFO kanałów REQ_A dotarło do B przed czasem B
     sprawdził W2. W chwili sprawdzania W2 przez B żądanie A jest w Q_B
     z tym samym dokiem — warunek W2 fałszywy. Sprzeczność.

L27. Ograniczenie mechaników (I2).
     W każdej chwili okręty będące w INSECTION mają parami różne doki
     (z I1). Ich łączne zapotrzebowanie = Σ m_k ≤ Σ_d max{m : dok=d}
     po zbiorze poprzedników, który był spójny w chwili sprawdzania W3.
     Warunek W3 gwarantuje, że ten max-sum + m_i ≤ M.

L28. Brak zakleszczenia.
     Żądanie o globalnie najniższym (ts, pid) ma pustą listę poprzedników.
     W3 = m_i ≤ M (trywialne), W2 = brak poprzedników = true.
     Po domknięciu W1 (otrzymaniu ACK od wszystkich) wchodzi do sekcji.
     RELEASE odblokowuje kolejne procesy.

L29. Brak głodzenia.
     Zegar Lamporta rośnie monotonicznie, więc każde żądanie ma skończoną
     liczbę poprzedników w sensie (ts, pid). Każdy poprzednik ostatecznie
     opuszcza sekcję i znika z Q (RELEASE). Po usunięciu wszystkich
     poprzedników z tym samym dokiem (W2) i wystarczającej liczbie
     RELEASE (W3) — żądanie wejdzie.
```

---

## ZŁOŻONOŚĆ

```
L30. Czasowa: 3 rundy (REQ → ACK → RELEASE) — tak jak klasyczny Lamport.
L31. Komunikacyjna: 3·(N−1) wiadomości na pojedyncze wejście do sekcji.
L32. Rozszerzenie o W2 i W3 jest obliczeniem lokalnym na Q —
     nie dodaje żadnych nowych typów wiadomości ani rund komunikacji.
```
