#!/bin/bash
# generate_schedules.sh -- Generate GA schedules for all dataset/process combinations.
#
# Run on taurus after compiling on cluster node:
#   ssh stud204-01 'cd ~/Documents/IS/StopienII/SRiR/project && source /opt/nfs/config/source_mpich500.sh && make'
#   cd ~/Documents/IS/StopienII/SRiR/project
#   source /opt/nfs/config/source_mpich500.sh
#   source /opt/nfs/config/source_cuda121.sh
#   export MPIR_CVAR_ENABLE_GPU=0
#   bash scripts/generate_schedules.sh
#
# Uses mpiexec -f nodes (same pattern as benchmark.sh).
# Produces results_v3/schedule_{dataset}_p{procs}.csv for each combo.
# Existing files are skipped (delete to regenerate).

set -euo pipefail

DATASETS="simple_n100 simple_n200 simple_n400 simple"
PROCS_LIST="1 4 8 16"
RESULTS_DIR="results_v3"
BINARY="./timetable_ga"

mkdir -p "$RESULTS_DIR"

if [ ! -f "$BINARY" ]; then
    echo "Binary not found: $BINARY"
    echo "Compile first on cluster node (stud204-01)."
    exit 1
fi

if [ ! -f nodes ]; then
    echo "nodes file not found — needed for mpiexec -f nodes"
    exit 1
fi

TOTAL=0
DONE=0

for ds in $DATASETS; do
    data_dir="data/${ds}/"
    if [ ! -d "$data_dir" ]; then
        echo "SKIP: $data_dir not found"
        continue
    fi

    for np in $PROCS_LIST; do
        outfile="${RESULTS_DIR}/schedule_${ds}_p${np}.csv"
        TOTAL=$((TOTAL + 1))

        if [ -f "$outfile" ]; then
            echo "EXISTS: $outfile (skip)"
            DONE=$((DONE + 1))
            continue
        fi

        echo ""
        echo "=== [$((DONE+1))/$TOTAL] dataset=$ds procs=$np ==="

        OUTPUT=$(mpiexec -f nodes -n "$np" $BINARY "$data_dir" 2>&1) || true

        # Show key results
        echo "$OUTPUT" | grep -E "(Hard violations|Soft violations|Total fitness|Wall-clock)" || true

        # NFS sync — wait for schedule.csv to appear (may be delayed from remote node)
        FOUND=0
        for try in 1 2 3 4 5; do
            if [ -f "schedule.csv" ]; then
                FOUND=1
                break
            fi
            sleep 2
        done

        # Move output to results with proper naming
        if [ "$FOUND" -eq 1 ]; then
            mv schedule.csv "$outfile"
            LINES=$(wc -l < "$outfile")
            echo "  -> $outfile ($LINES lines)"
            DONE=$((DONE + 1))
        else
            echo "  WARNING: schedule.csv not found after 10s wait"
        fi

        # Clean up per-run files
        rm -f timetable.txt convergence_rank*.csv
    done
done

echo ""
echo "========================================"
echo " Done: $DONE/$TOTAL schedules generated"
echo "========================================"
ls -la "$RESULTS_DIR"/schedule_*.csv 2>/dev/null || echo "  (none)"
