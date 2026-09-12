#!/usr/bin/env python3
"""Extract the verse lines of a Shakespeare play as plain text, one line per line,
for `splc --scan-text`, `--suggest-stress` and tools/mine_stress.py.

usage: play_to_lines.py PLAY... > lines.txt

PLAY may be
  * a shakespeare.mit.edu play page (full.html, or the per-scene pages): dialogue
    lines are <A NAME=act.scene.line>...</A> (full.html) or <A NAME=line>...</A>
    (scene pages), speakers are <b>NAME</b>;
  * a directory holding such a play: its full.html is used;
  * a plain-text play in the "speaker on its own line, dialogue indented" layout
    used by the common GitHub mirrors of the same site.

Stage directions, speaker labels and prose-looking lines (not starting with a capital
letter, or fewer than `--min-words` words, default 6) are dropped.  The result is
noisy where the play mixes prose and verse; Richard II and Richard III are entirely
verse and make the cleanest corpora.
"""
import html, os, re, sys

MIN_WORDS = 6
STAGE = re.compile(r"^\s*(Enter|Exit|Exeunt|Re-enter|Flourish|Alarum|Sennet|Drum|Trumpet|Music|Aside)\b")

def from_html(text):
    for m in re.finditer(r"<A NAME=\d+(?:\.\d+\.\d+)?>(.*?)</A>", text, re.S | re.I):
        yield html.unescape(re.sub(r"<[^>]+>", "", m.group(1))).strip()

def from_plain(text):
    for raw in text.splitlines():
        if raw.startswith(" ") and not STAGE.match(raw):
            yield raw.strip()

def main(args):
    global MIN_WORDS
    files = []
    i = 0
    while i < len(args):
        if args[i] == "--min-words": MIN_WORDS = int(args[i + 1]); i += 2
        else: files.append(args[i]); i += 1
    if not files:
        print(__doc__); return 2
    n = 0
    for path in files:
        if os.path.isdir(path): path = os.path.join(path, "full.html")
        text = open(path, encoding="latin-1").read()
        lines = from_html(text) if "<A NAME=" in text.upper() or "<a name=" in text else from_plain(text)
        for l in lines:
            if not l or not l[0].isupper() or len(l.split()) < MIN_WORDS: continue
            if STAGE.match(l): continue
            print(l); n += 1
    print(f"{n} lines", file=sys.stderr)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
