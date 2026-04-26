# speedup_v3.gp -- Speedup analysis with REAL data from results_v3/
# Source: averages computed from results_v3/unitime_v2_{n100,n200,n400,nogrupa}.csv
# Run: gnuplot plots/speedup_v3.gp -> plots/speedup_v3.png

set terminal pngcairo enhanced font 'Arial,13' size 900,600
set output 'plots/speedup_v3.png'

set title "Speedup vs MPI Processes (Island Model GA, AGH cluster stud204-*)" font 'Arial,15'
set xlabel "MPI Processes"
set ylabel "Speedup S(p) = T(1)/T(p)"
set grid
set key top left

set xtics (1,4,8,16)
set xrange [0.5:18]
set yrange [0:24]

# Ideal linear reference
set style line 1 lc rgb '#cccccc' dt 2 lw 2

# Verified data (3 runs per config, average of T(1)/T(p)):
#   n100   from results_v3/unitime_v2_n100.csv
#   n200   from results_v3/unitime_v2_n200.csv
#   n400   from results_v3/unitime_v2_n400.csv
#   n633   from results_v3/unitime_v2_nogrupa.csv (full data/simple/)
$DATA << EOD
1   1.00  1.00  1.00  1.00
4   3.63  5.09  7.20  4.62
8   5.38  8.58 14.96  9.24
16  6.32 11.18 22.38 14.44
EOD

plot x title 'Ideal linear' ls 1, \
     $DATA using 1:2 with linespoints pt 7  ps 1.5 lw 2 lc rgb '#2196F3' title '100 events', \
     $DATA using 1:3 with linespoints pt 9  ps 1.5 lw 2 lc rgb '#4CAF50' title '200 events', \
     $DATA using 1:4 with linespoints pt 5  ps 1.5 lw 2 lc rgb '#FF9800' title '400 events (super-linear S_{16}=22.38x)', \
     $DATA using 1:5 with linespoints pt 13 ps 1.5 lw 2 lc rgb '#F44336' title '633 events (full)'
