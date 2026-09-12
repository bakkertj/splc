#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
"""Write a Shakespeare Programming Language play that prints a given text.

usage: text_to_spl.py [--verse] [--title T] [--speaker NAME] [--listener NAME] text.txt > play.spl

One character (the listener) holds the current output byte; the other (the speaker)
tells them what to become and to speak. Each byte is reached from the previous one by
adding or subtracting a constant, spelled as a sum of powers of two (`a big big cat` is
4), or set directly, whichever is shorter; a run of the same byte is a single assignment
followed by several `Speak thy mind!`.

Without --verse the play is prose and says so with a `[Prose]` stage direction.

With --verse every line is iambic pentameter and the lines rhyme in couplets, so the
play passes `splc -fpentameter=error -frhyme-scheme=AABB`. The trick is that the
generator uses only monosyllables (the listener is `thee`, never `thyself`; subtraction
adds an insulting noun rather than saying `the difference between`), and every
monosyllable is metrically flexible, so any run of ten words scans. That leaves the
rhymes: a dynamic programme over the sentences chooses, for each byte, the spelling of
its constant (direct or as a delta, with the terms in either order) that makes the tenth
word of as many lines as possible fall on a noun, and the nouns, which are free
choices, are then picked from rhyme classes drawn from the compiler's own lexicon.
"""
import collections, itertools, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PLAIN_NOUNS = ["cat", "hat", "bird", "flower", "rose", "star", "ship", "song", "tree", "kiss", "jewel", "angel"]
INSULTS = ["pig", "toad", "worm", "hog", "wolf", "thief"]

# Common monosyllabic nouns; the lexicon decides which are usable (neutral or positive
# polarity, one syllable) and how they rhyme.
COMMON = """cat hat bat rat mat vat gnat dog log frog bee tree sea key knee plea fee tea king ring wing string
spring thing night light knight sight height day bay tray clay hay jay ray way bow crow toe snow row
star car bar jar ship lip chip hip tip rose nose hose bug jug mug rug fox box ox book hook brook cook rook hand band
land sand moon noon spoon tune dune loon door floor shore boat coat goat note vote moat cake lake rake
bird word herd curd bell shell well spell cup pup ball wall hall stone bone cone throne ale whale sail
pail nail tale gale mail rain train chain brain lane cane plane crane bride tide side ride pride glove dove
love cloud crowd sun bun run nun horn corn thorn rock lock clock sock frock dock cheek beak peak creek
pear bear chair hair mare air heir gold fold hold mold hill mill pill quill sill drill gem hem stem sword lord cord
board ford chord duke nook look lamb jam ram clam dam yam pen hen wren den fen glen fish dish wish
vine wine line pine sign shrine gate plate date slate mate crate steed reed seed weed deed bead beast feast priest
yeast host post toast boast roast bread thread head bed sled shed farm arm charm barn yarn harp carp
pearl girl curl swirl grape cape tape drape ape crown gown town down clown lute flute root fruit boot suit
heart part chart dart fig twig wig sprig beak peak cheek creek sky pie tie fly spy hind rind wind drum gum plum thumb
cow bough plough brow pan fan van man can hood wood love dove glove hum wing""".split()

# Words the templates end lines with; a line ending on one of these can still rhyme
# with a noun of the same class in its partner line (thee/tree, of/love, and/hand).
ENDERS = ["thee", "of", "and", "art", "big", "speak", "thy", "mind", "sum", "thou", "than", "good", "worse", "as", "is", "the", "a"]

def load_rhyme_classes():
    """From generated/Lexicon.inc: ({rhyme key: [noun, ...]} for monosyllabic, non-negative
    nouns, {line-ending word: rhyme key})."""
    classes = collections.defaultdict(list)
    enders = {}
    path = os.path.join(ROOT, "generated", "Lexicon.inc")
    wanted = set(COMMON)
    for line in open(path):
        m = re.match(r'\{"([a-z]+)",(\d+),(-?\d),"([^"]*)","([^"]*)"\},', line)
        if not m: continue
        w, flags, pol, stress, rhyme = m.groups()
        if not rhyme: continue
        rhyme = rhyme.split("|")[0]
        if w in wanted and int(flags) & 1 and int(pol) >= 0 and stress == "1":
            classes[rhyme].append(w)
        if w in ENDERS: enders[w] = rhyme
    return {k: v for k, v in classes.items() if len(v) >= 2}, enders

# ---------------------------------------------------------------- prose mode ----

def powers(n, nouns, idx):
    parts = []
    for k in range(n.bit_length()):
        if n >> k & 1:
            noun = nouns[idx % len(nouns)]; idx += 1
            parts.append(("an " if noun[0] in "aeiou" else "a ") + "big " * k + noun)
    parts.reverse()
    expr = parts[-1]
    for p in reversed(parts[:-1]):
        expr = f"the sum of {p} and {expr}"
    return expr, idx

def prose(text, title, speaker, listener):
    out = [f"{title}\n", f"{listener}, who says what he is told.", f"{speaker}, who tells him.\n",
           "                    Act I: The recitation.\n", "                    Scene I: Every letter in its turn.\n",
           f"[Enter {listener} and {speaker}]", "[Prose]\n", f"{speaker}:"]
    cur, idx, first = 0, 0, True
    data = text.encode("utf-8")
    j = 0
    while j < len(data):
        b = data[j]
        run = 1
        while j + run < len(data) and data[j + run] == b: run += 1
        d = b - cur
        if d != 0:
            direct, idx1 = powers(b, PLAIN_NOUNS, idx)
            if d > 0:
                delta, idx2 = powers(d, PLAIN_NOUNS, idx); delta = f"the sum of thyself and {delta}"
            else:
                delta, idx2 = powers(-d, PLAIN_NOUNS, idx); delta = f"the difference between thyself and {delta}"
            if first or len(direct) <= len(delta): out.append(f" Thou art {direct}."); idx = idx1
            else: out.append(f" Thou art {delta}."); idx = idx2
            cur = b
        out.append(" Speak thy mind!" * run)
        first = False
        j += run
    out.append("\n[Exeunt]")
    return "\n".join(out)

# ---------------------------------------------------------------- verse mode ----

# A sentence is a list of words; the string "NOUN" is a slot for a neutral noun (value 1)
# and "BAD" a slot for an insulting one (value -1).  Everything else is a monosyllable.
def terms(n, slot):
    """the powers of two making n, each as a word list ending in a slot"""
    return [["a"] + ["big"] * k + [slot] for k in range(n.bit_length()) if n >> k & 1]

def chain(ts):
    """['the','sum','of', T1, 'and', 'the','sum','of', T2, 'and', T3]"""
    words = []
    for i, t in enumerate(ts):
        if i + 1 < len(ts): words += ["the", "sum", "of"] + t + ["and"]
        else: words += t
    return words

def spellings(b, cur, first):
    """Every candidate sentence (word list, without the final '.') that makes the listener b."""
    out = []
    ts = terms(b, "NOUN")
    orders = {tuple(map(tuple, p)) for p in itertools.permutations(ts)} if len(ts) <= 4 else {tuple(map(tuple, ts)), tuple(map(tuple, reversed(ts)))}
    for o in orders: out.append(["Thou", "art"] + chain([list(t) for t in o]))
    if not first and b != cur:
        d = b - cur
        dts = terms(abs(d), "NOUN" if d > 0 else "BAD")
        dorders = {tuple(map(tuple, p)) for p in itertools.permutations(dts)} if len(dts) <= 4 else {tuple(map(tuple, dts)), tuple(map(tuple, reversed(dts)))}
        for o in dorders:
            body = chain([list(t) for t in o])
            out.append(["Thou", "art", "the", "sum", "of", "thee", "and"] + body)
            out.append(["Thou", "art", "the", "sum", "of"] + body + ["and", "thee"])
    # keep spellings with distinct slot layouts, shortest first, up to a limit
    out.sort(key=len)
    seen, kept = set(), []
    for c in out:
        key = tuple(i for i, w in enumerate(c) if w in ("NOUN", "BAD"))
        if key in seen: continue
        seen.add(key); kept.append(c)
        if len(kept) == 40: break
    return kept

SPEAK = ["Speak", "thy", "mind!"]
PUNCT = ".?!"
def core(w): return w.rstrip(PUNCT)
def is_slot(w): return core(w) in ("NOUN", "BAD")

# Harmless sentences the generator may insert to move a line end onto a noun: a question
# sets the condition flag, which nothing reads; "Thou art thee" assigns the listener to
# himself.  All monosyllables, so the metre does not care where they fall.
FILLERS = [
    ["Art", "thou", "as", "good", "as", "a", "NOUN?"],
    ["Is", "a", "NOUN", "as", "good", "as", "thee?"],
    ["Art", "thou", "worse", "than", "a", "NOUN?"],
    ["Art", "thou", "worse", "than", "thee?"],
    ["Thou", "art", "thee."],
]
FILLER_COMBOS = [[]] + [f for f in FILLERS] + [f + g for f in FILLERS for g in FILLERS]

def with_stop(c):
    return c[:-1] + [c[-1] + "."] if c else []

def layout(steps, classes, enders):
    """steps: list of (candidates, run).  For each step choose a candidate spelling and a
    (possibly empty) run of fillers so that as many couplets as possible (lines 1-2, 3-4,
    ...) can rhyme, at the least cost in extra words.  A line end is a noun slot (which
    can take any rhyme) or a fixed word; a fixed word rhymes only with a slot filled from
    its own class, or with itself.  State: (position in line, parity, previous line end)."""
    def end_kind(w):
        c = core(w)
        if c == "NOUN": return "SLOT"
        k = enders.get(c.lower())
        return k if k and classes.get(k) else None   # a word with no nouns in its class cannot rhyme
    def pair_ok(a, b):
        if a is None or b is None: return False
        if a == "SLOT" or b == "SLOT": return True
        return a == b
    best = {(0, 0, None): (0.0, None, None)}
    history = []
    for cands, run in steps:
        nxt = {}
        base = len(with_stop(cands[0])) + 3 * run
        # options that lay their line ends on the same kinds of words are interchangeable
        options, sigs = [], set()
        for c in cands:
            for f in FILLER_COMBOS:
                seq = with_stop(c) + SPEAK * run + f
                sig = (len(seq) % 10, tuple((i % 10, end_kind(w)) for i, w in enumerate(seq) if i % 10 == 9 or end_kind(w)))
                if sig in sigs: continue
                sigs.add(sig); options.append(seq)
        # walk each option once per starting position; the line ends it hits depend only on that
        walks = {}
        for oi, seq in enumerate(options):
            kinds = [end_kind(w) for w in seq]
            for p0 in range(10):
                ends, p = [], p0
                for k in kinds:
                    p += 1
                    if p == 10: ends.append(k); p = 0
                walks[oi, p0] = (ends, p)
        for state, (cost, _, _) in best.items():
            p0, parity0, prev0 = state
            for oi, seq in enumerate(options):
                ends, p = walks[oi, p0]
                cst = cost + 0.02 * (len(seq) - base)
                parity, prev = parity0, prev0
                for k in ends:
                    if parity == 1 and not pair_ok(prev, k): cst += 1
                    prev, parity = k, 1 - parity
                ns = (p, parity, prev)
                if ns not in nxt or cst < nxt[ns][0]:
                    nxt[ns] = (cst, seq, state)
        history.append(nxt)
        best = nxt
    state = min(best, key=lambda k: best[k][0])
    chosen = []
    for nxt in reversed(history):
        cost, seq, prev = nxt[state]
        chosen.append(seq)
        state = prev
    chosen.reverse()
    return chosen

def verse(text, title, speaker, listener):
    classes, enders = load_rhyme_classes()
    data = text.encode("utf-8")
    steps = []
    cur, first, j = 0, True, 0
    while j < len(data):
        b = data[j]
        run = 1
        while j + run < len(data) and data[j + run] == b: run += 1
        steps.append((spellings(b, cur, first) if (b != cur or first) else [[]], run))
        cur, first = b, False
        j += run
    words = [w for seq in layout(steps, classes, enders) for w in seq]
    lines = [words[i:i + 10] for i in range(0, len(words), 10)]
    # rhymes: fill the slot at the end of each line of a couplet from a class its partner
    # can share; classes with two or more nouns rotate for slot/slot pairs
    two = sorted(k for k, v in classes.items() if len(v) >= 2)
    all_nouns = {n for v in classes.values() for n in v}
    plain_cycle = itertools.cycle([n for k in sorted(classes) for n in classes[k]])
    bad_cycle = itertools.cycle(INSULTS)
    used = collections.Counter()
    def put(l, noun): l[-1] = noun + l[-1][len(core(l[-1])):]
    def fresh(k):
        v = classes[k]; n = v[used[k] % len(v)]; used[k] += 1; return n
    ci = 0
    for i in range(0, len(lines) - 1, 2):
        a, b = lines[i], lines[i + 1]
        ea, eb = core(a[-1]), core(b[-1])
        if ea == "NOUN" and eb == "NOUN":
            k = two[ci % len(two)]; ci += 1
            put(a, fresh(k)); put(b, fresh(k))
        elif ea == "NOUN" and enders.get(eb.lower()) in classes: put(a, fresh(enders[eb.lower()]))
        elif eb == "NOUN" and enders.get(ea.lower()) in classes: put(b, fresh(enders[ea.lower()]))
    for l in lines:
        for k, w in enumerate(l):
            tail = w[len(core(w)):]
            if core(w) == "NOUN": l[k] = next(plain_cycle) + tail
            elif core(w) == "BAD": l[k] = next(bad_cycle) + tail
        for k in range(len(l) - 1):
            if l[k] == "a" and l[k + 1][0] in "aeiou": l[k] = "an"
    body = "\n".join(" " + " ".join(l) for l in lines)
    def key(w):
        c = core(w).lower()
        for k, v in classes.items():
            if c in v: return k
        return enders.get(c)
    couplets = sum(1 for i in range(0, len(lines) - 1, 2) if key(lines[i][-1]) is not None and key(lines[i][-1]) == key(lines[i + 1][-1]))
    print(f"{len(lines)} lines, {couplets} rhymed couplets of {len(lines) // 2}", file=sys.stderr)
    return "\n".join([f"{title}\n", f"{listener}, who says what he is told.", f"{speaker}, who tells him in verse.\n",
                       "                    Act I: The recitation.\n", "                    Scene I: Every letter in its turn, and every line a line.\n",
                       f"[Enter {listener} and {speaker}]\n", f"{speaker}:", body, "\n[Exeunt]"])

def main(argv):
    title, speaker, listener, mode = "A Text, Recited.", "Juliet", "Romeo", "prose"
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--title": title = argv[i + 1]; i += 2
        elif argv[i] == "--speaker": speaker = argv[i + 1]; i += 2
        elif argv[i] == "--listener": listener = argv[i + 1]; i += 2
        elif argv[i] == "--verse": mode = "verse"; i += 1
        else: files.append(argv[i]); i += 1
    if not files:
        print(__doc__); return 2
    text = open(files[0], encoding="utf-8").read()
    print(verse(text, title, speaker, listener) if mode == "verse" else prose(text, title, speaker, listener))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
