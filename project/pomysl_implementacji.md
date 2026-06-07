## Ogólna idea

Rozszerzamy klasyczny algorytm Lamporta o dwa dodatkowe zasoby: rozróżnialne doki i nierozróżnialni mechanicy. Kluczowy trick polega na tym, że zamiast osobnych mechanizmów dla każdego zasobu, trzymamy jedną wspólną kolejkę żądań posortowaną priorytetami Lamporta. Wejście do sekcji krytycznej wymaga spełnienia trzech warunków jednocześnie - jeden klasyczny (Lamport), dwa nowe (dok i mechanicy).

Każde żądanie od razu, przy tworzeniu, dostaje przypisany konkretny dok. Liczymy go deterministycznie ze znacznika czasu i identyfikatora procesu, żeby każdy inny proces mógł to samo wyliczyć bez żadnej dodatkowej komunikacji.

---

## Co trzyma każdy proces

Każdy okręt lokalnie trzyma:
- własny zegar logiczny Lamporta - rośnie przy każdym zdarzeniu
- swoją kopię globalnej kolejki żądań - każde żądanie zawiera: kto wysłał, kiedy, ile mechaników chce i do którego doku
- tablicę ostatnio widzianych znaczników od każdego innego procesu - potrzebna do klasycznego warunku Lamporta
- własny stan: odpoczynek, ubieganie się lub sekcja krytyczna

---

## Stany procesu

### REST - odpoczynek

Okręt walczy albo odpczywa, nie ubiega się o żadne zasoby. W tym czasie normalnie obsługuje wiadomości od innych - wstawia ich żądania do kolejki i odsyła potwierdzenia. To ważne, bo bez naszych potwierdzeń inne okręty nigdy nie wejdą do sekcji.

Kiedy okręt decyduje się wrócić do bazy, losuje sobie ile mechaników potrzebuje, wylicza swój dok (deterministycznie), wstawia żądanie do kolejki i rozsyła je do wszystkich. Przechodzi do TRYING.

### TRYING - ubieganie się

Okręt czeka na spełnienie trzech warunków jednocześnie. Po każdej odebranej wiadomości sprawdza czy już może wejść. Nadal obsługuje wiadomości od innych i odpowiada na ich REQ-i.

**Warunek 1 - klasyczny Lamport:**
Od każdego innego procesu musimy dostać jakąś wiadomość ze znacznikiem nowszym niż nasz własny znacznik żądania. To gwarantuje, że każdy wie o naszym żądaniu - bo kanały są FIFO, a skoro mamy nowszą wiadomość, nasza musiała do niego dotrzeć wcześniej.

**Warunek 2 - dok:**
Patrząc na kolejkę, żaden proces przed nami (w sensie priorytetu Lamporta) nie może chcieć tego samego doku co my. Jeśli ktoś z wyższym priorytetem chce tego samego doku, my czekamy.

**Warunek 3 - mechanicy:**
Sumujemy MAX mechaników z kazdego doku w kolejce, czyli np. proces1  D2 M3 oraz proces2 D2 M4 to bierzemy 4 mechaników. Jeśli ta suma plus nasza liczba mechaników nie przekracza pojemności, możemy wejść. Jeśli nie ma miejsca, czekamy aż poprzednicy zwolnią.

Gdy wszystkie trzy warunki są spełnione jednocześnie - wchodzimy do INSECTION.

### INSECTION - sekcja krytyczna

Okręt jest w swoim doku z przydzielonymi mechanikami. Nadal obsługuje wiadomości od innych (wstawia REQ-i do kolejki i potwierdza) - musimy to robić, żeby nie blokować innych w stanie TRYING.

Po zakończeniu naprawy okręt usuwa swoje żądanie z kolejki i rozsyła RELEASE do wszystkich. Atomowo zwalnia dok i mechaników jednocześnie - nie ma osobnego zwalniania każdego zasobu z osobna. Przechodzi do REST.

---