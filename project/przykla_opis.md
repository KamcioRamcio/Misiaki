## Struktury i zmienne

1. `WaitQueue` - kolejka procesów oczekujących na ACK, początkowo pusta.
2. `AckNum` - liczba otrzymanych potwierdzeń ACK, początkowo 0.
3. `n` - liczba procesów.

## Wiadomości

Wszystkie wiadomości są podbite znacznikiem czasowym (timestampem), modyfikowanym zgodnie z zasadami skalarnego zegara logicznego Lamporta.

1. `REQ` - żądanie o dostęp do sekcji krytycznej. Zawiera priorytet żądania.
2. `ACK` - potwierdzenie dostępu do sekcji krytycznej.

## Stany

Początkowym stanem procesu jest `REST`.

1. `REST` - nie ubiega się o dostęp.
2. `WAIT` - czeka na dostęp do sekcji krytycznej.
3. `INSECTION` - w sekcji krytycznej.

## Szkic algorytmu

1. Proces `i`, ubiegający się o wejście do sekcji krytycznej, wysyła do wszystkich pozostałych prośby `REQ` o dostęp. Pozostałe procesy odsyłają `ACK` do procesu `i`, o ile same się nie ubiegają o dostęp albo jeżeli priorytet ich żądania jest mniejszy od priorytetu procesu `i`. W przeciwnym wypadku zapamiętują `REQ` w kolejce `WaitQueue` i odsyłają `ACK` po wyjściu z sekcji krytycznej. Proces wchodzi do sekcji po zebraniu ACK-ów od wszystkich.
2. Uwaga: im większy zegar Lamporta, tym mniejszy priorytet.

## Opis szczegółowy algorytmu dla procesu `i`

### `REST`

1. Stan początkowy.
2. Proces `i` przebywa w stanie `REST` do czasu, aż podejmie decyzję o ubieganiu się o sekcję krytyczną. Ze stanu `REST` następuje przejście do stanu `WAIT` po uprzednim wysłaniu wiadomości `REQ` do wszystkich innych procesów oraz ustawieniu `AckCounter` na zero. Wszystkie wiadomości `REQ` są opisane tym samym priorytetem, równym zegarowi Lamporta w chwili wysłania pierwszej wiadomości `REQ`.
3. Reakcje na wiadomości:
   1. `REQ`: odsyła `ACK`.
   2. `ACK`: ignoruje (sytuacja niemożliwa).

### `WAIT`

1. Ubieganie się o sekcję krytyczną.
2. Ze stanu `WAIT` następuje przejście do stanu `INSECTION` pod warunkiem, że proces otrzyma `ACK` od wszystkich innych procesów (`AckCounter == n - 1`).
3. Reakcje na wiadomości:
   1. `REQ` od procesu `j`: jeżeli priorytet zawarty w `REQ` jest większy od priorytetu `i` (pamiętamy: większe wartości oznaczają mniejszy priorytet), odsyła `ACK`. W przeciwnym wypadku `REQ` zapamiętywany jest w kolejce `WaitQueue`.
   2. `ACK`: zwiększa licznik otrzymanych ACK (`AckCounter++`). Tak jak opisano wyżej, gdy otrzymano `ACK` od wszystkich pozostałych procesów, proces `i` przechodzi do stanu `INSECTION`.

### `INSECTION`

1. Przebywanie w sekcji krytycznej.
2. Proces przebywa w sekcji krytycznej do czasu podjęcia decyzji o jej opuszczeniu. Po podjęciu decyzji o opuszczeniu sekcji proces wysyła `ACK` w reakcji na wszystkie `REQ` znajdujące się w `WaitQueue`, a następnie przechodzi do stanu `REST`.
3. Reakcje na wiadomości:
   1. `REQ`: dodaje żądanie do kolejki `WaitQueue`.
   2. `ACK`: niemożliwe, ignorowane.

---

Koniec.
