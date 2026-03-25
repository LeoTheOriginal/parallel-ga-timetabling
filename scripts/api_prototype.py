#!/usr/bin/env python3
"""
api_prototype.py -- REST API for GA-UniTime side-by-side comparison.

Serves original UniTime and GA-optimized schedules for multiple datasets.
Auto-discovers datasets (data/*/) and GA schedules (results_v3/*.csv).

Usage:
    cd project && python3 scripts/api_prototype.py --port 8080
    Then open http://localhost:8080
"""

import json
import csv
import os
import re
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from collections import defaultdict
import argparse

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_BASE = os.path.join(BASE_DIR, 'data')
RESULTS_DIR = os.path.join(BASE_DIR, 'results_v3')

DATASETS = {}


def load_dataset(name, data_dir):
    """Load a single dataset (rooms, teachers, groups, courses, original schedule)."""
    ds = {
        'rooms': {},
        'teachers': {},
        'groups': {},
        'courses': {},
        'original': [],
        'ga_schedules': {},
    }

    for fname, key, transform in [
        ('rooms.csv', 'rooms', lambda r: {
            'id': int(r['id']), 'name': r['name'],
            'capacity': int(r['capacity']), 'is_lab': int(r['is_lab']),
        }),
        ('teachers.csv', 'teachers', lambda r: {
            'id': int(r['id']), 'name': r['name'],
        }),
        ('groups.csv', 'groups', lambda r: {
            'id': int(r['id']), 'name': r['name'],
            'num_students': int(r['num_students']),
        }),
        ('courses.csv', 'courses', lambda r: {
            'id': int(r['id']), 'name': r['name'],
            'teacher_id': int(r['teacher_id']),
            'group_id': int(r['group_id']),
            'duration_slots': int(r['duration_slots']),
            'requires_lab': int(r['requires_lab']),
        }),
    ]:
        path = os.path.join(data_dir, fname)
        if os.path.exists(path):
            with open(path, encoding='utf-8') as f:
                for row in csv.DictReader(f):
                    item = transform(row)
                    ds[key][item['id']] = item

    orig_path = os.path.join(data_dir, 'original_schedule.csv')
    if os.path.exists(orig_path):
        with open(orig_path, encoding='utf-8') as f:
            for row in csv.DictReader(f):
                ds['original'].append({
                    'event_id': int(row['event_id']),
                    'day': int(row['day']),
                    'period': int(row['period']),
                    'room_id': int(row['room_id']),
                    'teacher_id': int(row['teacher_id']),
                    'group_id': int(row['group_id']),
                    'duration_slots': int(row['duration_slots']),
                    'course_name': row.get('course_name', ''),
                    'type': row.get('type', ''),
                })

    return ds


def discover_ga_schedules(dataset_name, ds):
    """Find GA schedule files for a dataset in results_v3/."""
    if not os.path.isdir(RESULTS_DIR):
        return
    for f in os.listdir(RESULTS_DIR):
        # Match: schedule_{dataset}_n{procs}.csv or schedule_{dataset}_p{procs}.csv
        m = re.match(
            rf'^schedule_{re.escape(dataset_name)}_[np](\d+)\.csv$', f
        )
        if not m:
            continue
        procs = int(m.group(1))
        events = []
        with open(os.path.join(RESULTS_DIR, f), encoding='utf-8') as fh:
            for row in csv.DictReader(fh):
                cid = int(row['course_id'])
                course = ds['courses'].get(cid, {})
                events.append({
                    'event_id': int(row['event_id']),
                    'day': int(row['day']),
                    'period': int(row['period']),
                    'room_id': int(row['room_id']),
                    'teacher_id': int(row['teacher_id']),
                    'group_id': int(row['group_id']),
                    'duration_slots': int(row['duration_slots']),
                    'course_name': course.get('name', f"Event {row['event_id']}"),
                    'type': '',
                })
        ds['ga_schedules'][procs] = events


def load_all():
    """Auto-discover and load all datasets."""
    if not os.path.isdir(DATA_BASE):
        print(f"Warning: {DATA_BASE} not found")
        return
    for entry in sorted(os.listdir(DATA_BASE)):
        data_dir = os.path.join(DATA_BASE, entry)
        if not os.path.isdir(data_dir):
            continue
        if not os.path.exists(os.path.join(data_dir, 'courses.csv')):
            continue
        ds = load_dataset(entry, data_dir)
        discover_ga_schedules(entry, ds)
        ga_keys = sorted(ds['ga_schedules'].keys())
        print(f"  {entry}: {len(ds['courses'])} courses, {len(ds['rooms'])} rooms, "
              f"{len(ds['original'])} orig" +
              (f", GA: {ga_keys}" if ga_keys else ""))
        DATASETS[entry] = ds


def compute_stats(schedule):
    """Compute collision statistics for a schedule."""
    if not schedule:
        return {'total_events': 0, 'room_collisions': 0,
                'teacher_collisions': 0, 'group_collisions': 0}
    room_occ = defaultdict(int)
    teacher_occ = defaultdict(int)
    group_occ = defaultdict(int)
    for e in schedule:
        for s in range(e['duration_slots']):
            slot = e['period'] + s
            room_occ[(e['room_id'], e['day'], slot)] += 1
            teacher_occ[(e['teacher_id'], e['day'], slot)] += 1
            group_occ[(e['group_id'], e['day'], slot)] += 1
    return {
        'total_events': len(schedule),
        'room_collisions': sum(1 for v in room_occ.values() if v > 1),
        'teacher_collisions': sum(1 for v in teacher_occ.values() if v > 1),
        'group_collisions': sum(1 for v in group_occ.values() if v > 1),
    }


class APIHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        params = parse_qs(parsed.query)

        def p(name, default=None):
            return params.get(name, [default])[0]

        if path in ('/', '/index.html'):
            try:
                with open(os.path.join(BASE_DIR, 'webapp.html'), 'rb') as f:
                    self.send_response(200)
                    self.send_header('Content-Type', 'text/html; charset=utf-8')
                    self.end_headers()
                    self.wfile.write(f.read())
            except FileNotFoundError:
                self.send_error(404, 'webapp.html not found')
            return

        if path == '/api/datasets':
            result = []
            for name, ds in sorted(DATASETS.items()):
                result.append({
                    'name': name,
                    'events': len(ds['courses']),
                    'rooms': len(ds['rooms']),
                    'teachers': len(ds['teachers']),
                    'groups': len(ds['groups']),
                    'has_original': len(ds['original']) > 0,
                    'ga_procs': sorted(ds['ga_schedules'].keys()),
                })
            self.send_json(result)
            return

        ds_name = p('ds', 'simple')
        ds = DATASETS.get(ds_name)
        if not ds:
            self.send_json({'error': f'Unknown dataset: {ds_name}'})
            return

        if path == '/api/rooms':
            self.send_json(list(ds['rooms'].values()))
        elif path == '/api/teachers':
            self.send_json(list(ds['teachers'].values()))
        elif path == '/api/groups':
            self.send_json(list(ds['groups'].values()))
        elif path == '/api/schedule':
            source = p('source', 'original')
            if source == 'original':
                self.send_json(ds['original'])
            elif source == 'ga':
                procs = int(p('procs', '16'))
                self.send_json(ds['ga_schedules'].get(procs, []))
            else:
                self.send_json([])
        elif path == '/api/stats':
            source = p('source', 'original')
            if source == 'original':
                stats = compute_stats(ds['original'])
            else:
                procs = int(p('procs', '16'))
                stats = compute_stats(ds['ga_schedules'].get(procs, []))
            stats['total_rooms'] = len(ds['rooms'])
            stats['total_teachers'] = len(ds['teachers'])
            stats['total_groups'] = len(ds['groups'])
            self.send_json(stats)
        else:
            self.send_error(404, f'Not found: {path}')

    def send_json(self, data):
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data, ensure_ascii=False).encode('utf-8'))

    def log_message(self, fmt, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description='GA-UniTime comparison API')
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()

    print("Loading datasets...")
    load_all()
    if not DATASETS:
        print("No datasets found in data/")
        return

    print(f"\nhttp://localhost:{args.port}")
    server = HTTPServer(('', args.port), APIHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nBye.")


if __name__ == '__main__':
    main()
