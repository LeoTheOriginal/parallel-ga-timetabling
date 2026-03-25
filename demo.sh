#!/bin/bash
# demo.sh -- One-click demo for presentation
#
# Runs: data conversion → GA optimization → webapp visualization
#
# Usage:
#   bash demo.sh           (local: convert data + start webapp)
#   bash demo.sh cluster   (cluster: compile + run GA + download results)

set -e
cd "$(dirname "$0")"

echo "=== GA-UniTime Demo ==="
echo ""

if [ "$1" = "cluster" ]; then
    echo "[1/4] Compiling on cluster..."
    ssh 2piotrowski@taurus.fis.agh.edu.pl "ssh stud204-01 'cd /home/stud2022/2piotrowski/Documents/IS/StopienII/SRiR/project && source /opt/nfs/config/source_mpich500.sh && source /opt/nfs/config/source_cuda121.sh && export MPIR_CVAR_ENABLE_GPU=0 && make clean && make 2>&1 | tail -1'"

    echo "[2/4] Generating data on cluster..."
    ssh 2piotrowski@taurus.fis.agh.edu.pl "cd /home/stud2022/2piotrowski/Documents/IS/StopienII/SRiR/project && python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple/ --no-grupa 2>&1 | tail -4"

    echo "[3/4] Running GA (n=16)..."
    ssh 2piotrowski@taurus.fis.agh.edu.pl "ssh stud204-01 'cd /home/stud2022/2piotrowski/Documents/IS/StopienII/SRiR/project && source /opt/nfs/config/source_mpich500.sh && source /opt/nfs/config/source_cuda121.sh && export MPIR_CVAR_ENABLE_GPU=0 && /opt/nfs/config/station204_name_list.sh 1 16 > nodes_alive && mpiexec -f nodes_alive -n 16 ./timetable_ga data/simple/ 2>&1 | grep -E \"(Hard|Soft|Total|Wall|Events|Rooms)\"'"

    echo "[4/4] Downloading results..."
    scp 2piotrowski@taurus.fis.agh.edu.pl:/home/stud2022/2piotrowski/Documents/IS/StopienII/SRiR/project/schedule.csv results_v3/schedule_simple_n16.csv 2>/dev/null
    echo "Results saved to results_v3/schedule_simple_n16.csv"

else
    echo "[1/3] Converting UniTime data (8-block model)..."
    python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple/ --no-grupa 2>&1 | tail -4

    echo ""
    echo "[2/3] Creating SQLite database..."
    python3 scripts/create_db.py data/simple/ results_v3/schedule_simple_n16.csv -o timetable.db 2>&1

    echo ""
    echo "[3/3] Starting webapp..."
    echo ""
    echo "=========================================="
    echo "  Open: http://localhost:8080/"
    echo "  Tabs: Sale | Studenci | Prowadzacy | Statystyki"
    echo "  Press Ctrl+C to stop"
    echo "=========================================="
    echo ""
    python3 scripts/api_prototype.py --port 8080
fi
