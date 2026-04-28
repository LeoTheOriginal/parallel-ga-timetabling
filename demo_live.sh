#!/bin/bash
# demo_live.sh -- live demo na taurusie po zakonczeniu prezentacji
#
# UZYCIE (na taurusie po SSH):
#   cd ~/Documents/IS/StopienII/SRiR/parallel-ga-timetabling
#   bash demo_live.sh
#
# Skrypt zatrzymuje sie po kazdym etapie - nacisnij ENTER aby kontynuowac.

set -e

# Funkcja czyszczenia ekranu (z fallbackiem gdy brak TERM)
cls() {
    if [ -t 1 ] && [ -n "$TERM" ] && [ "$TERM" != "dumb" ]; then
        clear 2>/dev/null || printf '\n%.0s' {1..40}
    else
        printf '\n%.0s' {1..3}
    fi
}

# ============== ETAP 0: SETUP SRODOWISKA ==============
echo "=========================================="
echo "  Demo: Rownolegly GA - Plan Zajec"
echo "  Projekt 26, grupa 1, Piotrowski/Przezdzik"
echo "=========================================="
echo ""
echo "[Etap 0] Konfiguracja srodowiska MPICH 5.0..."

source /opt/nfs/config/source_mpich500.sh
source /opt/nfs/config/source_cuda121.sh
export MPIR_CVAR_ENABLE_GPU=0

echo "  - MPICH:  $(which mpiexec)"
echo "  - mpicc:  $(which mpicc)"

if [ ! -f timetable_ga ]; then
    echo ""
    echo "  Binarka nie istnieje - kompiluje..."
    make 2>&1 | tail -3
fi

echo ""
read -rp "[ENTER] aby pokazac dane wejsciowe..."

# ============== ETAP 1: PROBLEM ==============
cls
echo "=========================================="
echo "  ETAP 1: Dane wejsciowe (UniTime AGH, pelen dataset)"
echo "=========================================="
echo ""
echo "Dataset: data/simple/  (pelne 633 zdarzen z UniTime AGH, sem. letni 2026)"
echo ""
for f in courses.csv groups.csv rooms.csv teachers.csv; do
    n=$(wc -l < "data/simple/$f")
    printf "  %-15s %4d wierszy\n" "$f" "$((n - 1))"
done
echo ""
echo "Cel: przypisac kazde zdarzenie do pary (slot, sala) tak,"
echo "     aby wyzerowac twarde naruszenia i zminimalizowac miekkie."
echo ""
echo "Przestrzen rozwiazan: (35*41)^633 = 1435^633 ~ 10^2003 mozliwosci."
echo ""
read -rp "[ENTER] aby uruchomic GA na 16 procesach klastra..."

# ============== ETAP 2: 16 PROCESOW NA PELNYM DATASECIE ==============
cls
echo "=========================================="
echo "  ETAP 2: 16 procesow MPI (Island Model)"
echo "  na pelnym datasecie (633 zdarzen)"
echo "=========================================="
echo ""
echo "+ /opt/nfs/config/station204_name_list.sh 1 16 > nodes_par"
/opt/nfs/config/station204_name_list.sh 1 16 > nodes_par
echo "  -> dostepne wezly: $(wc -l < nodes_par) z 16"
echo ""
echo "+ mpiexec -f nodes_par -n 16 ./timetable_ga data/simple/"
echo ""

OUT_PAR=$(mktemp)
mpiexec -f nodes_par -n 16 ./timetable_ga data/simple/ < /dev/null 2>&1 | tee "$OUT_PAR" | tail -12
T_PAR=$(grep -E "^Wall-clock time:" "$OUT_PAR" | tail -1 | awk '{print $3}')
rm -f "$OUT_PAR"

echo ""
echo "  >>> Czas algorytmu (Wall-clock GA): ${T_PAR} s"
echo ""
echo "  Sekwencyjnie ten sam dataset zajalby ~160s (S16~14.4x z tabeli)."
echo ""
read -rp "[ENTER] aby zobaczyc plan zajec dla NASZEJ grupy (PIS-2)..."

# ============== ETAP 3: PLAN DLA GRUPY PIS-2 ==============
cls
echo "=========================================="
echo "  ETAP 3: Plan dla grupy FiIS-PIS-2"
echo "  (Informatyka Stosowana, II stopien)"
echo "=========================================="
echo ""
echo "+ python3 scripts/pretty_pis2.py"
echo ""
python3 scripts/pretty_pis2.py
echo ""
read -rp "[ENTER] aby zakonczyc demo..."

# ============== ETAP 4: ZAKONCZENIE ==============
cls
echo "=========================================="
echo "  Demo zakonczone"
echo "=========================================="
echo ""
echo "Wygenerowane pliki:"
echo "  schedule.csv             - plan zajec w CSV"
echo "  timetable.txt            - wydruk pogrupowany po dniach"
echo "  convergence_rank*.csv    - trajektorie fitness per rank ($(ls convergence_rank*.csv 2>/dev/null | wc -l) plikow)"
echo ""
echo "  Wyniki na pelnym datasecie (633 zdarzen, 16 procesow):"
echo "    T1=159.96s  T16=11.08s  -> S16 = 14.44x"
echo "    Hard violations: 0  Soft violations: ~2820"
echo ""
echo "Zeby wyczyscic artefakty: make clean"
echo ""
echo "Dziekujemy za uwage!"
