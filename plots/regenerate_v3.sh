#!/bin/bash
# regenerate_v3.sh -- Wygeneruj wszystkie 3 PNG ze rzeczywistych danych
#
# Wymagania:
#   - gnuplot (na taurusie zainstalowany)
#   - results_v3/unitime_v2_*.csv (committed do repo)
#   - convergence_rank*.csv w CWD (produkowane przez run klastra)
#
# Użycie (na taurusie po SSH):
#   cd ~/parallel-ga-timetabling
#   bash plots/regenerate_v3.sh
#
# Wynik: plots/{speedup_v3,quality_scaling_v3,convergence_v3}.png
# Skopiuj je do parallel-ga-timetabling-report/figures/ jako:
#   speedup.png, quality_scaling.png, convergence.png

set -e

cd "$(dirname "$0")/.."

echo "[1/3] Generowanie speedup_v3.png ze speedup_v3.gp..."
gnuplot plots/speedup_v3.gp
echo "  -> plots/speedup_v3.png"

echo "[2/3] Generowanie quality_scaling_v3.png ze quality_scaling_v3.gp..."
gnuplot plots/quality_scaling_v3.gp
echo "  -> plots/quality_scaling_v3.png"

if ls convergence_rank*.csv >/dev/null 2>&1; then
    echo "[3/3] Generowanie convergence_v3.png z convergence_rank*.csv..."
    gnuplot plots/convergence_v3.gp
    echo "  -> plots/convergence_v3.png"
else
    echo "[3/3] POMINIĘTE: brak convergence_rank*.csv w CWD."
    echo "    Najpierw uruchom benchmark żeby je wygenerować:"
    echo "      source /opt/nfs/config/source_mpich500.sh"
    echo "      source /opt/nfs/config/source_cuda121.sh"
    echo "      export MPIR_CVAR_ENABLE_GPU=0"
    echo "      /opt/nfs/config/station204_name_list.sh 1 16 > nodes"
    echo "      mpiexec -f nodes -n 16 ./timetable_ga data/simple/"
    echo "    Potem uruchom ten skrypt ponownie."
fi

echo ""
echo "Gotowe. Skopiuj PNG do raportu:"
echo "  cp plots/speedup_v3.png ../parallel-ga-timetabling-report/figures/speedup.png"
echo "  cp plots/quality_scaling_v3.png ../parallel-ga-timetabling-report/figures/quality_scaling.png"
echo "  cp plots/convergence_v3.png ../parallel-ga-timetabling-report/figures/convergence.png"
