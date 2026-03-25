# Parallel GA for University Course Timetabling

Parallel Genetic Algorithm solving the University Course Timetabling Problem (UCTP),
implemented in **C with MPI** using the **Island Model** with ring migration.

**AGH University of Krakow** | Systems Parallel and Distributed | Project #26

## Problem

Assign university events (lectures, labs, seminars) to `(timeslot, room)` pairs
while satisfying hard constraints (no collisions) and minimizing soft constraint penalties.

| Constraint type | Examples |
|---|---|
| **Hard** (must satisfy) | Room collisions, teacher collisions, group collisions, room-type mismatch |
| **Soft** (minimize) | Gaps between classes, late classes, teacher/student day compactness, building continuity, room capacity |

## Architecture

```
Rank 0                      Rank 1..N-1
  load CSV -> ProblemData     (wait)
  MPI_Bcast ──────────────>  receive data
  run_ga (island model)       run_ga (island model)
    ├── init population         ├── init population
    ├── generational loop       ├── generational loop
    │   ├── evaluate            │   ├── evaluate
    │   ├── select + crossover  │   ├── select + crossover
    │   ├── adaptive mutate     │   ├── adaptive mutate
    │   ├── repair              │   ├── repair
    │   ├── MPI_Allreduce <──>  │   ├── MPI_Allreduce (stop sync)
    │   └── MPI_Sendrecv  <──>  │   └── MPI_Sendrecv  (migrate)
    └── local search            └── local search
  MPI_Allreduce(MINLOC) ──>  global best
  write schedule.csv + timetable.txt
```

## GA Parameters

| Parameter | Value |
|---|---|
| Population | 200 / N islands |
| Selection | Tournament K=3 (two-level comparison) |
| Crossover | Uniform 50/50, rate 0.8 |
| Mutation | Adaptive: 5% (hard>0), 1% (hard=0) |
| Repair | Greedy: 20 random probes + systematic scan |
| Migration | Ring topology, every 50 generations |
| Termination | MPI_Allreduce OR: stagnation >= 200 or fitness == 0 |
| Local search | Hill climbing 5000 iter post-GA |

## Results

Tested on AGH cluster (16 nodes, stud204-01..16):

| Events | 1 proc | 16 procs | Speedup | Hard violations |
|--------|--------|----------|---------|-----------------|
| 100 | 3.2s | 0.27s | 12x | 0 |
| 200 | 15.0s | 0.61s | 25x | 0 |
| 400 | 53.4s | 1.86s | 29x | 0 |
| 1058 | 190s | 9.7s | 20x | 0 |

## Build & Run

### Prerequisites

- C compiler with C99 support
- MPI (MPICH 3.2+ or 5.0)
- gnuplot (optional, for charts)

### Compile

```bash
source /opt/nfs/config/source_mpich32.sh   # or source_mpich500.sh on cluster
export MPIR_CVAR_ENABLE_GPU=0
make
```

### Run

```bash
# Single process
mpiexec -n 1 ./timetable_ga data/simple/

# 4 MPI processes (island model)
mpiexec -n 4 ./timetable_ga data/simple/

# Full cluster (16 nodes)
/opt/nfs/config/station204_name_list.sh 1 16 > nodes
mpiexec -f nodes -n 16 ./timetable_ga data/simple/
```

### Benchmark

```bash
make benchmark    # 5 runs x {1,2,4,8,16} processes
make plot         # generate speedup/convergence charts
```

### Webapp (visualization)

```bash
bash demo.sh      # convert data + start webapp at http://localhost:8080
```

## Project Structure

```
src/
  main.c          Entry point, MPI setup, data broadcast
  ga.c/h          GA engine: init, select, crossover, mutate, migrate
  fitness.c/h     Two-level evaluation, repair operator, local search
  io.c/h          CSV parsing, timetable output
  types.h         Data structures (Room, Teacher, Event, Individual, etc.)
  pcg_basic.c/h   PCG32 PRNG (Apache 2.0, Melissa O'Neill)
scripts/
  convert_simple.py    UniTime -> GA input CSV converter
  create_db.py         SQLite database builder
  api_prototype.py     REST API + webapp server
plots/                 gnuplot scripts + generated charts
data/                  Input CSV files (not included, see data/README.md)
Makefile               Build system
benchmark.sh           Automated benchmarking
demo.sh                One-click demo
webapp.html            Dark-theme SPA for schedule visualization
```

## Input Data

Input data is **not included** in this repository (contains real university personnel data).
See [`data/README.md`](data/README.md) for the expected CSV format and generation instructions.

## References

- K. Banczyk, H. Boinski, A. Krawczyk, *Parallelisation of GA for University Course Timetable Optimisation*, IEEE PARELEC'06, 2006.
- H. Faris, A. Sheta, E. Tobal, *A Parallel GA for Solving Time Tabling Problem*, ICGST-AIML, 2008.
- PCG Random Number Generation: [pcg-random.org](http://www.pcg-random.org)

## License

Educational project. PCG32 PRNG is licensed under Apache 2.0.
