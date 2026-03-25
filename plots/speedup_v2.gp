# speedup_v2.gp -- Speedup analysis for multi-slot GA
# Generates: plots/speedup_v2.png

set terminal pngcairo enhanced font 'Arial,13' size 900,600
set output 'plots/speedup_v2.png'

set title "Speedup vs MPI Processes (Multi-slot GA, Island Model)" font 'Arial,15'
set xlabel "MPI Processes"
set ylabel "Speedup"
set grid
set key top left

set xtics (1,4,8,16)
set xrange [0.5:18]
set yrange [0:32]

# Ideal linear
set style line 1 lc rgb '#cccccc' dt 2 lw 2

# Data from results_v2/speedup_data.csv
$DATA << EOD
1 1.00 1.00 1.00 1.00
4 4.03 6.31 5.15 3.85
8 7.25 13.55 11.61 8.06
16 11.96 24.55 28.73 19.59
EOD

plot x title 'Ideal linear' ls 1, \
     $DATA using 1:2 with linespoints pt 7 ps 1.5 lw 2 lc rgb '#2196F3' title '100 events', \
     $DATA using 1:3 with linespoints pt 9 ps 1.5 lw 2 lc rgb '#4CAF50' title '200 events', \
     $DATA using 1:4 with linespoints pt 5 ps 1.5 lw 2 lc rgb '#FF9800' title '400 events', \
     $DATA using 1:5 with linespoints pt 13 ps 1.5 lw 2 lc rgb '#F44336' title '1058 events'
