Rownolegle Algorytm Genetyczny -- Plan Zajec Uniwersyteckich
=============================================================
Projekt nr 26
Przedmiot: Systemy Rownolegle i Rozproszone
Autor: Piotrowski
Prowadzacy: dr Gronek, AGH Krakow

Opis
----
Program rozwiazuje problem ukladania planu zajec uniwersyteckich
przy pomocy rownoleglego algorytmu genetycznego (Island Model)
zaimplementowanego w C z biblioteka MPI.

Wymagania
---------
- Kompilator C (gcc/mpicc) z obsluga C99
- MPICH (3.2+ na taurus, 5.0.0 na klastrze)
- gnuplot (do generowania wykresow)
- pdflatex (do kompilacji raportu)

Kompilacja
----------
Na serwerze taurus:
  source /opt/nfs/config/source_mpich32.sh
  export MPIR_CVAR_ENABLE_GPU=0
  cd project/
  make

Uruchomienie
------------
Pojedynczy proces:
  cd project/
  make run

Na klastrze (4 procesy MPI):
  cd project/
  make run-parallel

Na klastrze (N procesow z plikiem nodes):
  /opt/nfs/config/station204_name_list.sh 1 16 > nodes
  mpiexec -f nodes -n 16 ./timetable_ga data/

Benchmark
---------
  cd project/
  make benchmark    # 5 uruchomien x 5 konfiguracji (n=1,2,4,8,16)
  make plot         # generowanie wykresow z wynikow

Raport PDF
----------
  cd project/
  make report       # kompiluje report/raport.pdf

Struktura katalogow
-------------------
  project/src/      -- kod zrodlowy (C)
  project/data/     -- dane wejsciowe (CSV)
  project/plots/    -- skrypty gnuplot
  project/benchmark.sh -- skrypt benchmarkowy
  project/Makefile  -- system budowania
  report/           -- raport LaTeX
  report/raport.pdf -- skompilowany raport
