"""
generate_real_pngs.py -- Wygeneruj 2 PNG (speedup, quality_scaling) ze
zweryfikowanych danych z results_v3/unitime_v2_*.csv.

Uruchomienie:
    python plots/generate_real_pngs.py

Wynik:
    plots/speedup_real.png
    plots/quality_scaling_real.png
"""

import csv
import os
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS = os.path.join(ROOT, "results_v3")

DATASETS = [
    ("100",  "unitime_v2_n100.csv"),
    ("200",  "unitime_v2_n200.csv"),
    ("400",  "unitime_v2_n400.csv"),
    ("633",  "unitime_v2_nogrupa.csv"),  # full data/simple/
]

COLORS = {"100": "#2196F3", "200": "#4CAF50", "400": "#FF9800", "633": "#F44336"}
MARKERS = {"100": "o", "200": "s", "400": "^", "633": "D"}


def load_avg(csv_path):
    """Return dict[n_procs] -> dict('time': avg_t, 'soft': avg_soft)."""
    bucket = defaultdict(lambda: {"t": [], "s": []})
    with open(csv_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            n = int(row["n_procs"])
            bucket[n]["t"].append(float(row["time_sec"]))
            bucket[n]["s"].append(int(row["soft_violations"]))
    return {
        n: {"time": sum(b["t"]) / len(b["t"]),
            "soft": sum(b["s"]) / len(b["s"])}
        for n, b in bucket.items()
    }


def main():
    data = {label: load_avg(os.path.join(RESULTS, fname)) for label, fname in DATASETS}

    # --- speedup chart ---
    fig, ax = plt.subplots(figsize=(9, 6))
    procs = [1, 4, 8, 16]
    ax.plot([0.5, 17], [0.5, 17], "--", color="#cccccc", lw=1.5, label="Idealny liniowy")
    for label, _ in DATASETS:
        d = data[label]
        t1 = d[1]["time"]
        speedup = [t1 / d[p]["time"] for p in procs]
        ax.plot(procs, speedup, marker=MARKERS[label], ms=10, lw=2,
                color=COLORS[label],
                label=f"{label} zdarzeń (S₁₆={speedup[-1]:.2f}×)")
    ax.set_xlabel("Liczba procesów MPI", fontsize=12)
    ax.set_ylabel("Przyspieszenie S(p) = T(1)/T(p)", fontsize=12)
    ax.set_title("Przyspieszenie vs liczba procesów\n(Island Model GA, klaster MPI 16 węzłów)",
                 fontsize=13)
    ax.set_xticks(procs)
    ax.set_xlim(0.5, 17.5)
    ax.set_ylim(0, 24)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper left", fontsize=10)
    fig.tight_layout()
    out = os.path.join(ROOT, "plots", "speedup_real.png")
    fig.savefig(out, dpi=120)
    print(f"  -> {out}")
    plt.close(fig)

    # --- quality scaling: 2 panels ---
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))

    # left: soft viol vs events
    sizes = [int(label) for label, _ in DATASETS]
    soft_n1 = [data[label][1]["soft"] for label, _ in DATASETS]
    soft_n16 = [data[label][16]["soft"] for label, _ in DATASETS]
    ax1.plot(sizes, soft_n1, marker="o", ms=9, lw=2, color="#F44336", label="n=1 (sekwencyjnie)")
    ax1.plot(sizes, soft_n16, marker="s", ms=9, lw=2, color="#4CAF50", label="n=16 (równolegle)")
    ax1.set_xlabel("Liczba zdarzeń", fontsize=12)
    ax1.set_ylabel("Naruszenia miękkie (avg z 3 runów)", fontsize=12)
    ax1.set_title("Jakość rozwiązania vs rozmiar problemu", fontsize=13)
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=10)

    # right: time vs events (log)
    t_n1 = [data[label][1]["time"] for label, _ in DATASETS]
    t_n16 = [data[label][16]["time"] for label, _ in DATASETS]
    ax2.plot(sizes, t_n1, marker="o", ms=9, lw=2, color="#F44336", label="n=1")
    ax2.plot(sizes, t_n16, marker="s", ms=9, lw=2, color="#4CAF50", label="n=16")
    ax2.set_xlabel("Liczba zdarzeń", fontsize=12)
    ax2.set_ylabel("Czas wall-clock [s]", fontsize=12)
    ax2.set_title("Czas wykonania vs rozmiar problemu", fontsize=13)
    ax2.set_yscale("log")
    ax2.grid(True, alpha=0.3, which="both")
    ax2.legend(fontsize=10)

    fig.tight_layout()
    out = os.path.join(ROOT, "plots", "quality_scaling_real.png")
    fig.savefig(out, dpi=120, bbox_inches="tight")
    print(f"  -> {out}")
    plt.close(fig)

    # --- print summary table ---
    print("\n=== Zweryfikowane średnie ===")
    print(f"{'dataset':>8} {'T1 [s]':>10} {'T4 [s]':>10} {'T8 [s]':>10} {'T16 [s]':>10} "
          f"{'S16':>8} {'soft_T1':>9} {'soft_T16':>10}")
    for label, _ in DATASETS:
        d = data[label]
        t1, t4, t8, t16 = d[1]["time"], d[4]["time"], d[8]["time"], d[16]["time"]
        s1, s16 = d[1]["soft"], d[16]["soft"]
        print(f"{label:>8} {t1:>10.3f} {t4:>10.3f} {t8:>10.3f} {t16:>10.3f} "
              f"{t1/t16:>7.2f}x {s1:>9.1f} {s16:>10.1f}")


if __name__ == "__main__":
    main()
