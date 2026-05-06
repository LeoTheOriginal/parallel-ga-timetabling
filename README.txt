Rownolegly Algorytm Genetyczny -- Plan Zajec Uniwersyteckich
=============================================================
Projekt nr 26, grupa 1
Przedmiot: Systemy Rownolegle i Rozproszone
Autorzy: Dawid Piotrowski, Julia Przezdzik

Opis
----
Program rozwiazuje problem ukladania planu zajec uniwersyteckich
(University Course Timetabling Problem, NP-trudny) przy pomocy
rownoleglego algorytmu genetycznego w architekturze Island Model
zaimplementowanego w C99 z biblioteka MPI.

Cel: znalezc przypisanie kazdego zdarzenia (wyklad/cwiczenia/lab)
do pary (slot czasowy, sala) tak, aby wyzerowac twarde naruszenia
i zminimalizowac kary miekkie.

Zawartosc archiwum
------------------
  src/        -- kod zrodlowy w C (10 plikow .c/.h)
  data/       -- przykladowe dane wejsciowe (100 zdarzen, podzbior UniTime AGH)
                 - rooms.csv      sale (27)
                 - teachers.csv   prowadzacy (70)
                 - groups.csv     grupy studenckie (70)
                 - courses.csv    kursy/zajecia (100)
                 - original_schedule.csv  plan referencyjny
  results/    -- przykladowe wyniki dzialania programu na danych z data/
                 - schedule_example_n1.csv   wynik z 1 procesu MPI
                 - schedule_example_n16.csv  wynik z 16 procesow MPI
                 - benchmark_summary_n100.csv  podsumowanie benchmarku (3 runy/konfig)
  raport.pdf  -- opis budowy, dzialania i obslugi programu (zawiera schemat blokowy)
  Makefile    -- regulami budowania
  README.txt  -- ten plik

Wymagania
---------
- Kompilator C99 (mpicc)
- MPICH (testowane: 3.2+ na taurus.fis.agh.edu.pl, 5.0.0 na klastrze stud204)

Kompilacja
----------
Na serwerze taurus / klastrze AGH:
  source /opt/nfs/config/source_mpich500.sh
  source /opt/nfs/config/source_cuda121.sh
  export MPIR_CVAR_ENABLE_GPU=0
  make

Lokalnie (jezeli MPICH zainstalowany):
  make

Uruchomienie z danymi przykladowymi
-----------------------------------
  make run

Ta regula uruchamia program na danych z data/ (100 zdarzen)
w 1 procesie MPI. Wynik trafia do schedule.csv i timetable.txt.

Uruchomienie rownolegle (4 procesy)
-----------------------------------
  make run-parallel

Uruchomienie na klastrze AGH (N procesow MPI)
---------------------------------------------
  /opt/nfs/config/station204_name_list.sh 1 16 > nodes
  mpiexec -f nodes -n 16 ./timetable_ga data/

Przywrocenie zawartosci podkatalogu do stanu wyjsciowego
--------------------------------------------------------
  make clean

Usuwa: obj/, plik wykonywalny, schedule.csv, timetable.txt,
       convergence_rank*.csv, plik nodes.

Wyniki dzialania na danych przykladowych
----------------------------------------
Po `make run` program tworzy:
  schedule.csv          plan zajec w formacie CSV (event_id, slot, room)
  timetable.txt         czytelny wydruk planu pogrupowanego po dniach
  convergence_rankN.csv trajektoria fitness w trakcie ewolucji (per rank)

Pliki referencyjne (wyniki uzyskane na klastrze AGH) znajduja sie w results/.

Wyniki benchmarku (3 runy x {1, 4, 8, 16} procesow MPI, 100 zdarzen):
  T1  = 2.31 s          (sekwencyjnie)
  T4  = 0.64 s          (S = 3.6x)
  T8  = 0.43 s          (S = 5.4x)
  T16 = 0.37 s          (S = 6.3x)
  0 naruszen twardych we wszystkich konfiguracjach.

Pelny opis algorytmu, architektury MPI i analizy wydajnosci
znajduje sie w raporcie raport.pdf.
