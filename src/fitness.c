/*
 * fitness.c -- Two-level fitness evaluation and greedy repair operator.
 *
 * Implements hard constraint counting (room collision, teacher collision,
 * group collision, room-type mismatch), soft constraint counting (gaps,
 * load imbalance, late classes), combined fitness scoring, two-level
 * individual comparison, and a greedy repair operator with random probe
 * + systematic scan fallback.
 *
 * References:
 *   Banczyk et al. "Parallelisation of GA for University Timetabling"
 *   (PARELEC'06) -- two-level evaluation (h, s) with hmin = 0 comparison.
 */

#include <string.h>
#include <math.h>

#include "fitness.h"
#include "pcg_basic.h"

/* ---------------------------------------------------------------------------
 * Helper: count hard violations for a single event at a given (timeslot, room)
 * against the current occupancy arrays (which include all OTHER events).
 *
 * This avoids rebuilding full occupancy arrays when checking candidate
 * reassignments during repair.
 * --------------------------------------------------------------------------- */
static int count_single_event_violations(
    int event_idx,
    int start_ts,
    int room_id,
    const ProblemData *data,
    int room_occ[][MAX_ROOMS],
    int teacher_occ[][MAX_TEACHERS],
    int group_occ[][MAX_GROUPS])
{
    int violations = 0;
    int room_idx = room_id - 1;
    int dur = data->events[event_idx].duration_slots;
    int tid = data->events[event_idx].teacher_id - 1;
    int gid = data->events[event_idx].group_id - 1;
    int start_day = TIMESLOT_TO_DAY(start_ts);
    int start_period = TIMESLOT_TO_PERIOD(start_ts);

    /* Room-type mismatch */
    if (data->events[event_idx].requires_lab && !data->rooms[room_idx].is_lab)
        violations++;

    /* Day overflow: event extends past end of day */
    if (start_period + dur > PERIODS_PER_DAY)
        violations += (start_period + dur - PERIODS_PER_DAY);

    /* Check each occupied slot */
    int pgid = data->events[event_idx].parent_group_id;
    for (int s = 0; s < dur; s++) {
        int ts = start_ts + s;
        if (ts >= TOTAL_TIMESLOTS || TIMESLOT_TO_DAY(ts) != start_day)
            break;
        if (room_occ[ts][room_idx] > 0)
            violations++;
        if (teacher_occ[ts][tid] > 0)
            violations++;
        if (group_occ[ts][gid] > 0)
            violations++;
        /* Lecture-subgroup: if this event has a parent, check lecture overlap */
        if (pgid > 0 && pgid <= MAX_GROUPS && group_occ[ts][pgid - 1] > 0)
            violations++;
    }

    return violations;
}

/* ---------------------------------------------------------------------------
 * evaluate_hard -- Count total hard constraint violations for an individual.
 *
 * Hard constraints (each excess occupancy = 1 violation):
 *   1. Room collision:  two+ events in same room at same timeslot
 *   2. Teacher collision: same teacher has two+ events at same timeslot
 *   3. Group collision: same group has two+ events at same timeslot
 *   4. Room-type mismatch: lab event (requires_lab=1) in non-lab room (is_lab=0)
 * --------------------------------------------------------------------------- */
int evaluate_hard(const Individual *ind, const ProblemData *data)
{
    /* Occupancy tracking arrays -- zeroed each call.
     * Static to avoid stack overflow with large MAX_GROUPS. */
    static int room_occ[TOTAL_TIMESLOTS][MAX_ROOMS];
    static int teacher_occ[TOTAL_TIMESLOTS][MAX_TEACHERS];
    static int group_occ[TOTAL_TIMESLOTS][MAX_GROUPS];
    memset(room_occ, 0, sizeof(room_occ));
    memset(teacher_occ, 0, sizeof(teacher_occ));
    memset(group_occ, 0, sizeof(group_occ));

    int violations = 0;

    /* First pass: populate occupancy and count per-event violations */
    for (int i = 0; i < data->num_events; i++) {
        int start_ts = ind->genes[i].timeslot;
        int rid = ind->genes[i].room_id;
        int room_idx = rid - 1;
        int tid = data->events[i].teacher_id - 1;
        int gid = data->events[i].group_id - 1;
        int dur = data->events[i].duration_slots;
        int start_day = TIMESLOT_TO_DAY(start_ts);
        int start_period = TIMESLOT_TO_PERIOD(start_ts);

        /* Room-type mismatch: lab event in non-lab room */
        if (data->events[i].requires_lab && !data->rooms[room_idx].is_lab)
            violations++;

        /* Day overflow: event extends past end of day */
        if (start_period + dur > PERIODS_PER_DAY)
            violations += (start_period + dur - PERIODS_PER_DAY);

        /* Populate occupancy for each slot this event occupies */
        for (int s = 0; s < dur; s++) {
            int ts = start_ts + s;
            if (ts >= data->total_timeslots || TIMESLOT_TO_DAY(ts) != start_day)
                break;
            room_occ[ts][room_idx]++;
            teacher_occ[ts][tid]++;
            group_occ[ts][gid]++;
        }
    }

    /* Second pass: count collisions (excess occupancy) */
    for (int ts = 0; ts < data->total_timeslots; ts++) {
        for (int r = 0; r < data->num_rooms; r++)
            if (room_occ[ts][r] > 1)
                violations += room_occ[ts][r] - 1;
        for (int t = 0; t < data->num_teachers; t++)
            if (teacher_occ[ts][t] > 1)
                violations += teacher_occ[ts][t] - 1;
        for (int g = 0; g < data->num_groups; g++)
            if (group_occ[ts][g] > 1)
                violations += group_occ[ts][g] - 1;
    }

    /* Third pass: lecture-subgroup conflicts.
     * Wykład events use group_id = parent (program-level) group.
     * Non-Wykład events keep their subgroup but have parent_group_id set.
     * If a non-Wykład event overlaps with ANY event using its parent_group_id
     * as group_id (i.e., a Wykład), that's a hard violation. */
    for (int i = 0; i < data->num_events; i++) {
        int pgid = data->events[i].parent_group_id;
        if (pgid <= 0 || pgid > data->num_groups) continue;

        int start_ts = ind->genes[i].timeslot;
        int dur = data->events[i].duration_slots;
        int start_day = TIMESLOT_TO_DAY(start_ts);

        for (int s = 0; s < dur; s++) {
            int ts = start_ts + s;
            if (ts >= data->total_timeslots || TIMESLOT_TO_DAY(ts) != start_day)
                break;
            if (group_occ[ts][pgid - 1] > 0)
                violations++;
        }
    }

    return violations;
}

/* ---------------------------------------------------------------------------
 * evaluate_soft -- Count total soft constraint penalties for an individual.
 *
 * Soft constraints (weight 1 each):
 *   1. Gaps: empty periods between first and last class for a group on a day
 *   2. Load imbalance: deviation from ideal daily load per group
 *   3. Late classes: events scheduled in the last period (period index 7)
 * --------------------------------------------------------------------------- */
int evaluate_soft(const Individual *ind, const ProblemData *data)
{
    int penalty = 0;

    /* Per-group, per-day occupancy bitmap + stats.
     * Static to avoid blowing the stack with large MAX_GROUPS. */
    static int group_day_slots[MAX_GROUPS][NUM_DAYS];  /* count of occupied slots */
    static int group_day_min[MAX_GROUPS][NUM_DAYS];
    static int group_day_max[MAX_GROUPS][NUM_DAYS];

    memset(group_day_slots, 0, sizeof(group_day_slots));
    for (int g = 0; g < data->num_groups; g++)
        for (int d = 0; d < NUM_DAYS; d++) {
            group_day_min[g][d] = PERIODS_PER_DAY;
            group_day_max[g][d] = -1;
        }

    /* First pass: populate per-group per-day stats + capacity/late checks */
    for (int i = 0; i < data->num_events; i++) {
        int start_ts = ind->genes[i].timeslot;
        int day = TIMESLOT_TO_DAY(start_ts);
        int start_period = TIMESLOT_TO_PERIOD(start_ts);
        int dur = data->events[i].duration_slots;
        int gid = data->events[i].group_id - 1;
        int rid = ind->genes[i].room_id - 1;

        /* Capacity mismatch penalty (soft — estimation may be imprecise) */
        if (gid >= 0 && gid < data->num_groups && rid >= 0 && rid < data->num_rooms) {
            int students = data->groups[gid].num_students;
            int capacity = data->rooms[rid].capacity;
            if (students > capacity)
                penalty += (students - capacity) / 5 + 1;  /* scaled penalty */
        }

        /* Each slot this event occupies */
        for (int s = 0; s < dur; s++) {
            int period = start_period + s;
            if (period >= PERIODS_PER_DAY) break;

            group_day_slots[gid][day]++;
            if (period < group_day_min[gid][day])
                group_day_min[gid][day] = period;
            if (period > group_day_max[gid][day])
                group_day_max[gid][day] = period;

            /* Late class penalty: slots 12-13 (after 18:30) */
            if (period >= PERIODS_PER_DAY - 2)
                penalty += 1;
        }
    }

    /* Second pass: gap penalty + load balance per group */
    for (int g = 0; g < data->num_groups; g++) {
        int total_slots = 0;

        for (int d = 0; d < NUM_DAYS; d++) {
            int count = group_day_slots[g][d];
            total_slots += count;

            /* Gaps: span minus occupied slots (weight 2 — gaps are very disruptive) */
            if (count >= 2) {
                int span = group_day_max[g][d] - group_day_min[g][d] + 1;
                if (span > count)
                    penalty += 2 * (span - count);
            }
        }

        /* Compact days: prefer fewer days with classes (student-friendly).
         * Penalty for each day that has ANY classes but few slots
         * (encourages packing events into fewer, fuller days).
         * Also: bonus for completely free days (no penalty for empty days). */
        if (total_slots > 0) {
            int days_used = 0;
            for (int d = 0; d < NUM_DAYS; d++) {
                if (group_day_slots[g][d] > 0) days_used++;
            }
            /* Ideal: pack into ceil(total_slots / SLOTS_COMFORTABLE) days
             * where SLOTS_COMFORTABLE = 6 (3 blocks of 90min = 6 slots) */
            int ideal_days = (total_slots + 5) / 6;  /* ceil(total/6) */
            if (ideal_days < 1) ideal_days = 1;
            if (ideal_days > NUM_DAYS) ideal_days = NUM_DAYS;

            /* Penalty per extra day used beyond ideal */
            if (days_used > ideal_days)
                penalty += 3 * (days_used - ideal_days);

            /* Light penalty for very uneven days (one day overloaded) */
            for (int d = 0; d < NUM_DAYS; d++) {
                if (group_day_slots[g][d] > 10)  /* >5h in one day is exhausting */
                    penalty += (group_day_slots[g][d] - 10);
            }
        }
    }

    /* Teacher compactness: prefer fewer teaching days per teacher.
     * Uses teacher_id occupancy per day. */
    {
        static int teacher_days[MAX_TEACHERS][NUM_DAYS];
        memset(teacher_days, 0, sizeof(teacher_days));

        for (int i = 0; i < data->num_events; i++) {
            int tid = data->events[i].teacher_id - 1;
            int day = TIMESLOT_TO_DAY(ind->genes[i].timeslot);
            if (tid >= 0 && tid < data->num_teachers)
                teacher_days[tid][day] += data->events[i].duration_slots;
        }

        for (int t = 0; t < data->num_teachers; t++) {
            int t_days = 0, t_slots = 0;
            for (int d = 0; d < NUM_DAYS; d++) {
                if (teacher_days[t][d] > 0) t_days++;
                t_slots += teacher_days[t][d];
            }
            if (t_slots > 0) {
                int ideal = (t_slots + 7) / 8;  /* ceil(total/8) — 4h/day max ideal */
                if (ideal < 1) ideal = 1;
                if (ideal > NUM_DAYS) ideal = NUM_DAYS;
                if (t_days > ideal)
                    penalty += (t_days - ideal);  /* lighter weight than student */
            }
        }
    }

    /* Building continuity: penalize events for same group on same day
     * that use different buildings. O(groups * days) using occupancy.
     * Each group-day tracks set of buildings used; >1 building = penalty. */
    {
        /* Track unique buildings per group per day using bitmask (max 4 buildings) */
        static int group_day_bldg[MAX_GROUPS][NUM_DAYS];
        memset(group_day_bldg, 0, sizeof(group_day_bldg));

        for (int i = 0; i < data->num_events; i++) {
            int gid = data->events[i].group_id - 1;
            int day = TIMESLOT_TO_DAY(ind->genes[i].timeslot);
            int rid = ind->genes[i].room_id - 1;
            if (gid >= 0 && gid < data->num_groups &&
                rid >= 0 && rid < data->num_rooms) {
                group_day_bldg[gid][day] |= (1 << data->rooms[rid].building_id);
            }
        }

        for (int g = 0; g < data->num_groups; g++) {
            for (int d = 0; d < NUM_DAYS; d++) {
                int mask = group_day_bldg[g][d];
                if (mask == 0) continue;
                /* Count bits set (number of different buildings used) */
                int n_buildings = 0;
                while (mask) { n_buildings += mask & 1; mask >>= 1; }
                if (n_buildings > 1)
                    penalty += (n_buildings - 1);  /* 1 per extra building */
            }
        }
    }

    return penalty;
}

/* ---------------------------------------------------------------------------
 * evaluate_fitness -- Evaluate combined fitness for an individual.
 *
 * Sets fitness_hard, fitness_soft, and combined fitness.
 * fitness = fitness_hard * 1000 + fitness_soft
 * --------------------------------------------------------------------------- */
void evaluate_fitness(Individual *ind, const ProblemData *data)
{
    ind->fitness_hard = evaluate_hard(ind, data);
    ind->fitness_soft = evaluate_soft(ind, data);
    ind->fitness = ind->fitness_hard * 1000 + ind->fitness_soft;
}

/* ---------------------------------------------------------------------------
 * compare_individuals -- Two-level comparison per Banczyk et al.
 *
 * Returns negative if a is better than b, positive if b is better, 0 if equal.
 * Compares fitness_hard first; only compares fitness_soft when hard is tied.
 * --------------------------------------------------------------------------- */
int compare_individuals(const Individual *a, const Individual *b)
{
    if (a->fitness_hard != b->fitness_hard)
        return a->fitness_hard - b->fitness_hard;
    return a->fitness_soft - b->fitness_soft;
}

/* ---------------------------------------------------------------------------
 * repair_individual -- Greedy repair operator.
 *
 * For each repair attempt:
 *   1. Evaluate current hard violations; if 0, done.
 *   2. Find first event involved in a hard violation.
 *   3. Try 10 random conflict-free (timeslot, room) reassignments.
 *   4. If no conflict-free found, do systematic scan of all (timeslot, room)
 *      pairs and pick the one with minimum violations.
 *   5. Apply the best reassignment found.
 *
 * After all attempts (or early break), re-evaluate full fitness.
 * --------------------------------------------------------------------------- */
void repair_individual(Individual *ind, const ProblemData *data,
                       int max_attempts, pcg32_random_t *rng)
{
    /* Pre-compute lab room IDs for efficient random lab room selection */
    int lab_room_ids[MAX_ROOMS];
    int num_lab_rooms = 0;
    for (int r = 0; r < data->num_rooms; r++) {
        if (data->rooms[r].is_lab) {
            lab_room_ids[num_lab_rooms++] = data->rooms[r].id;
        }
    }

    for (int attempt = 0; attempt < max_attempts; attempt++) {
        /* Build occupancy arrays. Static to avoid stack overflow. */
        static int room_occ[TOTAL_TIMESLOTS][MAX_ROOMS];
        static int teacher_occ[TOTAL_TIMESLOTS][MAX_TEACHERS];
        static int group_occ[TOTAL_TIMESLOTS][MAX_GROUPS];
        memset(room_occ, 0, sizeof(room_occ));
        memset(teacher_occ, 0, sizeof(teacher_occ));
        memset(group_occ, 0, sizeof(group_occ));

        int has_violations = 0;

        /* Populate occupancy (multi-slot aware) */
        for (int i = 0; i < data->num_events; i++) {
            int start_ts = ind->genes[i].timeslot;
            int room_idx = ind->genes[i].room_id - 1;
            int tid = data->events[i].teacher_id - 1;
            int gid = data->events[i].group_id - 1;
            int dur = data->events[i].duration_slots;
            int start_day = TIMESLOT_TO_DAY(start_ts);

            for (int s = 0; s < dur; s++) {
                int ts = start_ts + s;
                if (ts >= data->total_timeslots || TIMESLOT_TO_DAY(ts) != start_day)
                    break;
                room_occ[ts][room_idx]++;
                teacher_occ[ts][tid]++;
                group_occ[ts][gid]++;
            }
        }

        /* Quick check for any violations */
        for (int i = 0; i < data->num_events && !has_violations; i++) {
            int start_ts = ind->genes[i].timeslot;
            int room_idx = ind->genes[i].room_id - 1;
            int tid = data->events[i].teacher_id - 1;
            int gid = data->events[i].group_id - 1;
            int dur = data->events[i].duration_slots;
            int start_day = TIMESLOT_TO_DAY(start_ts);
            int start_period = TIMESLOT_TO_PERIOD(start_ts);

            if (data->events[i].requires_lab && !data->rooms[room_idx].is_lab)
                has_violations = 1;
            if (start_period + dur > PERIODS_PER_DAY)
                has_violations = 1;

            for (int s = 0; s < dur && !has_violations; s++) {
                int ts = start_ts + s;
                if (ts >= data->total_timeslots || TIMESLOT_TO_DAY(ts) != start_day)
                    break;
                if (room_occ[ts][room_idx] > 1) has_violations = 1;
                if (teacher_occ[ts][tid] > 1) has_violations = 1;
                if (group_occ[ts][gid] > 1) has_violations = 1;
            }
        }

        if (!has_violations)
            break;

        /* Find first event with a violation */
        int target = -1;
        for (int i = 0; i < data->num_events; i++) {
            int start_ts = ind->genes[i].timeslot;
            int room_idx = ind->genes[i].room_id - 1;
            int tid = data->events[i].teacher_id - 1;
            int gid = data->events[i].group_id - 1;
            int dur = data->events[i].duration_slots;
            int start_day = TIMESLOT_TO_DAY(start_ts);
            int start_period = TIMESLOT_TO_PERIOD(start_ts);
            int viol = 0;

            if (data->events[i].requires_lab && !data->rooms[room_idx].is_lab) viol = 1;
            if (start_period + dur > PERIODS_PER_DAY) viol = 1;
            for (int s = 0; s < dur && !viol; s++) {
                int ts = start_ts + s;
                if (ts >= data->total_timeslots || TIMESLOT_TO_DAY(ts) != start_day) break;
                if (room_occ[ts][room_idx] > 1 ||
                    teacher_occ[ts][tid] > 1 ||
                    group_occ[ts][gid] > 1) viol = 1;
            }
            if (viol) { target = i; break; }
        }

        if (target < 0) break;

        /* Remove target from occupancy */
        {
            int old_ts = ind->genes[target].timeslot;
            int old_rid = ind->genes[target].room_id - 1;
            int old_tid = data->events[target].teacher_id - 1;
            int old_gid = data->events[target].group_id - 1;
            int dur = data->events[target].duration_slots;
            int old_day = TIMESLOT_TO_DAY(old_ts);
            for (int s = 0; s < dur; s++) {
                int ts = old_ts + s;
                if (ts >= data->total_timeslots || TIMESLOT_TO_DAY(ts) != old_day) break;
                room_occ[ts][old_rid]--;
                teacher_occ[ts][old_tid]--;
                group_occ[ts][old_gid]--;
            }
        }

        int best_ts = ind->genes[target].timeslot;
        int best_rid = ind->genes[target].room_id;
        int best_viol = count_single_event_violations(
            target, best_ts, best_rid, data, room_occ, teacher_occ, group_occ);
        int found_free = 0;
        int dur = data->events[target].duration_slots;

        /* Phase 1: Try 20 random (start_slot, room) probes.
         * Collect conflict-free candidates, then pick the one with
         * best soft impact (prefer earlier slots, avoid late periods). */
        int best_soft_score = 999999;  /* lower = better */
        for (int probe = 0; probe < 20; probe++) {
            int try_day = (int)pcg32_boundedrand_r(rng, (uint32_t)NUM_DAYS);
            int max_period = PERIODS_PER_DAY - dur;
            if (max_period < 0) max_period = 0;
            int try_period = (int)pcg32_boundedrand_r(rng, (uint32_t)(max_period + 1));
            int try_ts = DAY_PERIOD_TO_TS(try_day, try_period);
            int try_rid;

            if (data->events[target].requires_lab) {
                if (num_lab_rooms == 0) break;
                int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)num_lab_rooms);
                try_rid = lab_room_ids[idx];
            } else {
                int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)data->num_rooms);
                try_rid = data->rooms[idx].id;
            }

            int viol = count_single_event_violations(
                target, try_ts, try_rid, data, room_occ, teacher_occ, group_occ);
            if (viol < best_viol) {
                best_ts = try_ts; best_rid = try_rid; best_viol = viol;
                if (viol == 0) {
                    /* Soft-aware tiebreaker: late penalty + period preference */
                    int soft_score = try_period * 2;  /* prefer earlier slots */
                    if (try_period + dur - 1 >= PERIODS_PER_DAY - 2)
                        soft_score += 10 * dur;  /* penalize late slots */
                    best_soft_score = soft_score;
                    found_free = 1;
                }
            } else if (viol == 0 && found_free) {
                /* Already have a free slot — compare soft scores */
                int soft_score = try_period * 2;
                if (try_period + dur - 1 >= PERIODS_PER_DAY - 2)
                    soft_score += 10 * dur;
                if (soft_score < best_soft_score) {
                    best_ts = try_ts; best_rid = try_rid;
                    best_soft_score = soft_score;
                }
            }
        }

        /* Phase 2: Systematic scan (only day-valid start slots) */
        if (!found_free) {
            for (int d = 0; d < NUM_DAYS && best_viol > 0; d++) {
                int max_p = PERIODS_PER_DAY - dur;
                for (int p = 0; p <= max_p && best_viol > 0; p++) {
                    int try_ts = DAY_PERIOD_TO_TS(d, p);
                    for (int r = 0; r < data->num_rooms && best_viol > 0; r++) {
                        if (data->events[target].requires_lab && !data->rooms[r].is_lab)
                            continue;
                        int try_rid = data->rooms[r].id;
                        int viol = count_single_event_violations(
                            target, try_ts, try_rid, data,
                            room_occ, teacher_occ, group_occ);
                        if (viol < best_viol) {
                            best_ts = try_ts; best_rid = try_rid; best_viol = viol;
                            if (viol == 0) goto done_scan;
                        }
                    }
                }
            }
            done_scan: ;
        }

        ind->genes[target].timeslot = best_ts;
        ind->genes[target].room_id = best_rid;
    }

    /* Re-evaluate full fitness after all repair attempts */
    evaluate_fitness(ind, data);
}

/* ---------------------------------------------------------------------------
 * local_search -- Hill climbing on soft constraints (post-GA refinement).
 *
 * Assumes hard violations == 0. For each iteration:
 *   1. Pick a random event
 *   2. Save current assignment
 *   3. Try several random (day, period, room) reassignments
 *   4. For each: check hard==0, then compare soft
 *   5. Keep the best improvement found, or revert
 *
 * This is a pure soft optimizer — it never accepts moves that create
 * hard violations.
 * --------------------------------------------------------------------------- */
void local_search(Individual *ind, const ProblemData *data,
                  int max_iterations, int patience, pcg32_random_t *rng)
{
    if (ind->fitness_hard != 0) return;  /* only works on feasible solutions */

    int lab_room_ids[MAX_ROOMS];
    int num_lab_rooms = 0;
    for (int r = 0; r < data->num_rooms; r++) {
        if (data->rooms[r].is_lab)
            lab_room_ids[num_lab_rooms++] = data->rooms[r].id;
    }

    int no_improve = 0;
    int best_soft = ind->fitness_soft;

    for (int iter = 0; iter < max_iterations && no_improve < patience; iter++) {
        /* Pick random event */
        int ev = (int)pcg32_boundedrand_r(rng, (uint32_t)data->num_events);
        int dur = data->events[ev].duration_slots;

        /* Save current assignment */
        int old_ts = ind->genes[ev].timeslot;
        int old_rid = ind->genes[ev].room_id;

        int improved = 0;

        /* Try 15 random reassignments */
        for (int probe = 0; probe < 15; probe++) {
            int try_day = (int)pcg32_boundedrand_r(rng, (uint32_t)NUM_DAYS);
            int max_p = PERIODS_PER_DAY - dur;
            if (max_p < 0) max_p = 0;
            int try_period = (int)pcg32_boundedrand_r(rng, (uint32_t)(max_p + 1));
            int try_ts = DAY_PERIOD_TO_TS(try_day, try_period);
            int try_rid;

            if (data->events[ev].requires_lab && num_lab_rooms > 0) {
                int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)num_lab_rooms);
                try_rid = lab_room_ids[idx];
            } else {
                int idx = (int)pcg32_boundedrand_r(rng, (uint32_t)data->num_rooms);
                try_rid = data->rooms[idx].id;
            }

            /* Apply tentative move */
            ind->genes[ev].timeslot = try_ts;
            ind->genes[ev].room_id = try_rid;

            /* Quick hard check */
            int hard = evaluate_hard(ind, data);
            if (hard > 0) {
                /* Revert — hard violation */
                ind->genes[ev].timeslot = old_ts;
                ind->genes[ev].room_id = old_rid;
                continue;
            }

            /* Check soft improvement */
            int soft = evaluate_soft(ind, data);
            if (soft < best_soft) {
                /* Accept: keep the new assignment */
                best_soft = soft;
                old_ts = try_ts;
                old_rid = try_rid;
                improved = 1;
            } else {
                /* Revert */
                ind->genes[ev].timeslot = old_ts;
                ind->genes[ev].room_id = old_rid;
            }
        }

        if (improved) {
            no_improve = 0;
        } else {
            no_improve++;
        }
    }

    /* Final evaluation */
    evaluate_fitness(ind, data);
}
