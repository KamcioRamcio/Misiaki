# Opis projektu — Okręty / Misie

## Treść zadania

W odległej przyszłości federacja terrańska prowadzi wojnę podjazdową z misiami.
`N` okrętów co pewien czas wraca z walki do bazy i ubiega się o:

- jeden z **K rozróżnialnych doków** (przydział musi być deterministyczny i
  zbalansowany; dopuszczalne jest, że okręt czeka na dok `i` mimo że doki
  `1..i-1` są puste),
- pewną liczbę `m_S` z **M nierozróżnialnych mechaników**, gdzie `m_S` zależy
  od stopnia zniszczeń i jest losowane przy każdym powrocie z walki z rozkładu
  jednostajnego `Uniform[1, M]`.

Algorytm dostępu do doku ma odpowiadać równocześnie na pytanie *czy* okręt
ma dostęp i *do którego doku*, na podstawie deterministycznej reguły.

## Mapowanie zasobów na sekcje krytyczne

| Zasób                         | Sekcja krytyczna                                    |
|-------------------------------|-----------------------------------------------------|
| K rozróżnialnych doków        | jedna globalna kolejka żądań + reguła `dock_assign` |
| M nierozróżnialnych mechaników| jedna uogólniona sekcja krytyczna pojemności M      |

Dok jest wyznaczany deterministyczną regułą `dock = ((ts + pid) mod K) + 1`
w chwili tworzenia żądania i pozostaje przy nim niezmienny aż do `RELEASE`.
Każde żądanie kontensuje *tylko o jeden konkretny dok*, więc okręt o najgorszym
priorytecie nigdy nie musi „wygrywać" `n−1` sekcji — wystarczy mu jedna kolejka
priorytetowa do swojego doku, sklejona z resztą w jednej globalnej strukturze
porządkującej (W1, W2, W3 są warunkami lokalnymi). Patrz `docs/opis_algorytmu.md`.

Mechanicy są realizowani jako uogólniona sekcja krytyczna pojemności M
(*nie* przez licznik strzeżony mutexem ani przez M osobnych sekcji
pojemności 1) — zgodnie z `wymagania.md`, linia 81 i 87.

## Założenia środowiskowe

- Środowisko w pełni asynchroniczne.
- Kanały niezawodne i FIFO.
- Procesy nie ulegają awarii.
- Procesy działają w wiecznej pętli `REST → TRYING → INSECTION → REST`.
- Komunikacja: tylko `MPI_Send` / `MPI_Recv` (oraz ich asynchroniczne odpowiedniki).
  Funkcji grupowych używamy tylko do inicjalizacji.

## Parametryzacja

Program przyjmuje parametry z argumentów wywołania `mpirun`:

```
mpirun -np <N> ./okrety <K> <M> [seed]
```

- `N` — liczba okrętów (procesów) ustalana przez `-np` w `mpirun`,
- `K` — liczba doków,
- `M` — liczba mechaników,
- `seed` — opcjonalne ziarno generatora liczb pseudolosowych (domyślnie `time(NULL) ^ pid`).

`m_S` jest losowane lokalnie przy każdym wejściu do `TRYING` z rozkładu
`Uniform[1, M]`.

## Kompilacja i uruchomienie

```
make            # tryb release (logi tylko zmian stanu)
make debug      # tryb DEBUG: dodatkowe logi per-wiadomość (dla diagnostyki)
make clean
```

Uruchomienie lokalne:

```
mpirun -np 6 ./okrety 3 5
```

Uruchomienie na co najmniej dwóch maszynach (wymóg `wymagania.md`, linia 73):

```
mpirun --hostfile hosts.example -np 8 ./okrety 3 5 42
```

Plik `hosts.example` zawiera szablon listy hostów. Do ostatecznego uruchomienia
należy go skopiować do `hosts` i dostosować adresy/sloty.

## Format logów

Każda zmiana stanu jest logowana w formacie wymaganym przez `wymagania.md`
(linie 99–104):

```
[pid] [tNNN] Wracam z walki, m_S=X
[pid] [tNNN] Rozpoczynam staranie o sekcję (dok=?, m=X)
[pid] [tNNN] Wszedłem do doku d, mam X mechaników
[pid] [tNNN] Zwalniam dok d i X mechaników
```

`pid` to identyfikator procesu MPI; `tNNN` to lokalna wartość zegara Lamporta
w chwili logowania. W trybie release logi typu „wysłałem", „odebrałem",
„czekam" są wyłączone — uaktywniają się dopiero przy kompilacji `make debug`
(`-DDEBUG`).

## Struktura repozytorium

```
project/                 # specyfikacja prowadzącego (NIE modyfikujemy)
docs/
  opis_projektu.md       # ten plik
  opis_algorytmu.md      # opis algorytmu w formacie z wymagania.md
src/
  main.c                 # pętla stanów, parsowanie argumentów
  clock.c/h              # zegar Lamporta i wektor last_seen[N]
  queue.c/h              # sortowana kolejka żądań
  dock.c/h               # dock_assign + W2
  mechanics.c/h          # warunek W3
  comm.c/h               # MPI Send/Recv, pętla odbioru
  log.c/h                # makro println, tryb DEBUG
Makefile
hosts.example            # szablon hostfile
scripts/run.sh           # wrapper uruchomieniowy
```

## Algorytm — w skrócie

Algorytm to **rozszerzenie algorytmu Lamporta** z `rozproszona_sekcja_krytyczna.md`
o równoczesny przydział doku rozróżnialnego (przez deterministyczną regułę
`dock_assign`) oraz mechaników nierozróżnialnych (przez warunek heterogenicznej
sumy `Σ m_j ≤ M` po poprzednikach w kolejce). Pojedyncza, wspólna kolejka
i atomowy dual-grant eliminują z konstrukcji zakleszczenie pomiędzy dokiem
a mechanikami. Pełen opis: `docs/opis_algorytmu.md`.

## Obrona projektu

Podczas obrony należy mieć przy sobie wydruk `docs/opis_algorytmu.md`
(z numerowanymi liniami) oraz uruchomić program na ≥2 maszynach z różnymi
wartościami `K`, `M`, `N`. Prowadzący może poprosić o modyfikację kodu
podczas obrony — kluczowe punkty rozszerzeń są oznaczone w plikach źródłowych.
