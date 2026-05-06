Rownolegly Algorytm Genetyczny -- Plan Zajec Uniwersyteckich
=============================================================
Projekt nr 26, grupa 1
Przedmiot: Systemy Rownolegle i Rozproszone
Autorzy: Dawid Piotrowski, Julia Przezdzik

Opis
----
Rownolegly algorytm genetyczny (Island Model) rozwiazujacy problem
ukladania planu zajec uniwersyteckich (University Course Timetabling
Problem, NP-trudny). Implementacja w C99 + MPI.

Cel: znalezc przypisanie kazdego zdarzenia (wyklad/cwiczenia/lab) do
pary (slot czasowy, sala) tak, aby wyzerowac twarde naruszenia i
zminimalizowac kary miekkie.

Zawartosc archiwum
------------------
  src/        kod zrodlowy (10 plikow .c/.h)
  data/       dane wejsciowe (100 zdarzen)
  results/    przykladowe wyniki dla n=1 i n=16 procesow MPI
  raport.pdf  pelny opis z analiza wydajnosci
  Makefile    regulami budowania
  README.txt  ten plik

Wymagania
---------
- mpicc (MPICH 3.2 lub nowszy, lub kompatybilny)
- C99
- gnuplot (opcjonalnie, dla wykresow z `make plot`)

Kompilacja
----------
  make

Jezeli klaster wymaga zaladowania srodowiska MPI (custom mpicc/mpiexec
w PATH) -- nalezy je zaladowac przed wywolaniem `make`.

Uruchomienie
------------
  make run            -- 1 proces MPI, dane z data/simple_n100/
  make run-parallel   -- 4 procesy MPI na jednym hoscie

Wieloprocesowe uruchomienie na wielu wezlach (hostfile `nodes`
z lista nazw wezlow, jeden na linie):
  mpiexec -f nodes -n 16 ./timetable_ga data/simple_n100/

Czyszczenie
-----------
  make clean

Wyniki dzialania
----------------
Po `make run` powstaja:
  schedule.csv             plan w formacie CSV (event_id, slot, room)
  timetable.txt            czytelny wydruk planu zajec po dniach
  convergence_rankN.csv    trajektoria fitness w trakcie ewolucji (per rank)

Wyniki referencyjne (16 procesow MPI, 100 zdarzen, 3 uruchomienia/konfig):
  T1  = 2.31 s
  T4  = 0.64 s   (S = 3.6x)
  T8  = 0.43 s   (S = 5.4x)
  T16 = 0.37 s   (S = 6.3x)
  0 naruszen twardych we wszystkich konfiguracjach.

Pelny opis algorytmu, architektury MPI i analizy wydajnosci -- raport.pdf.
