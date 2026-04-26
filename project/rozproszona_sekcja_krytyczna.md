# Rozproszona sekcja krytyczna

## Czym jest rozproszona sekcja krytyczna?

Jeżeli w środowisku rozproszonym mamy $n$ procesów ubiegających się o dostęp do zasobu,
z którego tylko jeden naraz może skorzystać, mamy do czynienia z rozproszoną sekcją
krytyczną.

Jeżeli zasób równocześnie może używać $k$ procesów, mamy do czynienia z uogólnioną
sekcją krytyczną o pojemności $k$.

### Przykład

Mamy osiedle domków jednorodzinnych w czasie pandemii oraz sklepik z żywnością.
Tylko jedna osoba naraz może być w sklepiku. Osoby więc w jakiś sposób, przy pomocy
komórek i SMS-ów, ustalają, kto może w danej chwili wejść do sklepiku.

Jeżeli w sklepiku naraz mogą się znaleźć trzy osoby, dalej mamy sekcję krytyczną,
tyle że o pojemności trzy.

## Rozwiązanie trywialne

Najprościej rozwiązać problem w sposób scentralizowany:

1. Desygnujemy jeden proces, by działał jako zarządca.
2. Zarządca posiada kolejkę żądań.
3. Każdy proces wysyła żądanie do zarządcy.
4. Zarządca kolejkuje żądania według kolejności zgłoszeń i wysyła zgodę jednemu
     procesowi naraz.
5. Proces, wychodząc z sekcji krytycznej, powiadamia zarządcę.
6. Zarządca usuwa proces z kolejki i wybiera kolejny, któremu może wysłać zezwolenie
     na wejście do sekcji.

Oczywiście na przedmiocie o nazwie *Przetwarzanie Rozproszone* nie możemy patrzeć na
takie rozwiązanie bez obrzydzenia. Rozwiązania scentralizowane posiadają mnóstwo zalet
oraz sporo wad. Nam jednak zależy (nam, to znaczy prowadzącym i wam, studentom; nie
macie wyjścia, musi wam zależeć), by spróbować stworzyć algorytm obywający się bez
centralnego zarządcy.

## Naiwne i błędne rozwiązanie

Pierwszym podejściem do rozwiązania może być wymóg, by proces prosił pozostałe
procesy o zgodę.

Dla ułatwienia rozważmy tylko jedną rundę (tj. wybieramy tylko jeden proces z wielu
ubiegających się i nie dbamy, co dzieje się dalej).

### Założenia

Używamy dwóch typów wiadomości:

- `REQ` - prośba o dostęp do sekcji,
- `ACK` - udzielenie zgody na dostęp do sekcji.

Proces $i$-ty ubiegający się o dostęp do sekcji postępuje następująco:

1. Proces wysyła `REQ` do wszystkich procesów.
2. Proces dostaje się do sekcji po zebraniu `ACK` od wszystkich procesów.

W każdej chwili proces reaguje na wiadomości:

- Po otrzymaniu `REQ`:
    - jeżeli proces nie ubiega się o dostęp do sekcji, wysyła `ACK`,
    - jeżeli proces ubiega się o dostęp do sekcji, nic nie wysyła.
- Po otrzymaniu `ACK`:
    - proces zwiększa licznik otrzymanych `ACK` i wchodzi do sekcji, jeżeli otrzymał `ACK`
        od wszystkich procesów.

Widać na pierwszy rzut oka, że taki algorytm nie spełnia warunku postępu: jeżeli dwa
procesy równocześnie ubiegają się o wejście do sekcji, żaden z nich nie wyśle drugiemu
zgody i żaden z nich nie otrzyma dostępu.

### Modyfikacja z priorytetem (również błędna)

Zmodyfikujmy algorytm następująco:

- żądania procesów mają przydzielony priorytet,
- po otrzymaniu `REQ`:
    - jeżeli proces nie ubiega się o dostęp do sekcji, wysyła `ACK`, o ile nie wysłał wcześniej
        `ACK`,
    - jeżeli proces ubiega się o dostęp do sekcji i ma niższy priorytet, wysyła `ACK`, o ile nie
        wysłał wcześniej `ACK`,
    - jeżeli proces ubiega się o dostęp do sekcji i ma wyższy priorytet, nic nie wysyła.

Również ta wersja nie spełnia warunku postępu.

Załóżmy trzy procesy: `P1`, `P2`, `P3`. `P1` oraz `P2` ubiegają się o wejście do sekcji
krytycznej, a `P1` ma wyższy priorytet od `P2`:

1. `P1` wysyła `REQ1` do `P2` oraz `P3`.
2. `P2` wysyła `REQ2` do `P1` oraz `P3`.
3. `P2` odbiera `REQ1` od `P1` i odpowiada `ACK`.
4. `P3` odbiera `REQ2` od `P2` i odpowiada `ACK`.
5. `P3` odbiera `REQ1` od `P1` i nie odpowiada (bo już wysłał `ACK` do `P2`).
6. `P1` odbiera `REQ2` od `P2` i nie odpowiada (bo ma wyższy priorytet).

Żaden proces nie zbierze wymaganej liczby zgód.

Widać jednak, że ten algorytm można zmodyfikować dalej, by uzyskać działające
rozwiązanie problemu. Do sekcji krytycznej uzyska dostęp ten proces, który aktualnie ma
najwyższy priorytet.

Pozostały dwie kwestie do rozwiązania:

- jak dobrać priorytet, tak by jednoznacznie rozwiązywać konflikty,
- jak zmodyfikować algorytm, by działał więcej rund niż jedną.

## Algorytm Lamporta

> **Uwaga:** Nie mylić z algorytmem Chandy-Lamporta wyznaczania stanu spójnego.

Jako priorytet żądań ustalimy zegar Lamporta. Priorytet nadajemy żądaniom, nie procesom;
zapewni to sprawiedliwy dostęp do zasobów. Zegary lokalne modyfikujemy w normalny
sposób. Gdy generujemy żądanie, przypisujemy mu aktualną wartość lokalnego zegara
Lamporta.

Dostęp do sekcji ma proces o żądaniu z najwyższym priorytetem. Ponieważ dysponujemy
tylko wiedzą lokalną, musimy wiedzieć dla każdego innego procesu:

- czy inny proces się ubiega, a jeżeli tak - jaki priorytet ma jego żądanie,
- jeżeli inny proces się nie ubiega - jaki ma zegar Lamporta, by wiedzieć, czy gdyby
    zaczął się ubiegać, jego żądanie byłoby lepsze od naszego.

Każdy proces utrzymuje:

- lokalną kolejkę żądań, posortowaną po ich znacznikach czasowych (priorytetach),
- $n$-elementowy wektor zawierający zegar Lamporta ostatnio otrzymanej wiadomości
    od wszystkich innych procesów.

Używamy trzech typów wiadomości:

- `REQ` - żądania dostępu,
- `RELEASE` - zwolnienie zasobu,
- `ACK` - potwierdzenie, że wiemy o cudzym żądaniu.

### Wejście do sekcji krytycznej

Aby otrzymać dostęp do sekcji krytycznej, proces postępuje następująco:

1. Proces wysyła `REQ` do wszystkich innych procesów, łącznie z samym sobą.
     Powoduje to wstawienie własnego żądania do kolejki żądań.
2. Proces otrzymuje dostęp do sekcji krytycznej, gdy zachodzą równocześnie dwa warunki:
     - **(W1)** własne żądanie jest na szczycie kolejki żądań,
     - **(W2)** od wszystkich pozostałych procesów otrzymaliśmy wiadomości
         o starszej etykiecie czasowej.

### Obsługa wiadomości

Po stronie procesu $i$-tego:

1. Aktualizujemy zegar Lamporta w zwykły sposób oraz $j$-tą pozycję wektora znaczników
     czasowych ostatnio otrzymanych wiadomości (czyli od procesu $j$-tego).
2. Po otrzymaniu `REQ` od procesu $j$-tego proces $i$-ty wstawia żądanie do lokalnej
     kolejki (posortowanej po znacznikach czasowych), a następnie wysyła `ACK`.
3. Zgodnie z obsługą zegarów Lamporta, znacznik `ACK` musi być większy niż znacznik
     otrzymanego `REQ`.
4. Jeżeli proces $i$-ty jeszcze się nie ubiega, to gdyby zaczął ubiegać się w przyszłości
     o dostęp do sekcji krytycznej, priorytet jego hipotetycznego żądania musi być większy
     od wysłanego teraz `ACK`.
5. Po otrzymaniu `RELEASE` od procesu $j$-tego usuwamy jego żądanie z kolejki.
6. `ACK` powoduje tylko aktualizację zegara lokalnego oraz wektora znaczników czasowych
     ostatnio otrzymanych wiadomości.

Po obsłudze wiadomości sprawdzamy, czy zachodzą warunki **(W1)** oraz **(W2)**.
Jeżeli tak, wchodzimy do sekcji krytycznej. Po wyjściu z sekcji proces wysyła `RELEASE`
do wszystkich innych procesów.

### Złożoność

- Złożoność czasowa: 3 (trzy rundy), przy założeniu pomijalnych czasów przetwarzania
    lokalnego oraz jednostkowych czasów przesłania wiadomości.
- Złożoność komunikacyjna: $3n$ (dokładniej $3n - 3$, gdyby nie wysyłać wiadomości
    do samego siebie).

Z łatwością można zmodyfikować ten algorytm tak, by zezwalał na dostęp równocześnie
$X$ procesów do sekcji krytycznej. Można przy tym wykorzystać już utworzoną kolejkę
procesów (posortowanych po priorytetach) do przechowywania dodatkowych informacji lub
sprawdzania dodatkowych warunków.

## Algorytm Ricarta-Agrawali

Łatwo zauważyć, że procesy w poprzednim algorytmie musiały czekać na wiadomość typu
`ACK`. Możemy więc wrócić do naszego naiwnego algorytmu z początku dokumentu
i zaproponować następujące rozwiązanie.

Każde żądanie ma przypisany priorytet - najlepiej zegar Lamporta, ale może to być dowolny
priorytet, byle jednoznacznie pozwalał na rozstrzyganie konfliktów.

W przypadku, gdy dwa żądania mają identyczny priorytet, decyduje identyfikator
procesu-właściciela żądania.

Procesy utrzymują zegary Lamporta w zwykły sposób. W chwili utworzenia żądania,
żądanie otrzymuje bieżącą wartość zegara Lamporta jako priorytet. Oczywiście wszystkie
wiadomości muszą mieć przypisany znacznik czasowy, jak wynika z algorytmu utrzymania
zegara Lamporta.

Używamy dwóch typów wiadomości:

- `REQ` - prośba o dostęp,
- `ACK` - udzielona zgoda.

Procesy utrzymują kolejkę oczekujących na wiadomość typu `ACK` oraz licznik zgód.

### Wejście do sekcji krytycznej

Aby otrzymać dostęp do sekcji krytycznej, proces postępuje następująco:

1. Proces wysyła `REQ` do wszystkich innych procesów.
2. Ustawia licznik zgód na zero.
3. Proces otrzymuje dostęp do sekcji krytycznej, gdy otrzyma `ACK` od wszystkich
     innych procesów.

### Obsługa wiadomości

Po stronie procesu $i$-tego:

1. Aktualizujemy zegar Lamporta w zwykły sposób.
2. Po otrzymaniu `REQ` od procesu $j$-tego:
     - jeżeli proces $i$-ty nie ubiega się o sekcję, wysyła `ACK`,
     - jeżeli proces $i$-ty ubiega się o sekcję, ale jego żądanie ma niższy priorytet, wysyła
         `ACK`,
     - jeżeli proces $i$-ty ubiega się o sekcję, ale jego żądanie ma wyższy priorytet, nie wysyła
         `ACK`, lecz zapamiętuje proces $j$-ty w kolejce procesów oczekujących.
3. Po otrzymaniu `ACK` proces $i$-ty zwiększa licznik zgód.
4. Jeżeli otrzymał zgody od wszystkich innych procesów, wchodzi do sekcji krytycznej.
5. Po wyjściu z sekcji wysyła `ACK` do wszystkich procesów z kolejki oczekujących.

> **Uwaga:** jeżeli w przyszłości proces $i$-ty zmieni zdanie i zacznie się ubiegać,
> jego `REQ` będzie miało większy zegar Lamporta niż wcześniej wysłane `ACK`.

### Złożoność

- Złożoność komunikacyjna pakietowa: $2n$ (dokładniej $2n - 2$).
- Złożoność czasowa: 2.

### Dalsza optymalizacja

Jeżeli proces $i$-ty otrzyma `REQ` o priorytecie 5 od procesu $j$-tego,
a sam wcześniej wysłał do procesu $j$-tego `REQ` o priorytecie 4, to proces $i$-ty
może wywnioskować:

- proces $j$-ty ma niższy priorytet,
- na pewno musi wysłać `ACK`,
- można więc zachowywać się tak, jakby już wysłał, czyli nie czekać na `ACK`
    od procesu $j$-tego i od razu zwiększyć licznik zgód.

Z drugiej strony, jeżeli proces $i$-ty otrzyma `REQ` o priorytecie 5 od procesu $j$-tego,
a sam wcześniej wysłał do procesu $j$-tego `REQ` o priorytecie 6, to proces $i$-ty
może wywnioskować:

- proces $j$-ty ma wyższy priorytet,
- dostanie ode mnie mój `REQ` i wtedy dowie się, że mam priorytet niższy,
- będzie więc wiedział, że ja na pewno muszę mu wysłać `ACK`,
- będzie zachowywał się tak, jakbym już mu `ACK` wysłał,
- nie będzie czekał na `ACK` ode mnie,
- w takim razie nie muszę mu w ogóle wysyłać `ACK`.

Zaoszczędzamy więc jeden `ACK`. W przypadku szczególnym, gdy wszyscy się ubiegają,
odpada więc $n$ `ACK`-ów, a proces z najwyższym priorytetem wchodzi w pierwszej rundzie.
Mamy więc chwilową złożoność czasową 1, a komunikacyjną pakietową tylko $n$.

Tak samo jak poprzednio, także algorytm Ricarta-Agrawali można zmodyfikować w celu
umożliwienia $X$ procesom równoczesnego dostępu do sekcji krytycznej. Odpowiednią
zmianę zostawiamy czytelnikom do przemyślenia.