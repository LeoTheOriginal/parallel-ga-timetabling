# Input Data

This directory should contain CSV files with timetabling data.
The actual university data is not included in the repository for privacy reasons
(it contains real professor names, room assignments, and student group data).

## Required files

| File | Columns | Description |
|------|---------|-------------|
| `rooms.csv` | id, name, capacity, is_lab | Available rooms |
| `teachers.csv` | id, name | Teaching staff |
| `groups.csv` | id, name, num_students | Student groups |
| `courses.csv` | id, name, teacher_id, group_id, hours_per_week, requires_lab, duration_slots, parent_group_id | Course definitions |

## Generating data from UniTime export

If you have a UniTime `events_raw.csv` export, place it in `data/unitime/` and run:

```bash
python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple/ --no-grupa --stats
```

This filters and converts raw UniTime data into the CSV format expected by the GA solver.

## Subset datasets

To generate smaller datasets for testing/benchmarking:

```bash
for N in 100 200 400; do
    python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple_n${N}/ --no-grupa --subset $N
done
```
