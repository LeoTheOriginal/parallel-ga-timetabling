#!/usr/bin/env python3
"""
pretty_pis2.py -- czytelny plan zajec dla grupy FiIS-PIS-2 z wynikow GA.

Joinuje schedule.csv (output GA) z courses/rooms/groups CSV-ami.
Wypisuje plan w formie pionowej (czytelne na konsoli, bez problemow z szerokoscia).

Uzycie:
    python3 scripts/pretty_pis2.py [data_dir] [schedule_csv]

Domyslnie: data/simple/ + schedule.csv
"""
import csv
import sys
from collections import defaultdict

DATA_DIR = sys.argv[1] if len(sys.argv) > 1 else "data/simple"
SCHEDULE = sys.argv[2] if len(sys.argv) > 2 else "schedule.csv"

DAYS = ["Pn", "Wt", "Sr", "Cz", "Pt"]
TIMES = ["8:00", "9:45", "11:30", "13:15", "15:00", "16:45", "18:30"]


def load_lookup(path, name_field="name"):
    out = {}
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            out[r["id"]] = r[name_field]
    return out


def main():
    groups = load_lookup(f"{DATA_DIR}/groups.csv")
    courses = load_lookup(f"{DATA_DIR}/courses.csv")
    rooms = load_lookup(f"{DATA_DIR}/rooms.csv")

    pis2_ids = {gid: name for gid, name in groups.items() if name.startswith("FiIS-PIS-2")}

    if not pis2_ids:
        print("Nie znaleziono grup FiIS-PIS-2 w datasecie.")
        sys.exit(1)

    by_subgroup = defaultdict(list)
    with open(SCHEDULE, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            gid = r["group_id"]
            if gid in pis2_ids:
                by_subgroup[gid].append({
                    "day": int(r["day"]),
                    "period": int(r["period"]),
                    "course": courses.get(r["course_id"], "?"),
                    "room": rooms.get(r["room_id"], "?"),
                })

    print("=" * 78)
    print("  Plan zajec dla grupy nadrzednej FiIS-PIS-2")
    print("=" * 78)
    print()

    abbrev = {
        "AiPO": "Analiza i przetwarzanie obrazow",
        "SSN": "Sztuczne sieci neuronowe",
        "FWI": "Fizyka wspolczesna w informatyce",
        "SRiR": "Systemy Rownolegle i Rozproszone",
        "UM": "Uczenie maszynowe",
        "ZTI": "Zaawansowane technologie informacyjne",
    }

    total = 0
    print(f"  {'Skrot':<5}  {'Pelna nazwa':<40}  {'Liczba zaj.':>11}")
    print(f"  {'-'*5}  {'-'*40}  {'-'*11}")
    for gid, subgroup_name in sorted(pis2_ids.items(), key=lambda x: int(x[0])):
        short = subgroup_name.split()[1].split("(")[0]
        full = abbrev.get(short, short)
        n = len(by_subgroup.get(gid, []))
        marker = "  <-- nasz przedmiot" if short == "SRiR" else ""
        print(f"  {short:<5}  {full:<40}  {n:>11}{marker}")
        total += n
    print(f"  {'-'*5}  {'-'*40}  {'-'*11}")
    print(f"  {'RAZEM':<5}  {' ':<40}  {total:>11}")
    print()

    srir_id = next((gid for gid, n in pis2_ids.items() if "SRiR" in n), None)
    if srir_id and by_subgroup.get(srir_id):
        print("-" * 78)
        print("  Plan dla podgrupy FiIS-PIS-2 SRiR (Systemy Rownolegle):")
        print("-" * 78)
        print()
        events = sorted(by_subgroup[srir_id], key=lambda e: (e["day"], e["period"]))
        for e in events:
            day = DAYS[e["day"]]
            time = TIMES[e["period"]]
            print(f"    {day}  {time:>5}    SRiR    sala: {e['room']}")
        print()
        print(f"    Razem: {len(events)} zajec")
    else:
        print("(brak danych o SRiR w wyniku)")


if __name__ == "__main__":
    main()
