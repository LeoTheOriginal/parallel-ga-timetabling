#!/bin/bash
# demo.sh -- one-click local demo: convert dataset, build SQLite, start the webapp.
#
# Prerequisite: a UniTime export at data/unitime/events_raw.csv (private; see data/README.md).
# Output: a dark-theme SPA at http://localhost:8080/.

set -e
cd "$(dirname "$0")"

echo "=== GA Timetabling Demo ==="
echo

echo "[1/3] Converting dataset (1.5h-block model)..."
python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple/ --no-grupa 2>&1 | tail -4

echo
echo "[2/3] Building SQLite cache..."
python3 scripts/create_db.py data/simple/ results_v3/schedule_simple_n16.csv -o timetable.db 2>&1

echo
echo "[3/3] Starting webapp..."
echo "=========================================="
echo "  Open: http://localhost:8080/"
echo "  Tabs: Sale | Studenci | Prowadzacy | Statystyki"
echo "  Press Ctrl+C to stop"
echo "=========================================="
echo
python3 scripts/api_prototype.py --port 8080
