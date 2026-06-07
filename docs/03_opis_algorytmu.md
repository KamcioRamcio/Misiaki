# Opis algorytmu — Okręty / Misie

**Algorytm Lamport-Dual**
N okrętów, K rozróżnialnych doków, M nierozróżnialnych mechaników.

---

## OPIS OGÓLNY

```
L01. Każdy proces MPI reprezentuje jeden okręt działający w nieskończonej
     pętli stanów REST → TRYING → INSECTION → REST.

L02. Przy wejściu do fazy TRYING okręt i losuje zapotrzebowanie na
     mechaniki m_i ∈ {1, ..., M} oraz oblicza deterministycznie dok:
         d_i := ((ts_i + i) mod K) + 1,
     gdzie ts_i to wartość lokalnego zegara Lamporta po clock_tick().

L03. Okręt wysyła REQ(ts_i, i, m_i, d_i) do wszystkich pozostałych
     N−1 procesów i jednocześnie wstawia własne żądanie do lokalnej
     kolejki Q. Następnie przechodzi do stanu TRYING.

L04. Każdy odbiorca REQ wstawia żądanie do swojej kopii Q
     i natychmiast odsyła ACK — niezależnie od własnego stanu.
     Brak opóźniania ACK (algorytm Lamporta, nie Ricarta-Agrawali).

L05. Każdy proces utrzymuje lokalną kopię kolejki Q posortowaną
     rosnąco po parze (ts, pid) (mniejsza para = wyższy priorytet)
     oraz wektor last_seen[N], gdzie last_seen[k] = znacznik Lamporta
     ostatniej wiadomości od procesu k.

L06. Okręt wchodzi do INSECTION gdy spełnione są jednocześnie:
         W1: ∀k ≠ i : last_seen[k] > ts_i
             (każdy inny wie o naszym żądaniu; gwarantowane przez
              kanały FIFO i monotoniczność zegara Lamporta),
         W2: żaden poprzednik my_req w Q (p z (ts_p,pid_p) < (ts_i,i))
             nie ma p.dok == d_i
             (nikt ważniejszy nie czeka na nasz dok),
         W3: (Σ_{d=1}^{K} max{p.m : p ∈ pred(my_req), p.dok = d})
             + m_i ≤ M,
             gdzie pred(my_req) = {p ∈ Q : (ts_p,pid_p) < (ts_i,i)}
             (pesymistyczna suma mechaników poprzedników + nasza
             liczba mechaników mieści się w puli M).

L07. Po zakończeniu naprawy okręt usuwa własne żądanie z Q i wysyła
     RELEASE(ts, i) do wszystkich. Jeden RELEASE zwalnia jednocześnie
     dok i mechaniki — brak osobnych wiadomości per zasób.

L08. Reguła d_i = ((ts_i + i) mod K) + 1 jest czystą funkcją
     (ts_i, i, K). Każdy proces znający treść REQ oblicza ten sam
     dok bez dodatkowej komunikacji.

L09. Brak centralnego koordynatora. Wszystkie decyzje o wejściu
     do sekcji krytycznej są podejmowane lokalnie na podstawie
     lokalnej kopii Q i wektora last_seen.
```

---

## STANY PROCESÓW

```
L10. REST
     Okręt walczy lub odpoczywa między rundami. Brak aktywnego
     żądania w Q (MY_REQ = NULL). W tym stanie proces nadal obsługuje
     wiadomości od innych (REQ → wstawia do Q i odpowiada ACK;
     RELEASE → usuwa z Q), by nie blokować ich postępu.

L11. TRYING
     Okręt wysłał REQ. Czeka aż spełnione będą jednocześnie
     warunki W1 ∧ W2 ∧ W3. Po każdej odebranej wiadomości
     sprawdza warunki. Nadal odpowiada ACK na REQ od innych.

L12. INSECTION
     Okręt jest naprawiany w doku d_i, zajmuje m_i mechaników.
     Obydwa zasoby trzymane atomowo przez cały czas naprawy.
     Nadal obsługuje wiadomości — w szczególności odpowiada ACK
     na REQ, żeby nie blokować innych w stanie TRYING.
```

---

## TYPY WIADOMOŚCI

```
L13. REQ(ts, pid, m, dok)
     Prośba o dostęp do sekcji krytycznej. Niesie pełną treść
     żądania: znacznik czasowy ts, identyfikator nadawcy pid,
     zapotrzebowanie na mechaniki m, przypisany dok.
     Wysyłana do wszystkich N−1 pozostałych procesów przy
     przejściu REST → TRYING.
     Reguła zegara przy wysyłaniu: ts := clock_tick() przed REQ.

L14. ACK(ts, pid)
     Potwierdzenie odbioru REQ. Znacznik ts(ACK) > ts(REQ)
     (wynika z reguły zegara Lamporta: L := max(L,ts)+1).
     Wysyłana natychmiast po wstawieniu odebranego REQ do Q.
     Aktualizuje last_seen[pid] u odbiorcy, co może domknąć W1.

L15. RELEASE(ts, pid)
     Informacja o zwolnieniu obu zasobów przez pid. Powoduje
     usunięcie żądania pid z lokalnej kopii Q u każdego odbiorcy.
     Jedno RELEASE zwalnia dok i mechaniki jednocześnie.
     Wysyłana do wszystkich N−1 po wyjściu z sekcji.
```

---

## OPIS SZCZEGÓŁOWY

### Stan REST

```
L16. Stan początkowy procesu. Proces nie ma żądania w Q.

L17. Po podjęciu decyzji o wejściu do bazy proces wykonuje:
     1. m_i := Uniform{1, ..., M}
     2. L := L + 1;  ts_i := L
     3. d_i := ((ts_i + i) mod K) + 1
     4. wstaw {ts_i, i, m_i, d_i} do Q
     5. MY_REQ := wskaźnik na wstawiony element
     6. state := TRYING
     7. wyślij REQ(ts_i, i, m_i, d_i) do wszystkich k ≠ i
     Krok 4 przed krokiem 7: własne żądanie jest w Q zanim REQ
     dotrze do jakiegokolwiek innego procesu.

L18. Reakcje na wiadomości w stanie REST:
     case REQ(ts, j, m, d):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         wstaw {ts, j, m, d} do Q;
         L := L + 1;
         wyślij ACK(L, i) do j.
     case ACK(ts, j):
         L := max(L, ts) + 1;  last_seen[j] := ts.
     case RELEASE(ts, j):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         usuń żądanie j z Q.
```

### Stan TRYING

```
L19. Proces ma własne żądanie my_req w Q i czeka na wejście
     do sekcji krytycznej.

L20. Po obsłudze każdej odebranej wiadomości sprawdza W1∧W2∧W3:
     W1: ∀k ≠ i : last_seen[k] > ts_i
     W2: ¬∃p ∈ Q : (ts_p,pid_p) < (ts_i,i) ∧ p.dok = d_i
     W3: (Σ_{d=1}^{K} max{p.m : p ∈ pred(my_req), p.dok=d})
         + m_i ≤ M
     Gdy W1 ∧ W2 ∧ W3: state := INSECTION.

L21. Reakcje na wiadomości w stanie TRYING (identyczne z REST,
     z dodatkowym sprawdzeniem warunków po każdej obsłudze):
     case REQ(ts, j, m, d):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         wstaw {ts, j, m, d} do Q;
         L := L + 1;
         wyślij ACK(L, i) do j;
         sprawdź W1 ∧ W2 ∧ W3.
     case ACK(ts, j):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         sprawdź W1 ∧ W2 ∧ W3.
     case RELEASE(ts, j):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         usuń żądanie j z Q;
         sprawdź W1 ∧ W2 ∧ W3.
```

### Stan INSECTION

```
L22. Okręt jest naprawiany w doku d_i z przydzielonymi m_i
     mechanikami.

L23. Nadal odbiera wiadomości i odpowiada ACK na REQ, by nie
     blokować postępu procesów w stanie TRYING (ich W1 wymaga
     otrzymania wiadomości od każdego procesu).

L24. Po zakończeniu naprawy:
     1. usuń my_req z Q
     2. MY_REQ := NULL
     3. L := L + 1;  rel_ts := L
     4. wyślij RELEASE(rel_ts, i) do wszystkich k ≠ i
     5. state := REST

L25. Reakcje na wiadomości w stanie INSECTION:
     case REQ(ts, j, m, d):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         wstaw {ts, j, m, d} do Q;
         L := L + 1;
         wyślij ACK(L, i) do j.
     case ACK(ts, j):
         L := max(L, ts) + 1;  last_seen[j] := ts.
     case RELEASE(ts, j):
         L := max(L, ts) + 1;  last_seen[j] := ts;
         usuń żądanie j z Q.
```

---

## POPRAWNOŚĆ (SKRÓT)

```
L26. Wzajemne wykluczanie na doku (I1): co najwyżej 1 okręt
     na danym doku d jednocześnie w INSECTION.
     Dowód: niech A i B w INSECTION z tym samym dokiem d,
     (ts_A,A) < (ts_B,B). Z W1 dla B: last_seen_B[A] > ts_B > ts_A.
     Kanały FIFO ⇒ REQ_A dotarło do B przed ACK(ts>ts_B).
     Zatem REQ_A było w Q_B gdy B sprawdzało W2.
     A jest poprzednikiem B z tym samym dokiem ⇒ W2 fałszywe.
     Sprzeczność — B nie mogło wejść. ∎

L27. Ograniczenie mechaników (I2): Σ m_k ≤ M dla aktywnych.
     Z I1: na każdym doku ≤ 1 aktywny okręt. Per-dok max
     w W3 = realnie zajęte mechaniki na tym doku (max jest
     osiągany przez jedynego aktywnego). W3 gwarantuje
     total ≤ M w chwili każdego wejścia. ∎

L28. Brak zakleszczenia.
     Żądanie o globalnie najniższym (ts, pid) ma pustą pred.
     W3: m_i ≤ M (m_i ∈ [1,M]) — spełnione.
     W2: brak poprzedników — spełnione.
     W1 domknie się po odebraniu ACK od wszystkich (każdy
     wysyła ACK natychmiast po REQ). Wchodzi, wysyła RELEASE,
     odblokowuje kolejnych. Indukcja po (ts,pid). ∎

L29. Brak głodzenia.
     Zegar Lamporta rośnie monotonicznie ⇒ każde żądanie ma
     skończoną liczbę poprzedników. Każdy poprzednik eventualnie
     wchodzi do sekcji i wysyła RELEASE (z L28). Po usunięciu
     wszystkich poprzedników z tym samym dokiem (W2) i zwolnieniu
     wystarczającej liczby mechaników (W3) — żądanie wejdzie. ∎
```

---

## ZŁOŻONOŚĆ

```
L30. Złożoność czasowa: 3 rundy komunikacyjne.
     REQ (runda 1) → ACK (runda 2) → RELEASE (runda 3).
     Zgodna ze złożonością klasycznego algorytmu Lamporta.

L31. Złożoność komunikacyjna: 3·(N−1) wiadomości na jedno
     wejście i wyjście z sekcji krytycznej.
     (N−1 REQ) + (N−1 ACK) + (N−1 RELEASE).

L32. Warunki W2 i W3 są obliczeniami czysto lokalnymi na
     lokalnej kopii kolejki Q. Nie wprowadzają żadnych nowych
     typów wiadomości ani dodatkowych rund komunikacyjnych.
     Kosztem jest obliczenie O(|Q|) per sprawdzenie warunków.
```

---

*Koniec opisu algorytmu.*
