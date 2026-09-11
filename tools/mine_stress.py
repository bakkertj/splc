#!/usr/bin/env python3
"""Propose data/elizabethan_stress.tsv lines from a verse corpus.

usage: mine_stress.py [--splc build/splc] [--min N] corpus.txt [corpus2.txt ...]
       e.g. mine_stress.py shakespeare/sonnets_lines.txt shakespeare/richard2_lines.txt

Runs `splc --suggest-stress` over each corpus (one verse line per line), counts how
often each word=pattern fix recurs, drops the words the table already knows and the
patterns that are just a mid-line trochee on an -ing/-er/-ly word, and prints
candidate lines in the .tsv format, most frequent first, with the count as a comment.
Nothing is written; paste what you agree with into data/elizabethan_stress.tsv and
re-run tools/gen_lexicon.py.
"""
import collections, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def load_table(path):
    known = {}
    if os.path.exists(path):
        for l in open(path):
            if l.strip() and not l.startswith("#"):
                w, st = (l.rstrip("\n").split("\t") + [""])[:2]
                known[w.lower()] = st
    return known

def main(argv):
    splc = os.path.join(ROOT, "build", "splc")
    minimum = 2
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--splc": splc = argv[i + 1]; i += 2
        elif argv[i] == "--min": minimum = int(argv[i + 1]); i += 2
        else: files.append(argv[i]); i += 1
    if not files:
        print(__doc__); return 2
    known = load_table(os.path.join(ROOT, "data", "elizabethan_stress.tsv"))
    counts = collections.Counter()
    for f in files:
        out = subprocess.run([splc, "--suggest-stress", f], capture_output=True, text=True).stdout
        for line in out.splitlines():
            if "=" in line: counts[tuple(line.split("=", 1))] += 1
    # A "fix" that only moves stress onto a suffix is a trochaic substitution, not a
    # pronunciation: never propose stressing -ing, -er, -ly, -est, -eth, -ed, -es.
    suffix = re.compile(r"(ing|er|ly|est|eth|ed|es|y)$")
    proposed = 0
    print("# proposed by tools/mine_stress.py; count = lines in the corpus this reading would fix")
    for (w, pat), n in counts.most_common():
        if n < minimum: break
        if w in known and pat in known[w].split("|"): continue
        if suffix.search(w) and pat.endswith("1"): continue
        if "'" in w or "-" in w: continue
        print(f"{w}\t{pat}\t# {n}")
        proposed += 1
    print(f"# {proposed} candidate(s) from {sum(counts.values())} suggested fixes", file=sys.stderr)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
