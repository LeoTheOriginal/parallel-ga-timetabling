# convergence_v2.gp -- Multi-island convergence (16 islands, 1058 events, 1000 gens)
# Shows per-island best_hard and best_fitness over generations

set terminal pngcairo enhanced font 'Arial,13' size 1200,500
set output 'plots/convergence_v2.png'
set datafile separator ","

set multiplot layout 1,2 title "Convergence: 16 Islands, 1058 Events, 1000 Generations" font 'Arial,15'

# --- Left: Hard violations convergence ---
set xlabel "Generation"
set ylabel "Hard violations (best)"
set grid
set key top right
set yrange [-5:*]

plot 'results_v2/convergence_rank0.csv' using 1:2 every 5 with lines lw 1.5 lc rgb '#F44336' title 'Island 0', \
     'results_v2/convergence_rank4.csv' using 1:2 every 5 with lines lw 1.5 lc rgb '#4CAF50' title 'Island 4', \
     'results_v2/convergence_rank8.csv' using 1:2 every 5 with lines lw 1.5 lc rgb '#2196F3' title 'Island 8', \
     'results_v2/convergence_rank12.csv' using 1:2 every 5 with lines lw 1.5 lc rgb '#FF9800' title 'Island 12'

unset logscale y

# --- Right: Soft violations convergence (after hard=0) ---
set xlabel "Generation"
set ylabel "Soft violations (best)"
set yrange [1800:2800]
set key top right

plot 'results_v2/convergence_rank0.csv' using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#F44336' title 'Island 0', \
     'results_v2/convergence_rank4.csv' using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#4CAF50' title 'Island 4', \
     'results_v2/convergence_rank8.csv' using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#2196F3' title 'Island 8', \
     'results_v2/convergence_rank12.csv' using 1:($2==0 ? $3 : 1/0) every 2 with lines lw 1.5 lc rgb '#FF9800' title 'Island 12'

unset multiplot
