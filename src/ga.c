/*
 * ga.c -- Complete GA engine for university timetabling.
 *
 * Implements lab-aware random initialization, tournament selection (K=3),
 * uniform crossover (50/50 per gene), per-gene mutation (5%), elitism
 * (top 1 preserved), and a generational main loop with convergence CSV
 * logging and stagnation stop.
 *
 * References:
 *   Banczyk et al. "Parallelisation of GA for University Timetabling"
 *   (PARELEC'06) -- two-level evaluation, stagnation stop, elitism.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <mpi.h>

#include "ga.h"
#include "fitness.h"
#include "pcg_basic.h"

/* ---------------------------------------------------------------------------
 * init_individual -- Random lab-aware initialization of one individual.
 *
 * For each event:
 *   - Assign random timeslot in [0, TOTAL_TIMESLOTS)
 *   - If event requires lab: pick random lab room from lab_room_ids
 *   - Else: pick any room from data->rooms[]
 *
 * Room IDs are stored 1-indexed (matching Room.id from CSV).
 * --------------------------------------------------------------------------- */
void init_individual(Individual *ind, const ProblemData *data,
                     pcg32_random_t *rng,
                     const int *lab_room_ids, int num_lab_rooms)
{
    for (int i = 0; i < data->num_events; i++) {
        int dur = data->events[i].duration_slots;
        /* Pick random day and valid start period so event fits within day */
        int day = (int)pcg32_boundedrand_r(rng, (uint32_t)NUM_DAYS);
        int max_period = PERIODS_PER_DAY - dur;
        if (max_period < 0) max_period = 0;
        int period = (int)pcg32_boundedrand_r(rng, (uint32_t)(max_period + 1));
        ind->genes[i].timeslot = DAY_PERIOD_TO_TS(day, period);

        if (data->events[i].requires_lab && num_lab_rooms > 0) {
            int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)num_lab_rooms);
            ind->genes[i].room_id = lab_room_ids[idx];
        } else {
            int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)data->num_rooms);
            ind->genes[i].room_id = data->rooms[idx].id;
        }
    }

    ind->fitness_hard = 0;
    ind->fitness_soft = 0;
    ind->fitness = 0;
}

/* ---------------------------------------------------------------------------
 * tournament_select -- Tournament selection with two-level comparison.
 *
 * Pick tournament_size random individuals from the population.
 * Return the index of the best one (fewest hard violations; if tied,
 * fewest soft violations).
 * --------------------------------------------------------------------------- */
int tournament_select(const Individual *pop, int pop_size,
                      int tournament_size, pcg32_random_t *rng)
{
    int best_idx = (int)pcg32_boundedrand_r(rng, (uint32_t)pop_size);

    for (int i = 1; i < tournament_size; i++) {
        int candidate = (int)pcg32_boundedrand_r(rng, (uint32_t)pop_size);

        /* Two-level comparison: fewer hard violations wins;
         * if tied, fewer soft violations wins */
        if (compare_individuals(&pop[candidate], &pop[best_idx]) < 0) {
            best_idx = candidate;
        }
    }

    return best_idx;
}

/* ---------------------------------------------------------------------------
 * uniform_crossover -- 50/50 per-gene crossover.
 *
 * For each gene position, randomly select from parent1 or parent2
 * with equal probability.
 * --------------------------------------------------------------------------- */
void uniform_crossover(const Individual *p1, const Individual *p2,
                       Individual *child, int num_events,
                       pcg32_random_t *rng)
{
    for (int i = 0; i < num_events; i++) {
        if (pcg32_boundedrand_r(rng, 2) == 0) {
            child->genes[i] = p1->genes[i];
        } else {
            child->genes[i] = p2->genes[i];
        }
    }
}

/* ---------------------------------------------------------------------------
 * mutate -- Per-gene mutation with lab-aware room reassignment.
 *
 * Each gene has mutation_rate probability of being reassigned to a
 * random timeslot and a random lab-aware room.
 *
 * Uses PCG32 full-range comparison for efficient mutation probability:
 * threshold = mutation_rate * 2^32, then pcg32_random_r() < threshold.
 * --------------------------------------------------------------------------- */
void mutate(Individual *ind, int num_events, const ProblemData *data,
            double mutation_rate, pcg32_random_t *rng,
            const int *lab_room_ids, int num_lab_rooms)
{
    /* Convert mutation_rate to integer threshold for PCG comparison */
    uint32_t threshold = (uint32_t)(mutation_rate * 4294967296.0);

    for (int i = 0; i < num_events; i++) {
        if (pcg32_random_r(rng) < threshold) {
            int dur = data->events[i].duration_slots;
            int day = (int)pcg32_boundedrand_r(rng, (uint32_t)NUM_DAYS);
            int max_period = PERIODS_PER_DAY - dur;
            if (max_period < 0) max_period = 0;
            int period = (int)pcg32_boundedrand_r(rng, (uint32_t)(max_period + 1));
            ind->genes[i].timeslot = DAY_PERIOD_TO_TS(day, period);

            if (data->events[i].requires_lab && num_lab_rooms > 0) {
                int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)num_lab_rooms);
                ind->genes[i].room_id = lab_room_ids[idx];
            } else {
                int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)data->num_rooms);
                ind->genes[i].room_id = data->rooms[idx].id;
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * pack_genes -- Serialize Individual's genes to flat int buffer.
 *
 * For each event i: buf[i*2] = timeslot, buf[i*2+1] = room_id.
 * Buffer must be at least num_events * 2 ints.
 * --------------------------------------------------------------------------- */
static void pack_genes(const Individual *ind, int *buf, int num_events)
{
    for (int i = 0; i < num_events; i++) {
        buf[i * 2]     = ind->genes[i].timeslot;
        buf[i * 2 + 1] = ind->genes[i].room_id;
    }
}

/* ---------------------------------------------------------------------------
 * unpack_genes -- Deserialize flat int buffer into Individual's genes.
 *
 * Reverse of pack_genes: reads timeslot and room_id from buffer.
 * --------------------------------------------------------------------------- */
static void unpack_genes(Individual *ind, const int *buf, int num_events)
{
    for (int i = 0; i < num_events; i++) {
        ind->genes[i].timeslot = buf[i * 2];
        ind->genes[i].room_id  = buf[i * 2 + 1];
    }
}

/* ---------------------------------------------------------------------------
 * perform_migration -- Ring migration with MPI_Sendrecv.
 *
 * Sends best individual to right neighbor (rank+1) and receives from left
 * neighbor (rank-1) in a ring topology. Uses MPI_Sendrecv for deadlock-free
 * synchronous exchange. Received migrant replaces the worst individual in
 * the local population if the migrant is better.
 *
 * MPI_Barrier before and after exchange per CONTEXT.md locked decision.
 * --------------------------------------------------------------------------- */
static void perform_migration(Individual *pop, int pop_size,
                               const ProblemData *data,
                               int rank, int size)
{
    int dest   = (rank + 1) % size;
    int source = (rank - 1 + size) % size;
    int count  = data->num_events * 2;

    int send_buf[MAX_EVENTS * 2];
    int recv_buf[MAX_EVENTS * 2];

    /* Find best individual in local population */
    int best_idx = 0;
    for (int i = 1; i < pop_size; i++) {
        if (compare_individuals(&pop[i], &pop[best_idx]) < 0)
            best_idx = i;
    }

    /* Pack best individual's genes */
    pack_genes(&pop[best_idx], send_buf, data->num_events);

    /* Deadlock-free ring exchange (MPI_Sendrecv handles ordering) */
    MPI_Sendrecv(send_buf, count, MPI_INT, dest,   0,
                 recv_buf, count, MPI_INT, source, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    /* Unpack received individual */
    Individual migrant;
    memset(&migrant, 0, sizeof(migrant));
    unpack_genes(&migrant, recv_buf, data->num_events);
    evaluate_fitness(&migrant, data);

    /* Find worst individual in local population */
    int worst_idx = 0;
    for (int i = 1; i < pop_size; i++) {
        if (compare_individuals(&pop[i], &pop[worst_idx]) > 0)
            worst_idx = i;
    }

    /* Replace worst if migrant is better */
    if (compare_individuals(&migrant, &pop[worst_idx]) < 0) {
        memcpy(&pop[worst_idx], &migrant, sizeof(Individual));
    }
}

/* ---------------------------------------------------------------------------
 * run_ga -- Complete generational GA with elitism and convergence logging.
 *
 * 1. Pre-compute lab room IDs
 * 2. Initialize population randomly (lab-aware)
 * 3. Evaluate all individuals
 * 4. Main loop:
 *    a. Log convergence CSV row
 *    b. Print progress every 100 generations
 *    c. Check termination (perfect solution, stagnation)
 *    d. Elitism: copy best to next_pop[0]
 *    e. Fill remaining slots: select, crossover, mutate, repair, evaluate
 *    f. Swap populations
 * 5. Return best individual found
 * --------------------------------------------------------------------------- */
Individual run_ga(const ProblemData *data, const GAConfig *config,
                  pcg32_random_t *rng, const char *convergence_csv_path,
                  int mpi_rank, int mpi_size)
{
    /* 1. Pre-compute lab room IDs for O(1) random lab room selection */
    int lab_room_ids[MAX_ROOMS];
    int num_lab_rooms = 0;
    for (int r = 0; r < data->num_rooms; r++) {
        if (data->rooms[r].is_lab) {
            lab_room_ids[num_lab_rooms++] = data->rooms[r].id;
        }
    }

    /* 2. Allocate population arrays in BSS segment (too large for stack).
     * Size matches max possible population_size. */
    #define MAX_POP_SIZE 200
    static Individual pop[MAX_POP_SIZE];
    static Individual next_pop[MAX_POP_SIZE];

    if (config->population_size > MAX_POP_SIZE) {
        fprintf(stderr, "Error: population_size %d exceeds MAX_POP_SIZE %d\n",
                config->population_size, MAX_POP_SIZE);
        Individual empty;
        memset(&empty, 0, sizeof(empty));
        empty.fitness = INT_MAX;
        return empty;
    }

    /* 3. Initialize population with random lab-aware assignments */
    for (int i = 0; i < config->population_size; i++) {
        init_individual(&pop[i], data, rng, lab_room_ids, num_lab_rooms);
        evaluate_fitness(&pop[i], data);
    }

    /* 4. Find initial best individual */
    int best_idx = 0;
    for (int i = 1; i < config->population_size; i++) {
        if (compare_individuals(&pop[i], &pop[best_idx]) < 0) {
            best_idx = i;
        }
    }
    Individual best_ever;
    memcpy(&best_ever, &pop[best_idx], sizeof(Individual));
    int best_fitness_ever = best_ever.fitness;

    /* 5. Open convergence CSV for logging (only when path is provided) */
    FILE *csv = NULL;
    if (convergence_csv_path) {
        csv = fopen(convergence_csv_path, "w");
        if (!csv) {
            fprintf(stderr, "Warning: cannot open %s for writing\n",
                    convergence_csv_path);
        }
    }
    if (csv) {
        fprintf(csv, "generation,best_hard,best_soft,best_fitness,"
                     "avg_fitness,worst_fitness\n");
    }

    /* 6. Stagnation counter */
    int stagnation_counter = 0;
    int final_gen = 0;

    /* 7. Main generational loop */
    for (int gen = 0; gen < config->max_generations; gen++) {
        final_gen = gen;

        /* a. Find current generation stats: best, avg, worst */
        int gen_best_idx = 0;
        double sum_fitness = 0.0;
        int worst_fitness = pop[0].fitness;

        for (int i = 0; i < config->population_size; i++) {
            sum_fitness += pop[i].fitness;
            if (pop[i].fitness > worst_fitness)
                worst_fitness = pop[i].fitness;
            if (compare_individuals(&pop[i], &pop[gen_best_idx]) < 0)
                gen_best_idx = i;
        }

        double avg_fitness = sum_fitness / config->population_size;
        const Individual *current_best = &pop[gen_best_idx];

        /* Log convergence CSV row */
        if (csv) {
            fprintf(csv, "%d,%d,%d,%d,%.2f,%d\n",
                    gen,
                    current_best->fitness_hard,
                    current_best->fitness_soft,
                    current_best->fitness,
                    avg_fitness,
                    worst_fitness);
        }

        /* b. Progress print every 100 generations (rank 0 only) */
        if (gen % 100 == 0 && mpi_rank == 0) {
            printf("Gen %d: best_hard=%d best_soft=%d fitness=%d avg=%.1f\n",
                   gen,
                   current_best->fitness_hard,
                   current_best->fitness_soft,
                   current_best->fitness,
                   avg_fitness);
            fflush(stdout);
        }

        /* c. Check termination conditions */

        /* Update best_ever and stagnation counter */
        if (current_best->fitness < best_fitness_ever) {
            best_fitness_ever = current_best->fitness;
            stagnation_counter = 0;
            memcpy(&best_ever, current_best, sizeof(Individual));
        } else {
            stagnation_counter++;
        }

        /* Determine if this rank wants to stop */
        int local_stop = 0;
        if (current_best->fitness == 0) {
            local_stop = 1;
        }
        if (stagnation_counter >= config->stagnation_limit) {
            local_stop = 1;
        }

        /* In parallel mode: synchronize termination across all ranks.
         * Any rank finding a stop condition triggers global stop (OR semantics). */
        if (mpi_size > 1) {
            int global_stop = 0;
            MPI_Allreduce(&local_stop, &global_stop, 1, MPI_INT,
                          MPI_MAX, MPI_COMM_WORLD);
            if (global_stop) {
                if (mpi_rank == 0)
                    printf("Synchronized stop at generation %d\n", gen);
                break;
            }
        } else {
            if (local_stop) {
                if (current_best->fitness == 0)
                    printf("Perfect solution found at generation %d\n", gen);
                else
                    printf("Stagnation stop at generation %d\n", gen);
                break;
            }
        }

        /* d. Elitism: copy best individual to next_pop[0] */
        memcpy(&next_pop[0], &best_ever, sizeof(Individual));

        /* e. Fill remaining slots (i = 1 to pop_size - 1) */
        for (int i = 1; i < config->population_size; i++) {
            /* Select two parents via tournament */
            int parent1 = tournament_select(pop, config->population_size,
                                            config->tournament_size, rng);
            int parent2 = tournament_select(pop, config->population_size,
                                            config->tournament_size, rng);

            /* Crossover check */
            if (pcg32_boundedrand_r(rng, 100) <
                (uint32_t)(config->crossover_rate * 100)) {
                uniform_crossover(&pop[parent1], &pop[parent2],
                                  &next_pop[i], data->num_events, rng);
            } else {
                /* No crossover: copy better parent */
                memcpy(&next_pop[i], &pop[parent1], sizeof(Individual));
            }

            /* Mutate — adaptive rate: high (5%) when hard>0, low (1%) when hard=0
             * to preserve good soft assignments while still exploring */
            double mut_rate = config->mutation_rate;
            if (best_ever.fitness_hard == 0) {
                mut_rate = 0.01;  /* reduced after feasibility achieved */
            }
            mutate(&next_pop[i], data->num_events, data,
                   mut_rate, rng,
                   lab_room_ids, num_lab_rooms);

            /* Repair hard constraint violations.
             * repair_individual calls evaluate_fitness at the end,
             * so no separate evaluate_fitness call needed here. */
            repair_individual(&next_pop[i], data,
                              config->max_repair_attempts, rng);
        }

        /* f. Swap populations */
        memcpy(pop, next_pop, sizeof(Individual) * config->population_size);

        /* g. Migration step (parallel mode only) */
        if (mpi_size > 1 && config->migration_interval > 0 &&
            gen > 0 && gen % config->migration_interval == 0) {
            perform_migration(pop, config->population_size, data,
                              mpi_rank, mpi_size);
        }
    }

    /* 8. Write final CSV row if loop ended naturally (not by break) */
    /* The break cases already logged their final generation in the loop */

    /* 9. Close CSV file */
    if (csv) {
        fclose(csv);
    }

    /* 10. Print final result (rank 0 only) */
    if (mpi_rank == 0) {
        printf("GA finished: %d generations, best fitness=%d (hard=%d, soft=%d)\n",
               final_gen + 1,
               best_ever.fitness,
               best_ever.fitness_hard,
               best_ever.fitness_soft);
    }

    /* 11. Return best individual */
    return best_ever;
}
