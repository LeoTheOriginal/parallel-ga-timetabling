/*
 * ga.h -- Genetic Algorithm engine for the university timetabling problem.
 *
 * Provides population initialization (lab-aware), tournament selection (K=3),
 * uniform crossover, per-gene mutation, and the generational main loop with
 * elitism, stagnation stop, and convergence CSV logging.
 */

#ifndef GA_H
#define GA_H

#include "types.h"
#include "pcg_basic.h"

/* Initialize one individual with random (timeslot, room) assignments.
 * Lab events get random lab rooms; non-lab events get any room.
 * lab_room_ids: pre-computed array of room IDs where is_lab==1.
 * num_lab_rooms: length of lab_room_ids array. */
void init_individual(Individual *ind, const ProblemData *data,
                     pcg32_random_t *rng,
                     const int *lab_room_ids, int num_lab_rooms);

/* Tournament selection: pick tournament_size random individuals,
 * return index of best (two-level comparison: hard first, then soft). */
int tournament_select(const Individual *pop, int pop_size,
                      int tournament_size, pcg32_random_t *rng);

/* Uniform crossover: for each gene, randomly pick from p1 or p2
 * with 50/50 probability. Result written to child. */
void uniform_crossover(const Individual *p1, const Individual *p2,
                       Individual *child, int num_events,
                       pcg32_random_t *rng);

/* Per-gene mutation: each gene has mutation_rate probability of being
 * reassigned to random timeslot + lab-aware random room. */
void mutate(Individual *ind, int num_events, const ProblemData *data,
            double mutation_rate, pcg32_random_t *rng,
            const int *lab_room_ids, int num_lab_rooms);

/* Run the complete GA. Returns the best Individual found.
 * Writes convergence log to convergence_csv_path (NULL to skip).
 * All ranks may provide convergence_csv_path for per-island
 * convergence logging (PERF-05).
 * Prints progress to stdout every 100 generations (rank 0 only).
 *
 * mpi_rank: MPI rank of calling process (0-indexed)
 * mpi_size: total number of MPI processes
 * When mpi_size > 1: migration occurs every config->migration_interval
 * generations, early termination is disabled, and only rank 0 prints
 * progress. */
Individual run_ga(const ProblemData *data, const GAConfig *config,
                  pcg32_random_t *rng, const char *convergence_csv_path,
                  int mpi_rank, int mpi_size);

#endif /* GA_H */
