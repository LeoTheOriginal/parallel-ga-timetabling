/*
 * io.h -- CSV loading and summary printing for timetabling data.
 *
 * Each load_* function reads a CSV file into the provided array,
 * returns the number of items loaded, or -1 on error.
 */

#ifndef IO_H
#define IO_H

#include "types.h"

/* Load entities from CSV files.
 * Returns number of items read, or -1 on error (file not found, etc.). */
int load_rooms(const char *filename, Room *rooms, int max_rooms);
int load_teachers(const char *filename, Teacher *teachers, int max_teachers);
int load_groups(const char *filename, StudentGroup *groups, int max_groups);
int load_courses(const char *filename, Course *courses, int max_courses);

/* Expand courses into individual events based on hours_per_week.
 * Fills data->events[] and sets data->num_events.
 * Returns total event count, or -1 on overflow (exceeds MAX_EVENTS). */
int expand_events(ProblemData *data);

/* Print a human-readable summary of all loaded entities. */
void print_summary(const ProblemData *data);

/* Write human-readable timetable to file.
 * Outputs one table per student group (rows=periods 1-8, cols=Mon-Fri).
 * Cells contain "CourseName (RoomName)" or "---" if empty.
 * Includes summary footer with violation counts. */
void write_timetable(const char *filename, const Individual *best,
                     const ProblemData *data);

/* Write machine-readable CSV with GA assignments for each event.
 * Columns: event_id,course_id,timeslot,day,period,room_id,duration_slots,
 *          teacher_id,group_id,requires_lab,fitness_hard,fitness_soft */
void write_schedule_csv(const char *filename, const Individual *best,
                        const ProblemData *data);

#endif /* IO_H */
