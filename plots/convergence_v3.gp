# convergence_v3.gp -- Multi-island convergence (16 islands, real run)
# Reads convergence_rank*.csv files produced by the binary itself.
# Run AFTER executing: mpiexec -f nodes -n 16 ./timetable_ga data/simple/
# (this produces convergence_rank0.csv .. convergence_rank15.csv in CWD)
# Then: gnuplot plots/convergence_v3.gp -> plots/convergence_v3.png

set terminal pngcairo enhanced font 'Arial,13' size 1200,500
set output 'plots/convergence_v3.png'
set datafile separator ","

set multiplot layout 1,2 title "Convergence per island (16 islands, real run on data/simple/, 633 events)" font 'Arial,14'

# CSV columns produced by ga.c:
# generation,best_hard,best_soft,best_fitness,avg_fitness,worst_fitness

# --- Left: Hard violations convergence ---
set xlabel "Generation"
set ylabel "Hard violations (best on island)"
set grid
set key top right
set yrange [-2:*]

plot 'convergence_rank0.csv'  using 1:2 every 5 with lines lw 1.5 lc rgb '#F44336' title 'Island 0', \
     'convergence_rank4.csv'  using 1:2 every 5 with lines lw 1.5 lc rgb '#4CAF50' title 'Island 4', \
     'convergence_rank8.csv'  using 1:2 every 5 with lines lw 1.5 lc rgb '#2196F3' title 'Island 8', \
     'convergence_rank12.csv' using 1:2 every 5 with lines lw 1.5 lc rgb '#FF9800' title 'Island 12'

# --- Right: Soft violations convergence (po hard=0) ---
set xlabel "Generation"
set ylabel "Soft violations (best, only when hard=0)"
set yrange [*:*]
set key top right

plot 'convergence_rank0.csv'  using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#F44336' title 'Island 0', \
     'convergence_rank4.csv'  using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#4CAF50' title 'Island 4', \
     'convergence_rank8.csv'  using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#2196F3' title 'Island 8', \
     'convergence_rank12.csv' using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#FF9800' title 'Island 12'

unset multiplot
