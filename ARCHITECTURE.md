# Architektura: Równoległy Algorytm Genetyczny dla Planowania Zajęć

> **Projekt**: Systemy Równoległe i Rozproszone, AGH WFiIS
> **Problem #26**: Planowanie zajęć uniwersyteckich
> **Stan**: Prosty model 8-blokowy, 633 eventów, 0 hard violations, 2.7s (16 proc)

---

## 1. Problem

WFiIS AGH ma **633 zajęć tygodniowo** (bloki 1.5h) w **41 salach** prowadzonych
przez **161 wykładowców** dla **191 grup**. GA rozmieszcza je w siatce
**7 bloków × 5 dni × 41 sal** bez kolizji.

**Model**: 1 event = 1 blok 1.5h (8:00-9:30, 9:45-11:15, ..., 18:30-20:00).
Przerwy 15-minutowe między blokami są wbudowane w siatkę.

## 2. Dane

Źródło: UniTime AGH, eksport `events_raw.csv` (4587 wierszy, semestr letni 2026).

**Filtrowanie** (`scripts/convert_simple.py --no-grupa`):
- Tylko budynki D-10, D-7, D-11
- Tylko eventy z przypisanym prowadzącym
- Tylko eventy **dokładnie pasujące do bloków 1.5h AGH**
- Bez: INNE, NST, SZD, egzaminów, konsultacji, kolokwiów
- Deduplikacja: klucz (Nazwa, Typ, Tytuł, Dzień, Prowadzący) — bez Grupy

**Wynik**: 633 unique weekly events, 44% fill rate (633 / 1435 room-blocks).

## 3. Architektura MPI

```
Rank 0:                     Rank 1..N-1:
  load CSV → ProblemData      (wait)
  MPI_Bcast ─────────────→  receive data
  run_ga (island model)       run_ga (island model)
    ├── init population         ├── init population
    ├── generational loop       ├── generational loop
    │   ├── evaluate (hard+soft)│   ├── evaluate
    │   ├── select + crossover  │   ├── select + crossover
    │   ├── adaptive mutate     │   ├── adaptive mutate
    │   ├── repair              │   ├── repair
    │   ├── MPI_Allreduce ←───→ │   ├── MPI_Allreduce (terminate)
    │   └── MPI_Sendrecv  ←───→ │   └── MPI_Sendrecv  (migrate)
    └── local search            └── local search
  MPI_Allreduce(MINLOC) ──→  global best
  write schedule.csv + timetable.txt
```

## 4. Algorytm GA

| Komponent | Opis |
|-----------|------|
| Populacja | 200 / N wysp |
| Selekcja | Turniej K=3, dwupoziomowe porównanie |
| Crossover | Uniform 50/50, 80% prawdopodobieństwo |
| Mutacja | Adaptive: 5% (hard>0), 1% (hard=0) |
| Repair | Greedy: 20 random probes + systematic scan, soft-aware tiebreaker |
| Migracja | Ring topology, co 50 generacji, MPI_Sendrecv |
| Terminacja | MPI_MAX (OR): stagnation≥200 lub fitness==0 |
| Local search | Hill climbing 5000 iter po GA, 15 probes/iter |

### Hard constraints
- Kolizja sali (2+ events w jednym room-block)
- Kolizja prowadzącego (ten sam teacher w 2 miejscach)
- Kolizja grupy (ta sama grupa w 2 miejscach)
- Day overflow (event poza zakresem dnia)

### Soft constraints
- Okienka (gaps×2 penalty)
- Compact days (preferuj mniej dni z zajęciami)
- Późne zajęcia (bloki 6-7, po 18:30)
- Teacher compactness (mniej dni prowadzenia)
- Room capacity (students > room capacity)
- Building continuity (minimalizuj zmiany budynku w ciągu dnia)

## 5. Wyniki

| MPI | Czas | Hard | Soft |
|-----|------|------|------|
| 1 | ~20s | 0 | ~5000 |
| 16 | **2.7s** | **0** | ~5000 |

Benchmarki z różnymi rozmiarami: results_v3/*.csv

## 6. Webapp

`webapp.html` — dark-theme SPA serwowana przez `scripts/api_prototype.py`.

Tabs: **Sale** | **Kierunki** | **Studenci** | **Prowadzący** | **Statystyki**

Start: `bash demo.sh` → http://localhost:8080/

## 7. Struktura plików

```
project/
├── src/                  (10 plików C — GA engine)
├── scripts/
│   ├── api_prototype.py  (API + webapp server)
│   ├── convert_simple.py (UniTime → GA input, strict 1.5h blocks)
│   └── create_db.py      (SQLite builder)
├── data/
│   ├── simple/           (aktywny dataset: 633 events)
│   └── unitime/          (surowe dane AGH)
├── results_v3/           (benchmarki: CSV)
├── plots/                (wykresy: speedup, convergence, quality)
├── webapp.html           (dark SPA)
├── timetable.db          (SQLite)
├── demo.sh               (one-click demo)
└── Makefile
```
