# quality_scaling_v3.gp -- Quality + time vs problem size with REAL data
# Source: results_v3/unitime_v2_{n100,n200,n400,nogrupa}.csv
# Run: gnuplot plots/quality_scaling_v3.gp -> plots/quality_scaling_v3.png

set terminal pngcairo enhanced font 'Arial,13' size 1200,500
set output 'plots/quality_scaling_v3.png'

set multiplot layout 1,2 title "GA Quality + Timing vs Problem Size (real data)" font 'Arial,15'

# --- Left: Soft violations vs events (n=1 vs n=16) ---
set xlabel "Events"
set ylabel "Soft violations (3-run avg)"
set grid
set key top left
unset logscale

# Verified soft viol averages from results_v3/unitime_v2_*.csv:
#   n100: 341 (always equal across all configs)
#   n200: 641 (always equal)
#   n400: 1291 (always equal)
#   n633: 2835.3 (n_procs=1) -> 2818.0 (n_procs=16)  (small drop with parallelism)
$QUALITY << EOD
100  341  341
200  641  641
400 1291 1291
633 2835 2818
EOD

plot $QUALITY using 1:2 with linespoints pt 7 ps 1.5 lw 2 lc rgb '#F44336' title 'n=1 (sequential)', \
     $QUALITY using 1:3 with linespoints pt 9 ps 1.5 lw 2 lc rgb '#4CAF50' title 'n=16 (parallel)'

# --- Right: Time vs events (log scale) ---
set xlabel "Events"
set ylabel "Wall-clock time [s]"
set key top left
set logscale y

# Verified time averages from results_v3/unitime_v2_*.csv:
#   n100: T1=2.313  T16=0.366
#   n200: T1=5.798  T16=0.519
#   n400: T1=24.461 T16=1.093
#   n633: T1=159.958 T16=11.078
$TIMING << EOD
100   2.313  0.366
200   5.798  0.519
400  24.461  1.093
633 159.958 11.078
EOD

plot $TIMING using 1:2 with linespoints pt 7 ps 1.5 lw 2 lc rgb '#F44336' title 'n=1', \
     $TIMING using 1:3 with linespoints pt 9 ps 1.5 lw 2 lc rgb '#4CAF50' title 'n=16'

unset multiplot
