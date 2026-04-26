## Wymagania ogólne wobec wszystkich projektów

### Zaliczenie, zasady ogólne

Zakładając, że student nie zostanie wykluczony z zajęć z powodów regulaminowych (np. brak odpowiedniej ilości obecności), zaliczenie odbywa się na podstawie obrony projektów zaliczeniowych. Studenci i studentki dobierają się w dwuosobowe grupy, z których każda następnie otrzymuje problem do rozwiązania. Problemy związane są zwykle z dostępem do rozproszonej sekcji krytycznej. Następnie każda grupa przygotowuje i prezentuje do akceptacji algorytm. Algorytm jest poprawiany tak długo, aż zyska akceptację prowadzącego i dopiero wtedy grupa może przystąpić do implementacji algorytmu.

Oceny wystawiane są indywidualnie, uwzględniając wkład członków grupy w rozwiązanie, jakość algorytmu, implementacji oraz sprawozdania. Zwykle członkowie grupy otrzymują taką samą ocenę, jednakże teoretycznie jest możliwe, że jeden student/studentka z pary otrzyma ocenę niedostateczną, a drugi/druga bardzo dobrą - zdarzyć się tak może w przypadku drastycznej asymetrii we wkładzie w ostateczne rozwiązanie.

Proszę zwrócić uwagę, że brak zaliczenia skutkuje koniecznością powtarzania przedmiotu. W gestii prowadzącego leży wydanie zgody na nieuczęszczanie na zajęcia i zaliczenie powtarzanego przedmiotu w trybie zaocznym, być może wcześniej. 

### Terminy

Dokładne terminy podawane są przez prowadzącego. Zwykle tematy problemów są wybierane w połowie semestru dla studiów stacjonarnych. Na studiach niestacjonarnych prowadzący może tematy przedstawić i przypisać studentom wcześniej. Na kolejnych zajęciach studenci proponują algorytmy. Na ostatnich zajęciach należy pokazać gotowe rozwiązania, uruchamiając je na co najmniej dwóch maszynach. Oddanie projektu w terminie późniejszym skutkować będzie obniżaniem oceny, zgodnie z wymogami przedstawionymi przez prowadzących. Oddanie po terminie ostatecznym jest niemożliwe - należy zwrócić się o nowy temat.

**UWAGA!** W systemie USOS jest bardzo trudno wystawić ocenę po rozpoczęciu sesji, w związku z tym należy przyjąć, że projekty **muszą** być oddane najpóźniej
w ostatni dzień przed sesją.

**UWAGA!** To, że student chce oddać w ostatni dzięń przed sesją, nie oznacza, że tak się stanie. Prowadzący mogą zachorować, mogą zajść nieprzewidziane zdarzenia losowe, może zajść konflikt z innymi zajęciami itd. BARDZO PROSIMY więc oddawać na czas; **Nie ma gwarancji** że można oddać w terminie innym niż na ostatnich zajęciach.

Można oczywiście oddać projekt przed czasem. Jest to mile widziane i może skutkować podwyższeniem oceny.

[Wersje uproszczone](#wersja-uproszczona) **musi** być oddana najpóźniej na ostatnich zajęciach. Nie wyrobienie się w terminie w tym wypadku oznacza konieczność stworzenia wersji pełnej.

### Przedstawienie algorytmu

Prowadzącemu należy wytłumaczyć, jak działa algorytm. Prowadzący może odrzucić algorytm, wyjaśniając powody. Kolejną wersję można przedstawić na tych samych
zajęciach lub następnych. W przypadkach wyjątkowych - można spróbować zrobić to pocztą elektroniczną, jednakże zniechęcamy do tego i uprzedzamy, że przy takiej
formie oczekiwanie na odpowiedź może być bardzo długie.

Opis podany powinien być kompletny, szczegółowy i nie powinien wymagać dodatkowych wyjaśnień podczas obrony projektu. Należy przyjąć, że dowolna osoba powinna być w stanie zaimplementować algorytm na podstawie jego opisu. Chociaż zmniejsza to czytelność, dla wygody odnoszenia się do algorytmu podczas dyskusji proszę o numerowanie linii. 

Rekomendowane jest **wydrukowanie** algorytmu oraz użycie następującego szablonu:


**OPIS OGÓLNY**
```
Schemat ogólny działania; np. aby dostać się do sekcji krytycznej, proces wysyła REQUEST do wszystkich procesów. Każdy proces
odpowiada ACK, jeżeli ma mniejszy priorytet. Po zebraniu ACK od wszystkich procesów otrzymuje się dostęp do sekcji. Priorytetem
są zegary Lamporta
```

**STANY PROCESÓW**
```
Opis stanów algorytmu, np:
BEGIN: początkowy stan procesu
```

**TYPY WIADOMOŚCI**
```
Opis wiadomości np:
REQUEST: żadanie dostępu do sekcji krytycznej; zawiera etykietę czasową .ts 
```

Poniżej należy dodać **opis szczegółowy**, gdzie dla każdego stanu w tym szablonie należy przedstawić jak procesy reagują na każdą wiadomość, którą mogą otrzymać w tym stanie oraz
w jaki sposób przechodzą do innych stanów.

Niemile widziane jest rozpisywanie kodu w pythonie, albo omawianie co robi który wątek. Powinno to być raczej coś takiego:

**STAN ŻĄDANIE DOSTĘPU**

```
case REQUEST: jeżeli REQ.priotytet > my_priorytet, wyślij ACK. WPW dodaj REQ do kolejki.
case ACK:     ack++. Jeżeli otrzymałeś ACK od wszystkich procesów (ack == n-1), przejdź do stanu **W SEKCJI KRYTYCZNEJ**
default:      zignoruj wiadomość
```

Katalog zawiera [przykładowy opis algorytmu](przyklad-opisu-algorytmu.odt)[^1]

### Obrona projektu

Obrona projektu polega na omówieniu algorytmu i jego implementacji, uruchomieniu programu ze zmienną liczbą parametrów. Mile widziane jest pokazanie wydrukowanego opisu algorytmu (ułatwia to sprawdzanie).
Prowadzący może zadać pytania każdej osobie z grupy dotyczące algorytmu lub kodu. Prowadzący może także polecić wprowadzenie modyfikacji kodu.
Projekt musi być uruchomiony na conajmniej dwóch maszynach.

### Wymagania dotyczące środowiska

Środowisko jest w pełni asynchroniczne, kanały są niezawodne i FIFO, procesy nie ulegają awarii. W związku z tym nie można zakładać, że jeżeli proces odczeka kilka sekund, to coś się zmieni w stanach innych procesów: zegary procesów nie są zsynchronizowane i funkcjonują z różną prędkością. Procesy działają w wiecznej pętli.

### Wymagania dotyczące rozwiązań.

Algorytmy powinny być maksymalnie rozproszone. Nie powinny dopuszczać elementów centralnych, procesy powinny mieć równorzędne role. Niedopuszczalne jest rozwiązanie, w którym procesy zakładają globalne zamki na jakieś struktury, aktualizują je i potem przesyłają innym modyfikacje. Na przykład, jeżeli w projekcie jest mowa o zasobie X nierozróżnialnych elementów, to dostęp do tego zasobu ma być strzeżony przez rozproszoną sekcję krytyczną o pojemności X i nie należy dokonywać transformacji do zasobu o nazwie “zmienna reprezentująca liczność” o liczności 1 (do którego dostęp następuje poprzez zmienną krytyczną o pojemności 1).
Nie należy także dodawać dodatkowych sekcji krytycznych strzegących dostępu do sekcji krytycznej.

> # Przykład **niedopuszczalnego** rozwiązania: 
> w projekcie jest mowa o zasobie “bilety” o liczności B. Zasób jest nierozróżnialny. Nie wolno rozwiązać to w ten sposób, że np. przy pomocy algorytmu Ricarta-Agarwali proces zdobywa dostęp do sekcji krytycznej, po zdobyciu dostępu zmniejsza zmienną “bilety” i przy zwalnianiu sekcji wysyła pozostałym informację, że zmniejszył liczność o 1. 
> Należy to rozwiązać tak, by jednocześnie teoretycznie B procesów mogło zdobyć bilet. 
> Nie wolno także rozwiązać tego tak, że tworzy się zamiast tego B zasobów “bilet” każdy o liczności 1.

NIE WOLNO tworzyć zarządców zasobów. Procesy między sobą uzgadniają dostępy do sekcji krytycznych/zasobów, zasoby są pasywne. Jeżeli w zadaniu jest napisane,
że jest dostępnych 5 biletów to oznacza zapewne, że gdzieś musi być zaimplementowana sekcja krytyczna o pojemności 5 (czyli pięć naraz procesów będzie mogło
wejść do tej sekcji).

Do komunikacji używamy tylko i wyłącznie MPI\_Send/MPI\_Recv (ewentualnie ich asynchronicznych odpowiedników). Funkcji do komunikacji grupowych i synchronizacyjnych wolno używać tylko i wyłącznie do inicjalizacji struktur na samym początku programu.

Należy zapewnić równoległość na tyle, na ile się da. W przypadku doboru procesów w grupy, lub dostępu do kilku zasobów rozróżnialnych należy spróbować zapewnić, by dobór/dostęp ten nie wymagał w najgorszym razie (dla procesu o najgorszym priorytecie) na przegraniu n-1 sekcji krytycznych. Losowanie zasobów jest dopuszczalne, ale nieoptymalne i należy się zastanowić nad rozwiązaniem lepszym. 

Procesy powinny wypisywać informacje o swoim stanie: o próbie dostępu do sekcji krytycznej (zasobu), o otrzymaniu dostępu, o zwolnieniu dostępu. Informacje powinny być poprzedzone identyfikatorem procesu i zegarem Lamporta. Przykład:

```
[1] [t100100] Śpię
[1] [t100101] Rozpoczynam staranie o sekcję krytyczną
[1] [t100102] Jestem w sekcji krytycznej
[1] [t100103] Wychodzę z sekcji krytycznej
```

Gdzie [1] to identyfikator procesu, [t101] to zegar Lamporta. Dobrze jest także włączyć możliwość bardziej szczegółowych komunikatów (np. reakcje na wiadomości, wysyłanie wiadomości) z możliwością ich wyłączenia (np. podczas rekompilacji, ustawiając odpowiednio jakiś #define). Przydatne może okazać się makro println z projektu, gdzie implementowaliśmy zegary Lamporta.

W wersji ostatecznej należy usunąć komunikaty typu “wysłałem wiadomość”, “czekam… czekam… czekam…” albo “odebrałem wiadomość”, ponieważ doświadczenie wskazuje, że jest ich zbyt wiele i utrudniają weryfikację poprawności implementacji.

Należy zapewnić, by procesy nie były głodzone. Próba zapewnienia sprawiedliwego zbilansowania dostępów (fairness) jest mile widziana, ale nieobowiązkowa. Natomiast, jeżeli algorytm tylko zapewnia dostęp z prawdopodobieństwem równym 1, a nie “ostatecznie na pewno zapewni dostęp”, należy algorytm przemyśleć.

Programy mogą być pisane w dowolnym języku programowania z użyciem MPI, jednakże tylko dla kombinacji C+MPI gwarantowane jest wsparcie ze strony prowadzących. W pozostałych przypadkach należy samodzielnie zapewnić możliwość uruchomienia programu (co zwykle wymagać będzie kontaktu z administratorem laboratorium i doinstalowaniem odpowiednich pakietów). Nie wspieramy oficjalnie kombinacji python+MPI, ale z doświadczenia  również działa po pewnych zmianach.

W tym roku eksperymentalnie zezwalamy na samodzielne wybranie innego środowiska wspierającego wymianę wiadomości. Wymagania: musi posiadać asynchroniczną funkcję wysyłania wiadomości (czyli nie powinno wymagać randez-vous przy wysyłaniu/odbiorze), gwarantować FIFO w kanałach oraz umożliwiać uruchomienie na kilku różnych komputerach. 

Kod powinien być sparametryzowany, umożliwiając szybką zmianę liczby dostępnych zasobów (miejsc w sekcji krytycznej) i tak dalej, zgodnie z konkretnymi wymogami wybranego przez grupę tematu. Kod powinien być czytelny, z krótkimi funkcjami o nazwach ilustrujących ich przeznaczenie.

Algorytmy są optymalizowane pod względem minimalizowania formalnej złożoności czasowej i komunikacyjnej. Domyślnie należy przewagę dać minimalizacji formalnej złożoności czasowej (nie rzeczywistym czasom!), jednakże prowadzący może zlecić preferencję dla złożoności komunikacyjnej. W przypadkach szczególnych, jeżeli studenci potrafią to właściwie uzasadnić, możliwe jest dopuszczenie algorytmów o większej złożoności formalnej, jeżeli posiadają one inne zalety. W związku z tym, zwykle bardzo niechętnie spoglądamy na algorytmy używające tokeny.

[^1]: Tak, jest to plik .odt, a nie .tex albo .rst. Mile widziana kontrybucja kogoś, komu będzie się chciało zrobić wersję w innych formatach.
