#!/usr/bin/env python3
"""
convert_simple.py -- Strict converter: ONLY events matching exact 1.5h AGH blocks.

Filters: known teacher, D-10/D-7/D-11, no exams/INNE/NST/SZD,
         start and end times EXACTLY match AGH block boundaries.

Usage:
    python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple/ --stats
"""

import csv, os, argparse, unicodedata, random
from collections import OrderedDict, defaultdict

COL_NAZWA=0; COL_GRUPA=1; COL_TYP=2; COL_TYTUL=3; COL_DZIEN=5
COL_START=8; COL_KONIEC=9; COL_MIEJSCE=10; COL_POJEMN=11; COL_PROWADZ=12

BLOCKS = [(480,570),(585,675),(690,780),(795,885),(900,990),(1005,1095),(1110,1200)]
BLOCK_LABELS = ['8:00-9:30','9:45-11:15','11:30-13:00','13:15-14:45','15:00-16:30','16:45-18:15','18:30-20:00']
VALID_BLD = {'D-10','D-7','D-11'}
SKIP_NAZWA = ['FiIS-INNE','[NST]','-SZD']
SKIP_TYTUL = ['egzamin','konsultacje','kolokwium','zaliczenie','odrobienie','odrabienie']

def nfc(s): return unicodedata.normalize('NFC',s).strip()
def parse_time(t):
    if not t or ':' not in t: return -1
    p=t.strip().split(':')
    try: return int(p[0])*60+int(p[1])
    except: return -1

def match_block(s,e):
    for i,(bs,be) in enumerate(BLOCKS):
        if s==bs and e==be: return i
    return -1

def norm_day(s):
    s=nfc(s)
    if s in ('Pn','Wt','Cz','Pt'): return [s]
    if s in ('Sr','Śr','śr') or (len(s)==2 and s.endswith('r')): return ['Sr']
    days=[]
    i=0
    while i<len(s):
        for code,norm in [('Śr','Sr'),('Sr','Sr'),('Pn','Pn'),('Wt','Wt'),('Cz','Cz'),('Pt','Pt')]:
            if s[i:i+len(code)]==code: days.append(norm); i+=len(code); break
        else: i+=1
    return days

def sanitize(s): return s.replace(',','  ').replace('"','').replace('\\','')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input_file')
    parser.add_argument('output_dir')
    parser.add_argument('--no-grupa', action='store_true')
    parser.add_argument('--subset', type=int, default=0)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--stats', action='store_true')
    args = parser.parse_args()
    os.makedirs(args.output_dir, exist_ok=True)

    with open(args.input_file, encoding='utf-8') as f:
        reader=csv.reader(f); next(reader); raw=list(reader)
    print(f'[1] Raw: {len(raw)}')

    # Clean
    for r in raw:
        if COL_PROWADZ<len(r) and '\n' in r[COL_PROWADZ]:
            r[COL_PROWADZ]=r[COL_PROWADZ].split('\n')[0].strip()
        for i in range(len(r)): r[i]=r[i].replace('\n',' ').replace('\r',' ').strip()

    # Filter: strict
    filtered=[]
    for r in raw:
        if len(r)<13: continue
        nazwa=nfc(r[COL_NAZWA]); tytul=nfc(r[COL_TYTUL])
        teacher=nfc(r[COL_PROWADZ]); room=nfc(r[COL_MIEJSCE])
        bld=room.split()[0] if room.split() else ''

        # Skip: no teacher, wrong building, INNE/NST/SZD, exams, sala_wirtualna
        if not teacher: continue
        if bld not in VALID_BLD: continue
        if any(p in nazwa for p in SKIP_NAZWA): continue
        if any(tytul.lower().startswith(p) for p in SKIP_TYTUL): continue
        if 'sala_wirtualna' in room.lower(): continue

        # STRICT: only exact 1.5h block matches
        start=parse_time(r[COL_START]); end=parse_time(r[COL_KONIEC])
        block=match_block(start,end)
        if block<0: continue

        days=norm_day(r[COL_DZIEN])
        if not days: continue
        for day in days:
            filtered.append(list(r)+[day,str(block)])

    print(f'[2] Strict 1.5h block events: {len(filtered)}')

    # Dedup
    dedup=OrderedDict()
    for r in filtered:
        if args.no_grupa:
            key=(nfc(r[COL_NAZWA]),nfc(r[COL_TYP]),nfc(r[COL_TYTUL]),r[-2],nfc(r[COL_PROWADZ]))
        else:
            key=(nfc(r[COL_NAZWA]),nfc(r[COL_GRUPA]),nfc(r[COL_TYP]),nfc(r[COL_TYTUL]),r[-2],nfc(r[COL_PROWADZ]))
        if key not in dedup: dedup[key]=r
    events=list(dedup.values())
    print(f'[3] Unique weekly: {len(events)}')

    if args.subset>0 and args.subset<len(events):
        random.seed(args.seed)
        events=sorted(random.sample(events,args.subset),key=lambda r:(nfc(r[COL_NAZWA]),nfc(r[COL_TYTUL])))
        print(f'[3] Subset: {args.subset}')

    # Entity maps
    room_map=OrderedDict(); teacher_map=OrderedDict(); group_map=OrderedDict(); room_cap={}
    lab_rooms=set()
    for r in events:
        room=nfc(r[COL_MIEJSCE]); teacher=nfc(r[COL_PROWADZ])
        if nfc(r[COL_TYP])=='CWL': lab_rooms.add(room)
        gk=(nfc(r[COL_NAZWA]),nfc(r[COL_GRUPA])) if not args.no_grupa else (nfc(r[COL_NAZWA]),)
        cap=int(r[COL_POJEMN].strip()) if r[COL_POJEMN].strip().isdigit() else 50
        if room not in room_map: room_map[room]=len(room_map)+1
        if teacher not in teacher_map: teacher_map[teacher]=len(teacher_map)+1
        if gk not in group_map: group_map[gk]=len(group_map)+1
        if room not in room_cap or cap>room_cap[room]: room_cap[room]=cap

    # Build program-level parent groups for lecture conflict detection.
    # Extract program prefix from NAZWA (e.g., "FiIS-PFT-1" from "FiIS-PFT-1 EiOpt(1rok)").
    # Wykład events will use the parent group_id; subgroup events get parent_group_id reference.
    import re as _re
    program_map=OrderedDict()  # program_prefix -> parent_group_id
    group_to_program={}  # group_key -> program_prefix
    programs_with_lectures=set()

    for r in events:
        nazwa=nfc(r[COL_NAZWA])
        typ=nfc(r[COL_TYP])
        # Extract program prefix: "FiIS-PFT-1", "FiIS-PIS-2", "FiIS-OB", etc.
        # Skip external events [EXT] — different faculty, no shared lectures
        if '[EXT]' in nazwa:
            continue
        # Match patterns like "FiIS-PFT-1", "FiIS-OB", "FiIS [ESA]"
        m=_re.match(r'(FiIS[-\s]\S+?)(?:\s|$)', nazwa)
        if m:
            prefix=m.group(1).rstrip('-')
            gk=(nazwa,nfc(r[COL_GRUPA])) if not args.no_grupa else (nazwa,)
            group_to_program[gk]=prefix
            if prefix not in program_map:
                program_map[prefix]=len(group_map)+len(program_map)+1
            if typ in ('Wykład','WYK'):
                programs_with_lectures.add(prefix)

    # Only create parent groups for programs that actually have lectures
    parent_groups={}  # program_prefix -> parent_group_id (only if has lectures)
    next_pgid=len(group_map)+1
    for prefix in sorted(programs_with_lectures):
        parent_groups[prefix]=next_pgid
        next_pgid+=1

    print(f'[4] Rooms:{len(room_map)} Teachers:{len(teacher_map)} Groups:{len(group_map)}'
          f' ParentGroups:{len(parent_groups)} (programs with lectures)')
    if parent_groups:
        for p,pid in parent_groups.items():
            print(f'    {p} -> parent_group_id={pid}')

    # Write CSVs
    with open(os.path.join(args.output_dir,'rooms.csv'),'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f,quoting=csv.QUOTE_NONE,escapechar='\\')
        w.writerow(['id','name','capacity','is_lab'])
        for name,rid in room_map.items():
            w.writerow([rid,sanitize(name),room_cap.get(name,50),1 if name in lab_rooms else 0])

    with open(os.path.join(args.output_dir,'teachers.csv'),'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f,quoting=csv.QUOTE_NONE,escapechar='\\')
        w.writerow(['id','name'])
        for name,tid in teacher_map.items():
            w.writerow([tid,sanitize(name)])

    gcaps=defaultdict(list)
    for r in events:
        gk=(nfc(r[COL_NAZWA]),nfc(r[COL_GRUPA])) if not args.no_grupa else (nfc(r[COL_NAZWA]),)
        cap=int(r[COL_POJEMN].strip()) if r[COL_POJEMN].strip().isdigit() else 30
        gcaps[gk].append(cap)

    with open(os.path.join(args.output_dir,'groups.csv'),'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f,quoting=csv.QUOTE_NONE,escapechar='\\')
        w.writerow(['id','name','num_students'])
        for gk,gid in group_map.items():
            caps=sorted(gcaps.get(gk,[30]))
            w.writerow([gid,sanitize(' '.join(gk)),caps[len(caps)//2] if caps else 30])
        # Add parent groups (program-level, for lecture conflict detection)
        for prefix,pgid in parent_groups.items():
            # Aggregate: max capacity of any subgroup in this program
            max_cap=250  # lectures are typically in large halls
            w.writerow([pgid,sanitize(f'{prefix} [WYKLAD]'),max_cap])

    with open(os.path.join(args.output_dir,'courses.csv'),'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f,quoting=csv.QUOTE_NONE,escapechar='\\')
        w.writerow(['id','name','teacher_id','group_id','hours_per_week','requires_lab','duration_slots','parent_group_id'])
        for i,r in enumerate(events):
            teacher=nfc(r[COL_PROWADZ])
            typ=nfc(r[COL_TYP])
            gk=(nfc(r[COL_NAZWA]),nfc(r[COL_GRUPA])) if not args.no_grupa else (nfc(r[COL_NAZWA]),)
            name=sanitize(f'{nfc(r[COL_TYTUL])} ({typ})')
            program=group_to_program.get(gk)
            pgid=parent_groups.get(program,0) if program else 0

            if typ in ('Wykład','WYK') and pgid>0:
                # Wykład uses parent group as its group_id (blocks whole program)
                w.writerow([i+1,name,teacher_map[teacher],pgid,1,0,1,0])
            else:
                # Non-lecture: keeps subgroup, references parent for conflict check
                w.writerow([i+1,name,teacher_map[teacher],group_map[gk],1,0,1,pgid])

    # Original schedule (UniTime assignments)
    DAY_MAP={'Pn':0,'Wt':1,'Sr':2,'Cz':3,'Pt':4}
    with open(os.path.join(args.output_dir,'original_schedule.csv'),'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f)
        w.writerow(['event_id','day','period','room_id','teacher_id','group_id','duration_slots','course_name','room_name','teacher_name','group_name','type'])
        for i,r in enumerate(events):
            day=DAY_MAP.get(r[-2],0)
            block=int(r[-1])
            room=nfc(r[COL_MIEJSCE]); teacher=nfc(r[COL_PROWADZ])
            gk=(nfc(r[COL_NAZWA]),nfc(r[COL_GRUPA])) if not args.no_grupa else (nfc(r[COL_NAZWA]),)
            w.writerow([i,day,block,room_map.get(room,1),teacher_map.get(teacher,1),group_map.get(gk,1),1,
                        sanitize(f'{nfc(r[COL_TYTUL])} ({nfc(r[COL_TYP])})'),sanitize(room),sanitize(teacher),sanitize(' '.join(gk)),nfc(r[COL_TYP])])

    # Stats
    n=len(events); nr=len(room_map); avail=nr*7*5
    print(f'\nSimple model (strict 1.5h blocks):')
    print(f'  {n} events, {nr} rooms, 7 blocks/day x 5 days')
    print(f'  Available: {avail} room-blocks, Fill: {100*n/avail:.1f}%')
    print(f'  Teachers: {len(teacher_map)} (all known)')
    print(f'  Groups: {len(group_map)}')

    if args.stats:
        typ_dist=defaultdict(int)
        for r in events: typ_dist[nfc(r[COL_TYP])]+=1
        print(f'\n  By type:')
        for t,c in sorted(typ_dist.items(),key=lambda x:-x[1]):
            print(f'    {t:25s}: {c}')

if __name__=='__main__': main()
