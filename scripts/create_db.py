#!/usr/bin/env python3
"""
create_db.py -- Create SQLite database from CSV data.

Consolidates all CSV files into a single SQLite database for:
- Easier querying
- Better data integrity
- Single source of truth for all visualizations
- API backend

Usage:
    python3 scripts/create_db.py data/unitime_v2_nogrupa/ results_v3/schedule_v5_bldg.csv -o timetable.db
"""

import csv
import sqlite3
import os
import argparse
import json


def main():
    parser = argparse.ArgumentParser(description='Create SQLite database from CSV data')
    parser.add_argument('data_dir', help='Directory with CSV files')
    parser.add_argument('schedule', nargs='?', default='', help='GA schedule CSV')
    parser.add_argument('-o', '--output', default='timetable.db')
    args = parser.parse_args()

    if os.path.exists(args.output):
        os.remove(args.output)

    conn = sqlite3.connect(args.output)
    c = conn.cursor()

    # Create tables
    c.executescript('''
        CREATE TABLE rooms (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            capacity INTEGER NOT NULL,
            is_lab INTEGER NOT NULL DEFAULT 0,
            building TEXT,
            building_id INTEGER
        );

        CREATE TABLE teachers (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL
        );

        CREATE TABLE groups (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            num_students INTEGER NOT NULL DEFAULT 30,
            program TEXT,
            year TEXT
        );

        CREATE TABLE courses (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            teacher_id INTEGER REFERENCES teachers(id),
            group_id INTEGER REFERENCES groups(id),
            hours_per_week INTEGER DEFAULT 1,
            requires_lab INTEGER DEFAULT 0,
            duration_slots INTEGER DEFAULT 1
        );

        CREATE TABLE original_schedule (
            event_id INTEGER PRIMARY KEY,
            day_index INTEGER NOT NULL,
            start_slot INTEGER NOT NULL,
            duration_slots INTEGER NOT NULL,
            room_name TEXT,
            teacher_name TEXT,
            group_name TEXT,
            course_name TEXT,
            event_type TEXT,
            program TEXT
        );

        CREATE TABLE ga_schedule (
            event_id INTEGER,
            course_id INTEGER,
            timeslot INTEGER NOT NULL,
            day INTEGER NOT NULL,
            period INTEGER NOT NULL,
            room_id INTEGER REFERENCES rooms(id),
            duration_slots INTEGER NOT NULL,
            teacher_id INTEGER REFERENCES teachers(id),
            group_id INTEGER REFERENCES groups(id),
            requires_lab INTEGER DEFAULT 0
        );

        CREATE TABLE programs (
            program TEXT PRIMARY KEY,
            num_events INTEGER,
            num_groups INTEGER,
            group_ids TEXT
        );

        CREATE TABLE benchmark_runs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            dataset TEXT NOT NULL,
            n_procs INTEGER NOT NULL,
            run_num INTEGER NOT NULL,
            time_sec REAL,
            fitness INTEGER,
            hard_violations INTEGER,
            soft_violations INTEGER
        );
    ''')

    # Load rooms
    rooms_path = os.path.join(args.data_dir, 'rooms.csv')
    with open(rooms_path, encoding='utf-8') as f:
        for row in csv.DictReader(f):
            name = row['name']
            building = name.split()[0] if name.split() else '?'
            bld_id = {'D-10': 0, 'D-11': 1, 'D-7': 2}.get(building, 3)
            c.execute('INSERT INTO rooms VALUES (?,?,?,?,?,?)',
                      (int(row['id']), name, int(row['capacity']),
                       int(row['is_lab']), building, bld_id))

    # Load teachers
    with open(os.path.join(args.data_dir, 'teachers.csv'), encoding='utf-8') as f:
        for row in csv.DictReader(f):
            c.execute('INSERT INTO teachers VALUES (?,?)',
                      (int(row['id']), row['name']))

    # Load groups
    with open(os.path.join(args.data_dir, 'groups.csv'), encoding='utf-8') as f:
        for row in csv.DictReader(f):
            name = row['name']
            # Extract program and year
            program = '?'
            year = '?'
            for p in ['PIS-1', 'PIS-2', 'PFT-1', 'PFT-2', 'PFM-1', 'PFM-2',
                       'PMB-1', 'PMB-2', 'PNM-1', 'PCP-1', 'ESA', 'OB', 'EXT', 'HES']:
                if p in name:
                    program = p
                    break
            for y in ['1rok', '2rok', '3rok', '4rok']:
                if y in name.lower():
                    year = y
                    break

            c.execute('INSERT INTO groups VALUES (?,?,?,?,?)',
                      (int(row['id']), name, int(row['num_students']),
                       program, year))

    # Load courses
    with open(os.path.join(args.data_dir, 'courses.csv'), encoding='utf-8') as f:
        for row in csv.DictReader(f):
            c.execute('INSERT INTO courses VALUES (?,?,?,?,?,?,?)',
                      (int(row['id']), row['name'], int(row['teacher_id']),
                       int(row['group_id']), int(row['hours_per_week']),
                       int(row['requires_lab']), int(row['duration_slots'])))

    # Load original schedule
    orig_path = os.path.join(args.data_dir, 'original_schedule.csv')
    if os.path.exists(orig_path):
        with open(orig_path, encoding='utf-8') as f:
            for row in csv.DictReader(f):
                c.execute('INSERT INTO original_schedule VALUES (?,?,?,?,?,?,?,?,?,?)',
                          (int(row['event_id']), int(row['day_index']),
                           int(row['start_slot']), int(row['duration_slots']),
                           row['room_name'], row['teacher_name'],
                           row['group_name'], row['course_name'],
                           row['type'], row['program']))

    # Load GA schedule
    if args.schedule and os.path.exists(args.schedule):
        with open(args.schedule, encoding='utf-8') as f:
            for row in csv.DictReader(f):
                c.execute('INSERT INTO ga_schedule VALUES (?,?,?,?,?,?,?,?,?,?)',
                          (int(row['event_id']), int(row['course_id']),
                           int(row['timeslot']), int(row['day']),
                           int(row['period']), int(row['room_id']),
                           int(row['duration_slots']), int(row['teacher_id']),
                           int(row['group_id']), int(row['requires_lab'])))

    # Load programs
    progs_path = os.path.join(args.data_dir, 'programs.csv')
    if os.path.exists(progs_path):
        with open(progs_path, encoding='utf-8') as f:
            for row in csv.DictReader(f):
                c.execute('INSERT INTO programs VALUES (?,?,?,?)',
                          (row['program'], int(row['num_events']),
                           int(row['num_groups']), row['group_ids']))

    # Load benchmark results
    results_dir = os.path.dirname(args.schedule) if args.schedule else 'results_v3'
    if os.path.isdir(results_dir):
        for fname in os.listdir(results_dir):
            if fname.startswith('unitime') and fname.endswith('.csv'):
                ds = fname.replace('.csv', '')
                path = os.path.join(results_dir, fname)
                with open(path, encoding='utf-8') as f:
                    for row in csv.DictReader(f):
                        try:
                            c.execute('INSERT INTO benchmark_runs '
                                      '(dataset, n_procs, run_num, time_sec, '
                                      'fitness, hard_violations, soft_violations) '
                                      'VALUES (?,?,?,?,?,?,?)',
                                      (ds, int(row['n_procs']), int(row['run']),
                                       float(row['time_sec']), int(row['fitness']),
                                       int(row['hard_violations']),
                                       int(row['soft_violations'])))
                        except (ValueError, KeyError):
                            pass

    # Create useful views
    c.executescript('''
        CREATE VIEW v_ga_full AS
        SELECT
            gs.event_id, gs.day, gs.period, gs.duration_slots,
            r.name as room_name, r.capacity as room_capacity,
            r.building, r.is_lab as room_type,
            t.name as teacher_name,
            g.name as group_name, g.program, g.year,
            c.name as course_name, c.requires_lab
        FROM ga_schedule gs
        JOIN rooms r ON gs.room_id = r.id
        JOIN teachers t ON gs.teacher_id = t.id
        JOIN groups g ON gs.group_id = g.id
        JOIN courses c ON gs.course_id = c.id;

        CREATE VIEW v_room_utilization AS
        SELECT
            r.name, r.building, r.capacity, r.is_lab,
            COUNT(gs.event_id) as num_events,
            SUM(gs.duration_slots) as total_slots,
            ROUND(SUM(gs.duration_slots) * 100.0 / 70, 1) as utilization_pct
        FROM rooms r
        LEFT JOIN ga_schedule gs ON gs.room_id = r.id
        GROUP BY r.id
        ORDER BY total_slots DESC;

        CREATE VIEW v_teacher_workload AS
        SELECT
            t.name,
            COUNT(gs.event_id) as num_events,
            SUM(gs.duration_slots) as total_slots,
            COUNT(DISTINCT gs.day) as num_days,
            ROUND(SUM(gs.duration_slots) * 45.0 / 60, 1) as hours_per_week
        FROM teachers t
        LEFT JOIN ga_schedule gs ON gs.teacher_id = t.id
        GROUP BY t.id
        ORDER BY total_slots DESC;

        CREATE VIEW v_group_free_days AS
        SELECT
            g.name, g.program, g.year,
            COUNT(DISTINCT gs.day) as days_used,
            5 - COUNT(DISTINCT gs.day) as free_days,
            COUNT(gs.event_id) as num_events
        FROM groups g
        LEFT JOIN ga_schedule gs ON gs.group_id = g.id
        GROUP BY g.id
        ORDER BY g.program, g.year, g.name;

        CREATE VIEW v_benchmark_summary AS
        SELECT
            dataset,
            n_procs,
            COUNT(*) as runs,
            ROUND(AVG(time_sec), 3) as avg_time,
            ROUND(AVG(soft_violations), 0) as avg_soft,
            MAX(hard_violations) as max_hard
        FROM benchmark_runs
        GROUP BY dataset, n_procs
        ORDER BY dataset, n_procs;
    ''')

    conn.commit()

    # Print summary
    for table in ['rooms', 'teachers', 'groups', 'courses',
                   'original_schedule', 'ga_schedule', 'benchmark_runs']:
        count = c.execute(f'SELECT COUNT(*) FROM {table}').fetchone()[0]
        print(f'  {table}: {count} rows')

    db_size = os.path.getsize(args.output)
    print(f'\nDatabase: {args.output} ({db_size // 1024} KB)')
    print('Views: v_ga_full, v_room_utilization, v_teacher_workload, '
          'v_group_free_days, v_benchmark_summary')

    conn.close()


if __name__ == '__main__':
    main()
