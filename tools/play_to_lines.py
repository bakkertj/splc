#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
"""Extract the verse lines of a Shakespeare play as plain text, one line per line,
for `splc --scan-text`, `--suggest-stress` and tools/mine_stress.py.

usage: play_to_lines.py [--verse | --prose | --all] PLAY... > lines.txt

PLAY may be
  * a shakespeare.mit.edu play page (full.html, or the per-scene pages): dialogue
    lines are <A NAME=act.scene.line>...</A> (full.html) or <A NAME=line>...</A>
    (scene pages), speakers are <b>NAME</b>;
  * a directory holding such a play: its full.html is used;
  * a plain-text play in the "speaker on its own line, dialogue indented" layout
    used by the common GitHub mirrors of the same site.

Stage directions and speaker labels are dropped, as are lines of fewer than
`--min-words` words (default 6).

The HTML edition does not mark prose, but it wraps prose at a fixed width, so the
continuation lines of a prose speech begin in lower case while every verse line begins
with a capital.  A speech (one <blockquote>) is classed as prose when at least a third
of its lines begin in lower case, as verse when none do and it has two or more lines,
and left unclassified otherwise (one-line speeches).  --verse (the default) prints the
lines of verse speeches, --prose the lines of prose speeches, --all everything that is
not a stage direction, as before.  Plain-text plays only support --all.
"""
import html, os, re, sys

MIN_WORDS = 6
STAGE = re.compile(r"^\s*(Enter|Exit|Exeunt|Re-enter|Flourish|Alarum|Sennet|Drum|Trumpet|Music|Aside)\b")

LINE = re.compile(r"<A NAME=\d+(?:\.\d+\.\d+)?>(.*?)</A>", re.S | re.I)

def clean(fragment):
    return html.unescape(re.sub(r"<[^>]+>", "", fragment)).strip()

def from_html(text, mode):
    if mode == "all":
        for m in LINE.finditer(text): yield clean(m.group(1))
        return
    for sp in re.finditer(r"<blockquote>(.*?)</blockquote>", text, re.S | re.I):
        lines = [clean(m.group(1)) for m in LINE.finditer(sp.group(1))]
        lines = [l for l in lines if l]
        if len(lines) < 2: continue
        lower = sum(1 for l in lines if l[0].islower())
        kind = "prose" if lower * 3 >= len(lines) else ("verse" if lower == 0 else None)
        if kind == mode:
            for l in lines: yield l

def from_plain(text):
    for raw in text.splitlines():
        if raw.startswith(" ") and not STAGE.match(raw):
            yield raw.strip()

def main(args):
    global MIN_WORDS
    files = []
    mode = "verse"
    i = 0
    while i < len(args):
        if args[i] == "--min-words": MIN_WORDS = int(args[i + 1]); i += 2
        elif args[i] in ("--verse", "--prose", "--all"): mode = args[i][2:]; i += 1
        else: files.append(args[i]); i += 1
    if not files:
        print(__doc__); return 2
    n = 0
    for path in files:
        if os.path.isdir(path): path = os.path.join(path, "full.html")
        text = open(path, encoding="latin-1").read()
        lines = from_html(text, mode) if "<A NAME=" in text.upper() else from_plain(text)
        for l in lines:
            if not l or len(l.split()) < MIN_WORDS: continue
            if mode != "prose" and not l[0].isupper(): continue
            if STAGE.match(l): continue
            print(l); n += 1
    print(f"{n} lines", file=sys.stderr)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
