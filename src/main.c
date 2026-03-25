/*
 * main.c -- Entry point for the parallel GA timetabling solver.
 *
 * Phase 3: MPI island model. Rank 0 loads timetabling data from CSV files,
 * broadcasts ProblemData to all ranks via MPI_Bcast. All ranks run
 * independent GAs with periodic ring migration. Global best collected
 * via MPI_Allreduce + MPI_MINLOC.
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "types.h"
#include "io.h"
#include "pcg_basic.h"
#include "ga.h"
#include "fitness.h"

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    printf("MPI: rank %d of %d\n", rank, size);
    fflush(stdout);

    /* Initialize PCG32 PRNG with per-rank independent stream */
    pcg32_random_t rng;
    pcg32_srandom_r(&rng, (uint64_t)time(NULL), (uint64_t)rank);

    /* ProblemData on ALL ranks (static to avoid stack overflow with large instances) */
    static ProblemData data;
    memset(&data, 0, sizeof(data));

    if (rank == 0) {
        /* Determine data directory from command line (default: "data/") */
        const char *data_dir = (argc > 1) ? argv[1] : "data/";

        /* Build file paths */
        char path_rooms[256], path_teachers[256], path_groups[256],
             path_courses[256];
        snprintf(path_rooms,    sizeof(path_rooms),    "%srooms.csv",    data_dir);
        snprintf(path_teachers, sizeof(path_teachers), "%steachers.csv", data_dir);
        snprintf(path_groups,   sizeof(path_groups),   "%sgroups.csv",   data_dir);
        snprintf(path_courses,  sizeof(path_courses),  "%scourses.csv",  data_dir);

        /* Set timeslot constants */
        data.num_days = NUM_DAYS;
        data.periods_per_day = PERIODS_PER_DAY;
        data.total_timeslots = TOTAL_TIMESLOTS;

        /* Load all entity types */
        data.num_rooms = load_rooms(path_rooms, data.rooms, MAX_ROOMS);
        if (data.num_rooms < 0) {
            fprintf(stderr, "Fatal: failed to load rooms from %s\n",
                    path_rooms);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        data.num_teachers = load_teachers(path_teachers, data.teachers,
                                          MAX_TEACHERS);
        if (data.num_teachers < 0) {
            fprintf(stderr, "Fatal: failed to load teachers from %s\n",
                    path_teachers);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        data.num_groups = load_groups(path_groups, data.groups, MAX_GROUPS);
        if (data.num_groups < 0) {
            fprintf(stderr, "Fatal: failed to load groups from %s\n",
                    path_groups);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        data.num_courses = load_courses(path_courses, data.courses,
                                        MAX_COURSES);
        if (data.num_courses < 0) {
            fprintf(stderr, "Fatal: failed to load courses from %s\n",
                    path_courses);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        /* Expand courses into events */
        int num_events = expand_events(&data);
        if (num_events < 0) {
            fprintf(stderr, "Fatal: event expansion failed\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        /* Print summary */
        print_summary(&data);
    }

    /* Broadcast ProblemData from rank 0 to all ranks */
    MPI_Bcast(&data, sizeof(ProblemData), MPI_BYTE, 0, MPI_COMM_WORLD);

    /* Sanity check on all ranks after broadcast */
    assert(data.num_events > 0);

    /* Configure GA parameters (same config on all ranks) */
    /* Population divided across ranks (like Pi intervals in Lab 3):
     * Total population = 200, each island evolves 200/size individuals.
     * This gives near-linear speedup: n ranks -> n times less work per rank. */
    int total_population = 200;
    GAConfig config;
    config.population_size     = total_population / size;
    if (config.population_size < 4) config.population_size = 4;  /* minimum viable */
    if (rank == 0)
        printf("Population: %d total, %d per island (%d islands)\n",
               total_population, config.population_size, size);
    config.max_generations     = 500;
    config.stagnation_limit    = 200;
    config.tournament_size     = 3;
    config.crossover_rate      = 0.8;
    config.mutation_rate        = 0.05;
    config.elitism_count       = 1;
    config.max_repair_attempts = 100;
    config.migration_interval  = 50;
    config.num_migrants        = 1;

    /* Per-rank convergence CSV (PERF-05: each island logs separately) */
    char csv_path_buf[64];
    snprintf(csv_path_buf, sizeof(csv_path_buf), "convergence_rank%d.csv", rank);
    const char *csv_path = csv_path_buf;

    /* Synchronize all ranks before timing */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    /* ALL ranks run GA with migration */
    Individual best = run_ga(&data, &config, &rng, csv_path, rank, size);

    /* Local search post-processing: hill climbing on soft constraints.
     * Each rank refines its best solution independently. */
    if (best.fitness_hard == 0) {
        if (rank == 0)
            printf("Starting local search (soft optimization)...\n");
        local_search(&best, &data, 5000, 500, &rng);
        if (rank == 0)
            printf("Local search done: soft=%d (was %d)\n",
                   best.fitness_soft, best.fitness);
    }

    /* Stop timer: measure GA + local search time */
    double t_end = MPI_Wtime();
    double elapsed = t_end - t_start;

    /* Find global best across all ranks */
    struct { int val; int idx; } local_best, global_best;
    local_best.val = best.fitness;
    local_best.idx = rank;

    MPI_Allreduce(&local_best, &global_best, 1, MPI_2INT, MPI_MINLOC,
                  MPI_COMM_WORLD);

    /* Transfer winning chromosome to rank 0 if not already there */
    if (global_best.idx != 0) {
        if (rank == global_best.idx) {
            /* Winner sends chromosome + fitness to rank 0 */
            int buf[MAX_EVENTS * 2];
            for (int i = 0; i < data.num_events; i++) {
                buf[i * 2]     = best.genes[i].timeslot;
                buf[i * 2 + 1] = best.genes[i].room_id;
            }
            MPI_Send(buf, data.num_events * 2, MPI_INT, 0, 99,
                     MPI_COMM_WORLD);
            int fitness_info[3] = {best.fitness_hard, best.fitness_soft,
                                   best.fitness};
            MPI_Send(fitness_info, 3, MPI_INT, 0, 100, MPI_COMM_WORLD);
        }
        if (rank == 0) {
            int buf[MAX_EVENTS * 2];
            MPI_Recv(buf, data.num_events * 2, MPI_INT, global_best.idx, 99,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < data.num_events; i++) {
                best.genes[i].timeslot = buf[i * 2];
                best.genes[i].room_id  = buf[i * 2 + 1];
            }
            int fitness_info[3];
            MPI_Recv(fitness_info, 3, MPI_INT, global_best.idx, 100,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            best.fitness_hard = fitness_info[0];
            best.fitness_soft = fitness_info[1];
            best.fitness      = fitness_info[2];
        }
    }

    /* Rank 0 prints results and writes timetable */
    if (rank == 0) {
        printf("\n=== Global Best Solution (from rank %d) ===\n",
               global_best.idx);
        printf("Hard violations: %d\n", best.fitness_hard);
        printf("Soft violations: %d\n", best.fitness_soft);
        printf("Total fitness:   %d\n", best.fitness);
        printf("Wall-clock time: %.3f seconds\n", elapsed);

        write_timetable("timetable.txt", &best, &data);
        write_schedule_csv("schedule.csv", &best, &data);
    }

    MPI_Finalize();
    return 0;
}
