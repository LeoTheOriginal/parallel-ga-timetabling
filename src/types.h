/*
 * types.h -- Data structures for the university timetabling problem.
 *
 * Defines all entity types (Room, Teacher, StudentGroup, Course, Event)
 * and the ProblemData container that holds the entire loaded dataset.
 */

#ifndef TYPES_H
#define TYPES_H

/* ----- Constants ----- */

#define MAX_NAME_LEN    128
#define MAX_ROOMS       150
#define MAX_TEACHERS    300
#define MAX_GROUPS      1200
#define MAX_COURSES     2500
#define MAX_EVENTS      2500

#define NUM_DAYS        5
#define PERIODS_PER_DAY 8
#define TOTAL_TIMESLOTS (NUM_DAYS * PERIODS_PER_DAY)  /* 40 */

/* ----- Entity structs ----- */

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int capacity;
    int is_lab;          /* 0 = lecture, 1 = lab, 2 = mixed (both) */
    int building_id;     /* 0=D-10, 1=D-11, 2=D-7 (auto-assigned from name) */
} Room;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
} Teacher;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int num_students;
} StudentGroup;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int teacher_id;
    int group_id;
    int hours_per_week;
    int requires_lab;    /* 0 = no, 1 = yes */
    int duration_slots;  /* number of consecutive 45-min slots (1-5) */
    int parent_group_id; /* 0 = none; >0 = program-level group for lecture conflicts */
} Course;

/*
 * An "event" is one schedulable meeting of a course per week.
 * Each event occupies duration_slots consecutive 45-minute periods
 * in the same room. Fields are denormalized from the parent Course
 * for fast access during fitness evaluation.
 */
typedef struct {
    int event_id;
    int course_id;
    int teacher_id;
    int group_id;
    int requires_lab;
    int duration_slots;  /* 1-5: how many consecutive 45-min slots */
    int parent_group_id; /* 0 = none; >0 = check for lecture conflicts with this group */
} Event;

/* ----- Aggregate container ----- */

typedef struct {
    Room rooms[MAX_ROOMS];
    int num_rooms;

    Teacher teachers[MAX_TEACHERS];
    int num_teachers;

    StudentGroup groups[MAX_GROUPS];
    int num_groups;

    Course courses[MAX_COURSES];
    int num_courses;

    Event events[MAX_EVENTS];
    int num_events;

    int num_days;          /* 5 */
    int periods_per_day;   /* 8 (1.5h blocks) */
    int total_timeslots;   /* num_days * periods_per_day = 40 */
} ProblemData;

/* ----- Timeslot conversion macros ----- */

#define TIMESLOT_TO_DAY(ts)    ((ts) / PERIODS_PER_DAY)
#define TIMESLOT_TO_PERIOD(ts) ((ts) % PERIODS_PER_DAY)
#define DAY_PERIOD_TO_TS(d, p) ((d) * PERIODS_PER_DAY + (p))

/* ----- GA data structures ----- */

/* Gene: assignment of one event to a start timeslot and room.
 * The event occupies timeslot..timeslot+duration_slots-1 in room_id. */
typedef struct {
    int timeslot;   /* 0..TOTAL_TIMESLOTS-1 (0..39), start of event */
    int room_id;    /* 1..num_rooms (1-indexed, matches Room.id from CSV) */
} Gene;

/* Individual: one candidate timetable solution */
typedef struct {
    Gene genes[MAX_EVENTS];  /* genes[event_id] = (timeslot, room) assignment */
    int fitness_hard;        /* total hard constraint violations */
    int fitness_soft;        /* total soft constraint penalty */
    int fitness;             /* fitness_hard * 1000 + fitness_soft */
} Individual;

/* GA configuration parameters */
typedef struct {
    int population_size;     /* 200 / N islands */
    int max_generations;     /* 500 */
    int stagnation_limit;    /* 200 */
    int tournament_size;     /* 3 */
    double crossover_rate;   /* 0.8 */
    double mutation_rate;    /* 0.05 per gene */
    int elitism_count;       /* 1 */
    int max_repair_attempts; /* 100 */
    int migration_interval;  /* generations between migrations (default 50) */
    int num_migrants;        /* individuals exchanged per migration (default 1) */
} GAConfig;

#endif /* TYPES_H */
