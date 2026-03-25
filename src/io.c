/*
 * io.c -- CSV parsing and summary printing for timetabling data.
 *
 * Uses fgets + strtok pattern for robust CSV parsing.
 * Each CSV file has a header row (skipped) and comma-separated fields.
 * No quoted fields or commas in values.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "io.h"

#define MAX_LINE 256

/* ------------------------------------------------------------------ */
/*  load_rooms: id,name,capacity,is_lab                               */
/* ------------------------------------------------------------------ */

int load_rooms(const char *filename, Room *rooms, int max_rooms)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return -1;
    }

    char line[MAX_LINE];
    int count = 0;
    int line_num = 1;

    /* Skip header row */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL && count < max_rooms) {
        line_num++;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') continue;

        char *token;
        int fields = 0;

        /* Field 1: id */
        token = strtok(line, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing id\n", filename, line_num); continue; }
        rooms[count].id = atoi(token);
        fields++;

        /* Field 2: name */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing name\n", filename, line_num); continue; }
        strncpy(rooms[count].name, token, MAX_NAME_LEN - 1);
        rooms[count].name[MAX_NAME_LEN - 1] = '\0';
        fields++;

        /* Field 3: capacity */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing capacity\n", filename, line_num); continue; }
        rooms[count].capacity = atoi(token);
        fields++;

        /* Field 4: is_lab */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing is_lab\n", filename, line_num); continue; }
        rooms[count].is_lab = atoi(token);
        fields++;

        /* Auto-assign building_id from room name prefix */
        if (strncmp(rooms[count].name, "D-10", 4) == 0)
            rooms[count].building_id = 0;
        else if (strncmp(rooms[count].name, "D-11", 4) == 0)
            rooms[count].building_id = 1;
        else if (strncmp(rooms[count].name, "D-7", 3) == 0)
            rooms[count].building_id = 2;
        else
            rooms[count].building_id = 3;  /* other */

        count++;
    }

    fclose(fp);
    return count;
}

/* ------------------------------------------------------------------ */
/*  load_teachers: id,name                                            */
/* ------------------------------------------------------------------ */

int load_teachers(const char *filename, Teacher *teachers, int max_teachers)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return -1;
    }

    char line[MAX_LINE];
    int count = 0;
    int line_num = 1;

    /* Skip header row */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL && count < max_teachers) {
        line_num++;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') continue;

        char *token;

        /* Field 1: id */
        token = strtok(line, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing id\n", filename, line_num); continue; }
        teachers[count].id = atoi(token);

        /* Field 2: name */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing name\n", filename, line_num); continue; }
        strncpy(teachers[count].name, token, MAX_NAME_LEN - 1);
        teachers[count].name[MAX_NAME_LEN - 1] = '\0';

        count++;
    }

    fclose(fp);
    return count;
}

/* ------------------------------------------------------------------ */
/*  load_groups: id,name,num_students                                 */
/* ------------------------------------------------------------------ */

int load_groups(const char *filename, StudentGroup *groups, int max_groups)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return -1;
    }

    char line[MAX_LINE];
    int count = 0;
    int line_num = 1;

    /* Skip header row */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL && count < max_groups) {
        line_num++;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') continue;

        char *token;

        /* Field 1: id */
        token = strtok(line, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing id\n", filename, line_num); continue; }
        groups[count].id = atoi(token);

        /* Field 2: name */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing name\n", filename, line_num); continue; }
        strncpy(groups[count].name, token, MAX_NAME_LEN - 1);
        groups[count].name[MAX_NAME_LEN - 1] = '\0';

        /* Field 3: num_students */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing num_students\n", filename, line_num); continue; }
        groups[count].num_students = atoi(token);

        count++;
    }

    fclose(fp);
    return count;
}

/* ------------------------------------------------------------------ */
/*  load_courses: id,name,teacher_id,group_id,hours_per_week,         */
/*                requires_lab                                        */
/* ------------------------------------------------------------------ */

int load_courses(const char *filename, Course *courses, int max_courses)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return -1;
    }

    char line[MAX_LINE];
    int count = 0;
    int line_num = 1;

    /* Skip header row */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL && count < max_courses) {
        line_num++;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') continue;

        char *token;

        /* Field 1: id */
        token = strtok(line, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing id\n", filename, line_num); continue; }
        courses[count].id = atoi(token);

        /* Field 2: name */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing name\n", filename, line_num); continue; }
        strncpy(courses[count].name, token, MAX_NAME_LEN - 1);
        courses[count].name[MAX_NAME_LEN - 1] = '\0';

        /* Field 3: teacher_id */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing teacher_id\n", filename, line_num); continue; }
        courses[count].teacher_id = atoi(token);

        /* Field 4: group_id */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing group_id\n", filename, line_num); continue; }
        courses[count].group_id = atoi(token);

        /* Field 5: hours_per_week */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing hours_per_week\n", filename, line_num); continue; }
        courses[count].hours_per_week = atoi(token);

        /* Field 6: requires_lab */
        token = strtok(NULL, ",");
        if (!token) { fprintf(stderr, "Parse error at %s line %d: missing requires_lab\n", filename, line_num); continue; }
        courses[count].requires_lab = atoi(token);

        /* Field 7: duration_slots (optional, defaults to 1) */
        token = strtok(NULL, ",");
        courses[count].duration_slots = (token && atoi(token) > 0) ? atoi(token) : 1;

        /* Field 8: parent_group_id (optional, defaults to 0) */
        token = strtok(NULL, ",");
        courses[count].parent_group_id = (token) ? atoi(token) : 0;

        count++;
    }

    fclose(fp);
    return count;
}

/* ------------------------------------------------------------------ */
/*  expand_events: expand courses into individual schedulable events   */
/* ------------------------------------------------------------------ */

/* Renumber entity IDs to be contiguous 1..N.
 * This is critical: the fitness functions use id-1 as array index. */
static void renumber_ids(ProblemData *data)
{
    /* Rooms: ensure room[i].id == i+1 */
    for (int i = 0; i < data->num_rooms; i++)
        data->rooms[i].id = i + 1;

    /* Teachers */
    for (int i = 0; i < data->num_teachers; i++)
        data->teachers[i].id = i + 1;

    /* Groups */
    for (int i = 0; i < data->num_groups; i++)
        data->groups[i].id = i + 1;

    /* Courses: remap teacher_id and group_id to match renumbered entities.
     * Since converter already produces sequential IDs, this is a safety net. */
    for (int i = 0; i < data->num_courses; i++)
        data->courses[i].id = i + 1;
}

int expand_events(ProblemData *data)
{
    /* Ensure all IDs are contiguous 1..N before creating events */
    renumber_ids(data);

    int event_count = 0;

    for (int c = 0; c < data->num_courses; c++) {
        for (int h = 0; h < data->courses[c].hours_per_week; h++) {
            if (event_count >= MAX_EVENTS) {
                fprintf(stderr, "Error: too many events (max %d)\n", MAX_EVENTS);
                return -1;
            }
            Event *e = &data->events[event_count];
            e->event_id = event_count;
            e->course_id = data->courses[c].id;
            e->teacher_id = data->courses[c].teacher_id;
            e->group_id = data->courses[c].group_id;
            e->requires_lab = data->courses[c].requires_lab;
            e->duration_slots = data->courses[c].duration_slots;
            e->parent_group_id = data->courses[c].parent_group_id;
            event_count++;
        }
    }

    data->num_events = event_count;
    return event_count;
}

/* ------------------------------------------------------------------ */
/*  print_summary: display counts and sample entries                   */
/* ------------------------------------------------------------------ */

void print_summary(const ProblemData *data)
{
    printf("=== Timetabling Problem Summary ===\n\n");

    printf("Rooms:      %d\n", data->num_rooms);
    printf("Teachers:   %d\n", data->num_teachers);
    printf("Groups:     %d\n", data->num_groups);
    printf("Courses:    %d\n", data->num_courses);
    printf("Events:     %d (expanded from %d courses)\n",
           data->num_events, data->num_courses);
    printf("Timeslots:  %d (%d days x %d periods)\n\n",
           data->total_timeslots, data->num_days, data->periods_per_day);

    /* Sample rooms (up to 3) */
    int n = data->num_rooms < 3 ? data->num_rooms : 3;
    printf("Sample rooms:\n");
    for (int i = 0; i < n; i++) {
        printf("  [%d] %s (capacity=%d, lab=%d)\n",
               data->rooms[i].id, data->rooms[i].name,
               data->rooms[i].capacity, data->rooms[i].is_lab);
    }

    /* Sample teachers (up to 3) */
    n = data->num_teachers < 3 ? data->num_teachers : 3;
    printf("Sample teachers:\n");
    for (int i = 0; i < n; i++) {
        printf("  [%d] %s\n", data->teachers[i].id, data->teachers[i].name);
    }

    /* Sample groups (up to 3) */
    n = data->num_groups < 3 ? data->num_groups : 3;
    printf("Sample groups:\n");
    for (int i = 0; i < n; i++) {
        printf("  [%d] %s (%d students)\n",
               data->groups[i].id, data->groups[i].name,
               data->groups[i].num_students);
    }

    /* Sample courses (up to 3) */
    n = data->num_courses < 3 ? data->num_courses : 3;
    printf("Sample courses:\n");
    for (int i = 0; i < n; i++) {
        printf("  [%d] %s (teacher=%d, group=%d, hours=%d, lab=%d, dur=%d)\n",
               data->courses[i].id, data->courses[i].name,
               data->courses[i].teacher_id, data->courses[i].group_id,
               data->courses[i].hours_per_week, data->courses[i].requires_lab,
               data->courses[i].duration_slots);
    }

    /* Sample events (up to 3) */
    n = data->num_events < 3 ? data->num_events : 3;
    printf("Sample events:\n");
    for (int i = 0; i < n; i++) {
        printf("  event[%d] course=%d teacher=%d group=%d lab=%d\n",
               data->events[i].event_id, data->events[i].course_id,
               data->events[i].teacher_id, data->events[i].group_id,
               data->events[i].requires_lab);
    }

    /* Constraint density summary */
    int num_lab_rooms = 0;
    for (int i = 0; i < data->num_rooms; i++) {
        if (data->rooms[i].is_lab) num_lab_rooms++;
    }
    int num_lab_events = 0;
    for (int i = 0; i < data->num_events; i++) {
        if (data->events[i].requires_lab) num_lab_events++;
    }
    int total_slots = data->total_timeslots * data->num_rooms;
    int lab_slots = data->total_timeslots * num_lab_rooms;
    printf("\nConstraint density:\n");
    printf("  Overall:  %d events / %d slots = %.1f%% fill rate\n",
           data->num_events, total_slots,
           total_slots > 0 ? 100.0 * data->num_events / total_slots : 0.0);
    printf("  Lab only: %d lab events / %d lab slots = %.1f%% fill rate\n",
           num_lab_events, lab_slots,
           lab_slots > 0 ? 100.0 * num_lab_events / lab_slots : 0.0);

    /* Duration distribution */
    int dur_counts[6] = {0};
    int total_event_slots = 0;
    for (int i = 0; i < data->num_events; i++) {
        int d = data->events[i].duration_slots;
        if (d >= 1 && d <= 5) dur_counts[d]++;
        total_event_slots += d;
    }
    printf("  Event-slots: %d (total 45-min slots needed)\n", total_event_slots);
    printf("  Duration distribution:\n");
    for (int d = 1; d <= 5; d++) {
        if (dur_counts[d] > 0)
            printf("    %d-slot (%3dmin): %d events\n",
                   d, d * 45, dur_counts[d]);
    }
}

/* ------------------------------------------------------------------ */
/*  write_timetable: per-group grid output to file                     */
/* ------------------------------------------------------------------ */

void write_timetable(const char *filename, const Individual *best,
                     const ProblemData *data)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s for writing\n", filename);
        return;
    }

    /* Day names for column headers */
    const char *day_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri"};
    /* Slot time labels for 45-min granularity */
    const char *slot_labels[] = {
        " 8:00", " 8:45", " 9:45", "10:30", "11:30", "12:15",
        "13:15", "14:00", "15:00", "15:45", "16:45", "17:30",
        "18:30", "19:15"
    };

    /* For each student group, build and print a grid */
    for (int g = 0; g < data->num_groups; g++) {
        int group_id = data->groups[g].id;  /* 1-indexed */

        fprintf(fp, "=== Timetable for %s ===\n\n", data->groups[g].name);

        /* Build grid: cell[period][day] = string (bigger for long names) */
        char cell[PERIODS_PER_DAY][NUM_DAYS][200];
        for (int p = 0; p < PERIODS_PER_DAY; p++)
            for (int d = 0; d < NUM_DAYS; d++)
                strcpy(cell[p][d], "---");

        /* Fill cells from events assigned to this group */
        int group_event_count = 0;
        for (int i = 0; i < data->num_events; i++) {
            if (data->events[i].group_id != group_id) continue;
            group_event_count++;

            int ts = best->genes[i].timeslot;
            int day = TIMESLOT_TO_DAY(ts);
            int period = TIMESLOT_TO_PERIOD(ts);
            int rid = best->genes[i].room_id;
            int dur = data->events[i].duration_slots;

            /* Look up course name */
            const char *course_name = "???";
            for (int c = 0; c < data->num_courses; c++) {
                if (data->courses[c].id == data->events[i].course_id) {
                    course_name = data->courses[c].name;
                    break;
                }
            }

            /* Look up room name */
            const char *room_name = "???";
            for (int r = 0; r < data->num_rooms; r++) {
                if (data->rooms[r].id == rid) {
                    room_name = data->rooms[r].name;
                    break;
                }
            }

            /* Fill all slots occupied by this multi-slot event */
            for (int s = 0; s < dur && (period + s) < PERIODS_PER_DAY; s++) {
                if (s == 0) {
                    snprintf(cell[period][day], sizeof(cell[period][day]),
                             "%.30s (%.15s) [%dsl]",
                             course_name, room_name, dur);
                } else {
                    snprintf(cell[period + s][day],
                             sizeof(cell[period + s][day]),
                             "  (cont.)");
                }
            }
        }

        /* Print column headers */
        fprintf(fp, "%-6s", "Slot");
        for (int d = 0; d < NUM_DAYS; d++)
            fprintf(fp, "%-36s", day_names[d]);
        fprintf(fp, "\n");

        /* Print rows: 14 slots */
        for (int p = 0; p < PERIODS_PER_DAY; p++) {
            fprintf(fp, "%-6s", slot_labels[p]);
            for (int d = 0; d < NUM_DAYS; d++)
                fprintf(fp, "%-36s", cell[p][d]);
            fprintf(fp, "\n");
        }

        fprintf(fp, "\nTotal events: %d, Hard violations: %d, "
                "Soft violations: %d, Fitness: %d\n\n",
                group_event_count, best->fitness_hard,
                best->fitness_soft, best->fitness);
    }

    fclose(fp);
    printf("Timetable written to %s\n", filename);
}

/* ------------------------------------------------------------------ */
/*  write_schedule_csv: machine-readable GA output for visualization   */
/* ------------------------------------------------------------------ */

void write_schedule_csv(const char *filename, const Individual *best,
                        const ProblemData *data)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s for writing\n", filename);
        return;
    }

    fprintf(fp, "event_id,course_id,timeslot,day,period,room_id,"
                "duration_slots,teacher_id,group_id,requires_lab\n");

    for (int i = 0; i < data->num_events; i++) {
        int ts = best->genes[i].timeslot;
        int day = TIMESLOT_TO_DAY(ts);
        int period = TIMESLOT_TO_PERIOD(ts);
        fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                data->events[i].event_id,
                data->events[i].course_id,
                ts,
                day,
                period,
                best->genes[i].room_id,
                data->events[i].duration_slots,
                data->events[i].teacher_id,
                data->events[i].group_id,
                data->events[i].requires_lab);
    }

    fclose(fp);
    printf("Schedule CSV written to %s\n", filename);
}
