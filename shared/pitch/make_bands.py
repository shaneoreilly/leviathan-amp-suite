#!/usr/bin/env python3
"""Builds bands.json (Pitch > Play as > band search) from metal_tunings_cleaned.csv in
https://github.com/vprusso/metaltabs_scraper (MIT, (c) 2017 Vincent Russo): one row per song, "band,song,TUNING".

Songs marked UNKNOWN (and the few "steps up", which are 7-string tabs mislabelled) are left out. A band lists every
tuning that covers at least 15% of its songs, most used first, with the song count, so a band that changed tuning
shows each (song counts only order them; they aren't kept). Tunings are six strings, low to high, as MIDI notes;
names match the apps' tuningName(). One band per line.

Then popular.txt (hand-written: rock, blues, pop, punk, country...) is merged in; an artist there replaces the
tab-site entry with the same name.

    python3 make_bands.py metal_tunings_cleaned.csv > bands.json
"""
import collections, csv, json, os, sys

E = [40, 45, 50, 55, 59, 64]
DOWN = {"STANDARD": 0, "HALF STEP DOWN": 1, "ONE STEP DOWN": 2, "ONE AND HALF STEP DOWN": 3, "TWO STEPS DOWN": 4,
        "TWO AND HALF STEPS DOWN": 5, "THREE STEPS DOWN": 6, "THREE AND HALF STEPS DOWN": 7, "FOUR STEPS DOWN": 8,
        "FOUR AND HALF STEPS DOWN": 9}
DROP = {"DROP D": 38, "DROP C": 36, "DROP B": 35, "DROP A": 33}
NAMES = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]


def strings(t):
    if t in DOWN: return [n - DOWN[t] for n in E]
    if t in DROP:
        low = DROP[t]; up = [n - (38 - low) for n in E]   # standard a whole step above the low string, low string dropped
        return [low] + up[1:]
    return None


def name(s):
    iv = [b - a for a, b in zip(s, s[1:])]
    n = NAMES[s[0] % 12]
    if iv == [5, 5, 5, 4, 5]: return n + " Standard"
    if iv == [7, 5, 5, 4, 5]: return "Drop " + n
    if iv in ([7, 5, 4, 3, 5], [7, 5, 7, 5, 4]): return "Open " + n   # open D / E, open C shapes
    if iv == [5, 7, 5, 4, 3]: return "Open " + NAMES[s[1] % 12]      # open G shape (root on the 5th string)
    if iv == [7, 5, 5, 2, 5] and n == "D": return "DADGAD"
    return " ".join(NAMES[x % 12] for x in s)


OPEN = {"OPEN G": [38, 43, 50, 55, 59, 62], "OPEN D": [38, 45, 50, 54, 57, 62], "OPEN E": [40, 47, 52, 56, 59, 64],
        "OPEN C": [36, 43, 48, 55, 60, 64], "DADGAD": [38, 45, 50, 55, 57, 62]}
NOTE = {n: i for i, n in enumerate(NAMES)} | {"C#": 1, "D#": 3, "Gb": 6, "G#": 8, "A#": 10}


def named(t):
    """A popular.txt tuning: (shown name, strings)."""
    u = t.strip().upper()
    if u in OPEN: return t.strip(), OPEN[u]
    w = t.strip().split()
    if len(w) == 2 and w[1].lower() == "standard" and w[0] in NOTE:
        k = (NOTE[w[0]] - 4) % 12; k = k - 12 if k > 6 else k   # semitones from E, up to a tritone either way
        s = [n + k for n in E]
    elif len(w) == 2 and w[0].lower() == "drop" and w[1] in NOTE:
        k = (NOTE[w[1]] - 2) % 12; k = k - 12 if k > 6 else k   # from Drop D
        s = [n + k for n in [38, 45, 50, 55, 59, 64]]
    else:
        sys.exit("popular.txt: unknown tuning " + t)
    return name(s), s


songs = collections.defaultdict(collections.Counter)
for row in csv.reader(open(sys.argv[1], encoding="utf-8", errors="replace")):
    if len(row) < 3: continue
    band, tuning = row[0].strip(), row[-1].strip().upper()
    s = strings(tuning)
    if band and s: songs[band][tuple(s)] += 1

merged = {}
for band in songs:
    c = songs[band]; total = sum(c.values())
    merged[band.casefold()] = {"name": band, "tunings": [{"name": name(list(s)), "strings": list(s)} for s, k in c.most_common() if k / total >= 0.15]}
here = os.path.dirname(os.path.abspath(__file__))
for line in open(os.path.join(here, "popular.txt"), encoding="utf-8"):
    line = line.strip()
    if not line or line.startswith("#"): continue
    artist, ts = (x.strip() for x in line.split("|", 1))
    merged[artist.casefold()] = {"name": artist, "tunings": [{"name": n, "strings": s} for n, s in map(named, ts.split(";"))]}
bands = sorted(merged.values(), key=lambda b: b["name"].casefold())

print('{"source": "metaltabs.com tabs via github.com/vprusso/metaltabs_scraper (MIT, (c) 2017 Vincent Russo; LICENSE-metaltabs_scraper.txt)",')
print(' "bands": [')
print(",\n".join("  " + json.dumps(b, ensure_ascii=False, separators=(", ", ": ")) for b in bands))
print(" ]}")
