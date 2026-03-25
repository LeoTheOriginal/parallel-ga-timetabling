/*
 * fitness.h -- Fitness evaluation and repair operator for the GA.
 *
 * Two-level fitness evaluation (hard, soft) per Banczyk et al.
 * Hard constraints compared first; soft constraints only matter when hard == 0.
 */

#ifndef FITNESS_H
#define FITNESS_H

#include "types.h"
#include "pcg_basic.h"

/* Evaluate hard constraint violations for one individual.
 * Returns total hard violation count (room collision + teacher collision +
 * group collision + room-type mismatch). Each excess occupancy = 1 violation.
 * E.g., 3 events in same room at same timeslot = 2 violations. */
int evaluate_hard(const Individual *ind, const ProblemData *data);

/* Evaluate soft constraint penalties for one individual.
 * Returns total soft penalty (gaps + load imbalance + late classes).
 * Only meaningful when fitness_hard == 0 (per Banczyk two-level evaluation). */
int evaluate_soft(const Individual *ind, const ProblemData *data);

/* Evaluate full fitness: sets ind->fitness_hard, ind->fitness_soft, ind->fitness.
 * fitness = fitness_hard * 1000 + fitness_soft. */
void evaluate_fitness(Individual *ind, const ProblemData *data);

/* Two-level comparison: returns negative if a is better than b.
 * Compares fitness_hard first; only compares fitness_soft when hard is tied. */
int compare_individuals(const Individual *a, const Individual *b);

/* Greedy repair operator: attempts to fix hard constraint violations.
 * Iterates up to max_attempts times. For each violation found, tries random
 * conflict-free (timeslot, room) reassignment (10 random probes), then falls
 * back to systematic scan of all (timeslot, room) pairs picking
 * least-conflicting. Re-evaluates fitness after all repair attempts. */
void repair_individual(Individual *ind, const ProblemData *data,
                       int max_attempts, pcg32_random_t *rng);

/* Local search (hill climbing) on soft constraints.
 * Assumes hard violations == 0. For each iteration, picks a random event
 * and tries random reassignments that maintain hard=0 but improve soft.
 * Stops after max_iterations or when no improvement found for patience iters. */
void local_search(Individual *ind, const ProblemData *data,
                  int max_iterations, int patience, pcg32_random_t *rng);

#endif /* FITNESS_H */
