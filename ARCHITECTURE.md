# Architecture: Parallel Genetic Algorithm for University Course Timetabling

> **Course:** Systemy Równoległe i Rozproszone — Project #26
> **Result:** ≈100 events solved with 0 hard violations in ≈0.3 s on 16 MPI ranks (≈12× speedup over the sequential baseline).

---

## 1. Problem

A weekly schedule is a function $\sigma : \mathcal{E} \to \mathcal{T} \times \mathcal{R}$ assigning every academic event $e \in \mathcal{E}$ to a `(timeslot, room)` pair drawn from the grid

$$
|\mathcal{T}| \times |\mathcal{R}| \;=\; (\text{days} \times \text{blocks/day}) \times \text{rooms}
$$

Each event represents a 1.5 h block (08:00–09:30, 09:45–11:15, …, 18:30–20:00).

## 2. Data pipeline

A real-world UniTime export is filtered down to a clean CSV bundle by `scripts/convert_simple.py`:

- only events with an assigned teacher,
- only events that fit cleanly onto the 1.5 h grid,
- exclude exam slots, consultations, and ad-hoc colloquia,
- deduplicate on `(name, type, title, day, teacher)`.

**Note:** the input data is private (real personnel and group identifiers) and is not committed; see [`data/README.md`](data/README.md).

## 3. MPI architecture

The runtime view — what every rank does, and the four MPI primitives that
glue them together:

```mermaid
sequenceDiagram
    autonumber
    participant R0 as Rank 0
    participant Rk as Rank k
    participant RN as Rank N-1

    R0->>R0: parse CSV → ProblemData
    R0->>Rk: MPI_Bcast(ProblemData)
    R0->>RN: MPI_Bcast(ProblemData)

    par Independent evolution per island
        R0->>R0: select · cross · mutate · repair
    and
        Rk->>Rk: select · cross · mutate · repair
    and
        RN->>RN: select · cross · mutate · repair
    end

    Note over R0,RN: every 50 generations — ring migration
    R0->>Rk: MPI_Sendrecv(best chromosome)
    Rk->>RN: MPI_Sendrecv(best chromosome)
    RN->>R0: MPI_Sendrecv(best chromosome)

    Note over R0,RN: every generation — collective sync
    R0->>Rk: MPI_Allreduce(MINLOC) — global best
    Rk->>RN: MPI_Allreduce(MAX) — terminate?

    R0->>R0: hill-climb global best
    R0->>R0: write schedule.csv + timetable.txt
```

Migration topology — strict ring, deadlock-free thanks to `MPI_Sendrecv`:

```mermaid
flowchart LR
    R0((Rank 0)) -->|"Sendrecv"| R1((Rank 1))
    R1 -->|"Sendrecv"| R2((Rank 2))
    R2 -->|"Sendrecv"| R3((Rank 3))
    R3 -->|"Sendrecv"| Rd((…))
    Rd -->|"Sendrecv"| RN((Rank N-1))
    RN -->|"Sendrecv"| R0
```

Inside a single generation on one rank — the loop body that runs N times in
parallel between two synchronisation points:

```mermaid
flowchart TB
    Start([generation g]) --> Eval[evaluate fitness<br/>H · w_h + S · w_s]
    Eval --> Sel[tournament selection<br/>K = 3, two-level]
    Sel --> Cross[uniform crossover<br/>p_c = 0.8]
    Cross --> Mut{H &gt; 0?}
    Mut -->|yes| MutH[mutate p = 5%]
    Mut -->|no| MutL[mutate p = 1%]
    MutH --> Rep[greedy repair<br/>20 probes + scan]
    MutL --> Rep
    Rep --> Mig{g mod 50 == 0?}
    Mig -->|yes| Send[MPI_Sendrecv → ring]
    Mig -->|no| GB
    Send --> GB[MPI_Allreduce MINLOC<br/>global best]
    GB --> Stop[MPI_Allreduce MAX<br/>terminate flag]
    Stop --> End([generation g+1])
```

## 4. GA pipeline

| Operator | Choice |
|---|---|
| Population | 200 / N islands |
| Selection | Tournament K=3, two-level (hard before soft) |
| Crossover | Uniform 50/50, $p_c = 0.8$ |
| Mutation | Adaptive: 5 % when $H>0$, 1 % when $H=0$ |
| Repair | Greedy: 20 random probes + systematic scan, soft-aware tiebreaker |
| Migration | Ring topology, every 50 generations, `MPI_Sendrecv` |
| Termination | `MPI_Allreduce(MAX)`: stagnation ≥ 200 OR fitness == 0 |
| Local search | Hill climbing, 5000 iters, 15 probes/iter |

### Hard constraints
- Room collision (≥ 2 events sharing a room-block)
- Teacher collision (same teacher in two places)
- Group collision (same group in two places)
- Day overflow (event placed past the day's last slot)

### Soft constraints
- Gaps between classes (×2 penalty)
- Day compactness (prefer fewer days with classes)
- Late slots (blocks 6–7, after 18:30)
- Teacher day compactness
- Room capacity vs. group size
- Building continuity (minimise inter-block building changes)

## 5. Webapp

`webapp.html` is a dark-theme SPA served by `scripts/api_prototype.py`. Tabs:
**Sale** · **Kierunki** · **Studenci** · **Prowadzący** · **Statystyki**.

Start: `bash demo.sh` → http://localhost:8080/.

## 6. File map

```
src/                   GA engine in C99 (10 files)
scripts/
├── api_prototype.py   REST API + webapp server
├── convert_simple.py  UniTime → GA-input CSV (strict 1.5 h block filter)
├── create_db.py       SQLite cache builder
└── pretty_pis2.py     Per-group plan renderer (terminal)
plots/
├── generate_real_pngs.py     matplotlib chart generator
├── speedup_real.png          (used by the report)
├── quality_scaling_real.png  (used by the report)
└── sizeof_problemdata.c      verifies Bcast payload size matches types.h
data/                  input CSVs (not committed — see data/README.md)
results_v3/            benchmark output (per-run + summary CSVs)
webapp.html            dark-theme SPA
benchmark.sh           5-run-per-config benchmark with statistics
demo.sh                local one-click demo
Makefile               build, run, benchmark, plot, archive
```
