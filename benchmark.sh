#!/bin/bash
# benchmark.sh -- Automated benchmark runner for the parallel GA timetabling solver.
#
# Runs the GA binary with varying MPI process counts (1, 2, 4, 8, 16),
# collecting wall-clock time, fitness, and hard violation data over multiple
# runs.  Computes mean, std dev, speedup, and speedup error propagation.
#
# Outputs:
#   results/nN.csv         -- per-run data for each process count N
#   results/speedup.csv    -- statistical summary with speedup
#   results/summary.txt    -- formatted table for documentation
#   results/convergence_rankX.csv -- per-island convergence data from n=4 run

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================
PROCS="1 2 4 8 16"
RUNS=5
BINARY="./timetable_ga"
DATA_DIR="data/"
RESULTS_DIR="results"

# ============================================================================
# Setup
# ============================================================================
mkdir -p "$RESULTS_DIR"

echo "========================================"
echo " Benchmark: Parallel GA Timetabling"
echo "========================================"
echo "Configurations: n = $PROCS"
echo "Runs per config: $RUNS"
echo "Binary: $BINARY"
echo "Data directory: $DATA_DIR"
echo "----------------------------------------"

# Ensure binary is built
make

# ============================================================================
# Data Collection
# ============================================================================
for N in $PROCS; do
    echo ""
    echo "=== Configuration: n=$N ==="

    # Create/truncate per-config CSV
    NCSV="$RESULTS_DIR/n${N}.csv"
    echo "run,time,fitness,hard_violations" > "$NCSV"

    for RUN in $(seq 1 $RUNS); do
        # Run the GA binary and capture full output
        # Use -f nodes if nodes file exists (cluster), otherwise localhost
        if [ -f nodes ]; then
            OUTPUT=$(mpiexec -f nodes -n "$N" $BINARY "$DATA_DIR" 2>&1) || true
        else
            OUTPUT=$(mpiexec -n "$N" $BINARY "$DATA_DIR" 2>&1) || true
        fi

        # Extract metrics from rank 0 output
        TIME=$(echo "$OUTPUT" | grep "Wall-clock time:" | awk '{print $(NF-1)}' | head -1)
        FITNESS=$(echo "$OUTPUT" | grep "Total fitness:" | awk '{print $NF}' | head -1)
        HARD=$(echo "$OUTPUT" | grep "Hard violations:" | awk '{print $NF}' | head -1)

        # Fallback values in case parsing fails
        TIME=${TIME:-0.000}
        FITNESS=${FITNESS:-0}
        HARD=${HARD:-0}

        # Remove trailing "seconds" if present
        TIME=$(echo "$TIME" | sed 's/seconds//')

        # Append to per-config CSV
        echo "$RUN,$TIME,$FITNESS,$HARD" >> "$NCSV"

        # Progress output
        echo "  n=$N run=$RUN: ${TIME}s fitness=$FITNESS hard=$HARD"
    done

    # Preserve convergence files from n=4 for convergence plotting
    if [ "$N" = "4" ]; then
        echo "  Preserving convergence files for n=4..."
        for f in convergence_rank*.csv; do
            if [ -f "$f" ]; then
                cp "$f" "$RESULTS_DIR/"
            fi
        done
    fi

    # Clean up convergence files between configurations
    rm -f convergence_rank*.csv
done

echo ""
echo "=== Data collection complete ==="

# ============================================================================
# Statistical Analysis (awk)
# ============================================================================
echo ""
echo "Computing statistics..."

# Create speedup.csv header
echo "n,mean_time,std_time,mean_fitness,std_fitness,speedup,speedup_err" > "$RESULTS_DIR/speedup.csv"

# First pass: compute T1 (mean time for n=1)
T1=$(awk -F',' 'NR>1 { sum += $2; n++ } END { printf "%.6f", sum/n }' "$RESULTS_DIR/n1.csv")
T1_STD=$(awk -F',' -v mean="$T1" 'NR>1 { diff=$2-mean; sumsq+=diff*diff; n++ } END { printf "%.6f", sqrt(sumsq/(n-1)) }' "$RESULTS_DIR/n1.csv")

echo "  T1 (baseline): ${T1}s (std: ${T1_STD}s)"

# Second pass: compute stats for each configuration
for N in $PROCS; do
    NCSV="$RESULTS_DIR/n${N}.csv"

    # Compute mean and std dev of time and fitness
    STATS=$(awk -F',' -v t1="$T1" -v t1_std="$T1_STD" '
    NR > 1 {
        time[NR-1] = $2
        fit[NR-1]  = $3
        sum_t += $2
        sum_f += $3
        n++
    }
    END {
        mean_t = sum_t / n
        mean_f = sum_f / n

        sumsq_t = 0
        sumsq_f = 0
        for (i = 1; i <= n; i++) {
            sumsq_t += (time[i] - mean_t)^2
            sumsq_f += (fit[i] - mean_f)^2
        }
        std_t = (n > 1) ? sqrt(sumsq_t / (n - 1)) : 0
        std_f = (n > 1) ? sqrt(sumsq_f / (n - 1)) : 0

        # Speedup and error propagation
        speedup = t1 / mean_t
        rel_err_1 = (t1 > 0) ? t1_std / t1 : 0
        rel_err_n = (mean_t > 0) ? std_t / mean_t : 0
        speedup_err = speedup * sqrt(rel_err_1^2 + rel_err_n^2)

        printf "%.4f,%.4f,%.1f,%.1f,%.3f,%.3f", mean_t, std_t, mean_f, std_f, speedup, speedup_err
    }
    ' "$NCSV")

    echo "$N,$STATS" >> "$RESULTS_DIR/speedup.csv"
done

echo "  Speedup data written to $RESULTS_DIR/speedup.csv"

# ============================================================================
# Summary Table
# ============================================================================
echo ""
echo "Generating summary table..."

# Generate formatted table from speedup.csv
{
    echo "========================================"
    echo " Benchmark Results Summary"
    echo "========================================"
    echo ""
    printf "| %-9s | %-13s | %-11s | %-7s | %-12s | %-11s |\n" \
        "Processes" "Mean Time (s)" "Std Dev (s)" "Speedup" "Mean Fitness" "Std Fitness"
    printf "|-----------|---------------|-------------|---------|--------------|-------------|\n"

    awk -F',' 'NR > 1 {
        printf "| %-9s | %13.3f | %11.3f | %7.2f | %12.1f | %11.1f |\n", \
            $1, $2, $3, $6, $4, $5
    }' "$RESULTS_DIR/speedup.csv"

    echo ""
    echo "Runs per configuration: $RUNS"
    echo "Binary: $BINARY"
    echo "Data: $DATA_DIR"
} | tee "$RESULTS_DIR/summary.txt"

echo ""
echo "========================================"
echo " Benchmark complete!"
echo " Results: $RESULTS_DIR/speedup.csv"
echo " Summary: $RESULTS_DIR/summary.txt"
echo "========================================"
