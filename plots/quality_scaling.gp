# quality_scaling.gp -- Quality metrics vs problem size
# Generates: plots/quality_scaling.png

set terminal pngcairo enhanced font 'Arial,13' size 1200,500
set output 'plots/quality_scaling.png'

set multiplot layout 1,2 title "GA Quality Analysis" font 'Arial,15'

# --- Left: Soft violations vs events (n=1 vs n=16) ---
set xlabel "Events"
set ylabel "Soft violations"
set grid
set key top left

$QUALITY << EOD
100 150 150
200 293 292
400 675 645
1058 2061 1976
EOD

plot $QUALITY using 1:2 with linespoints pt 7 ps 1.5 lw 2 lc rgb '#F44336' title 'n=1 (sequential)', \
     $QUALITY using 1:3 with linespoints pt 9 ps 1.5 lw 2 lc rgb '#4CAF50' title 'n=16 (parallel)'

# --- Right: Time vs events ---
set xlabel "Events"
set ylabel "Time (seconds)"
set key top left
set logscale y

$TIMING << EOD
100 3.20 0.27
200 15.03 0.61
400 53.44 1.86
1058 189.94 9.70
EOD

plot $TIMING using 1:2 with linespoints pt 7 ps 1.5 lw 2 lc rgb '#F44336' title 'n=1', \
     $TIMING using 1:3 with linespoints pt 9 ps 1.5 lw 2 lc rgb '#4CAF50' title 'n=16'

unset multiplot
