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
play passes `splc -fpentameter=error -frhyme-scheme=AABB`, and the metre is real rather
than a trick of flexible monosyllables: nouns and adjectives carry their dictionary
stress, function words are unstressed, and the planner (see the verse section below)
keeps the stress strictly alternating from the first syllable of the speech to the last,
so no line reads "a big big big big cat".
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
cow bough plough brow pan fan van man can hood wood love dove glove hum wing mice ice dice price spice rice
slice tube purse verse nurse ware lair stair flair""".split()

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
#
# Every word the generator may use carries its stress from the lexicon: "0" for an
# unstressed monosyllable (the, a, of, and, as, than, thy, is), "1" for a stressed one
# (a noun, Speak, mind, good, sum, twice), "10" for a trochee (lovely, garden) and "x"
# for a monosyllable that goes either way (thou, thee, art).  A speech is planned as one
# stream of syllables in which stress strictly alternates, weak-strong, and no word
# straddles a ten-syllable line boundary (a trochaic noun may close a line with a
# feminine ending).  So "a big big cat" never appears: a power of two is spelled with
# trochaic adjectives, "a lovely golden cat" (4), or without the article where the metre
# needs a stressed start, "lovely golden cat", or with "twice".  A dynamic programme
# picks, for each byte, the spelling and the harmless filler sentences that keep the
# alternation going and land as many line ends as possible on rhymable words; the
# nouns and adjectives themselves are chosen afterwards, from rhyme classes drawn from
# the compiler's own lexicon.

PLAIN2 = """garden maiden blossom mountain river morning summer lady lover bosom ocean kingdom
window candle valley pillow honey music shadow fountain forest palace tower harbour angel lily
daisy poppy cherry apple berry mistress sister brother mother father daughter treasure pleasure
beauty glory story virtue nature spirit fortune goddess sonnet ribbon cradle castle chapel cottage
kitten robin sparrow swallow linnet eagle lion tiger falcon dolphin puppy fiddle trumpet lantern
mirror feather dagger arrow bower anchor pillar altar temple""".split()
ADJ = """lovely gentle golden pretty noble happy mighty tender merry jolly bonny gracious precious
handsome honest humble joyful stately shining silver rosy sunny fragrant dainty comely courtly
royal loyal worthy hearty wholesome peaceful playful glowing gleaming smiling charming cheerful
faithful graceful youthful lively kindly saintly mellow modest pleasant princely quiet scarlet
sacred witty winsome simple purple crimson velvet little tiny holy friendly lofty pearly shady
dewy leafy mossy misty snowy frosty silken sylvan verdant""".split()
BAD1 = "pig toad worm hog wolf thief snake curse ass fool louse sin witch cheat fraud dirt grief woe pain doom gloom".split()
BAD2 = "devil villain coward poison sorrow cancer bully vulture scandal terror horror hatred mischief ruin peril danger hunger".split()
BADADJ = """filthy rotten stupid ugly wicked greedy nasty dirty foolish evil sickly sullen surly vulgar
vicious wretched wanton bloody deadly dismal feeble ghastly gloomy guilty hateful hellish horrid idle
lousy mangy moldy muddy paltry rusty shabby shameful sinful slimy sluggish smelly sneaky sordid
stinking stormy crooked hollow bitter crafty heartless jealous lazy naughty peevish rancid savage
scornful selfish spiteful stubborn""".split()

PUNCT = ".?!"
# fixed words: text -> stress
FIXED = {"the": "0", "a": "0", "an": "0", "of": "0", "and": "0", "as": "0", "than": "0", "thy": "0", "is": "0",
         "Thou": "x", "thou": "x", "thee": "x", "art": "x", "Art": "x", "Is": "0",
         "Speak": "1", "mind": "1", "nothing": "10", "square": "1", "cube": "1", "good": "1", "worse": "1", "sum": "1", "twice": "1", "thyself": "01"}

class Tok:
    __slots__ = ("text", "stress", "kind", "extra")
    def __init__(self, text, stress=None, kind=None, extra=0.0):
        self.text, self.kind, self.extra = text, kind, extra
        self.stress = stress if stress is not None else FIXED[text.rstrip(PUNCT)]
    def __repr__(self): return self.text if self.kind is None else f"<{self.kind}>"

# slot kinds: N plain monosyllabic noun, N2 plain trochaic noun, B and B2 their insulting
# counterparts, A and BA trochaic adjectives (flattering / insulting)
SLOT_STRESS = {"N": "1", "B": "1", "N2": "10", "B2": "10", "A": "10", "BA": "10"}
def slot(kind, extra=0.0, punct=""): return Tok(kind + punct, SLOT_STRESS[kind], kind, extra)
def lit(*words): return [Tok(w) for w in words]

def load_pools():
    """From generated/Lexicon.inc: {kind: {rhyme key: [word, ...]}} for the slot kinds,
    {word: rhyme key} for line-ending fixed words, and the adjective lists."""
    lex = {}
    for line in open(os.path.join(ROOT, "generated", "Lexicon.inc")):
        m = re.match(r'\{"([a-z]+)",(\d+),(-?\d),"([^"]*)","([^"]*)"\},', line)
        if m: lex[m[1]] = (int(m[2]), int(m[3]), m[4], m[5].split("|")[0])
    def pick(words, flag, stress, sign):
        out = []
        for w in words:
            e = lex.get(w)
            if e and e[0] & flag and e[2] == stress and (e[1] >= 0 if sign > 0 else e[1] < 0 if sign < 0 else e[1] <= 0):
                if w not in out: out.append(w)
        return out
    pools = {"N": pick(COMMON, 1, "1", 1), "N2": pick(PLAIN2, 1, "10", 1), "B": pick(BAD1, 1, "1", -1), "B2": pick(BAD2, 1, "10", -1)}
    classes = {k: collections.defaultdict(list) for k in pools}
    for k, ws in pools.items():
        for w in ws: classes[k][lex[w][3]].append(w)
    enders = {w: lex[w][3] for w in ("thee", "mind", "good", "worse", "thou", "art", "sum", "twice", "square", "cube", "speak") if w in lex and lex[w][3]}
    adjs = {"A": pick(ADJ, 2, "10", 1), "BA": pick(BADADJ, 2, "10", 0)}
    return classes, enders, adjs

def term(k, neg):
    """alternatives (token lists) spelling +-2**k.  A trochaic adjective chain cannot
    cross a line boundary, so at most three adjectives are chained and larger powers are
    built from "twice", "the square of", "the cube of" and "the sum of"; the alternatives
    come in two parities, starting on a weak syllable ("a lovely cat", "the sum of")
    or a strong one ("lovely cat", "twice a cat")."""
    n, n2, a = ("B", "B2", "BA") if neg else ("N", "N2", "A")
    memo = {}
    def weak(k):
        if ("w", k) in memo: return memo["w", k]
        alts = []
        if k <= 3: alts += [lit("a") + [slot(a)] * k + [slot(n)], lit("a") + [slot(a)] * k + [slot(n2)]]
        if k >= 2 and k % 2 == 0 and not neg: alts += [lit("the", "square", "of") + t for t in strong(k // 2)]   # a square is never negative
        if k >= 3 and k % 3 == 0: alts += [lit("the", "cube", "of") + t for t in strong(k // 3)]
        if k >= 1: alts += [lit("the", "sum", "of") + t + lit("and") + u for t in strong(k - 1) for u in strong(k - 1)]
        memo["w", k] = trim(alts); return memo["w", k]
    def strong(k):
        if ("s", k) in memo: return memo["s", k]
        alts = []
        if k == 0: alts.append([slot(n, 0.5)])
        elif k <= 3: alts += [[slot(a)] * k + [slot(n)], [slot(a)] * k + [slot(n2)]]
        if k >= 1: alts += [[Tok("twice", extra=0.1)] + t for t in weak(k - 1)]
        memo["s", k] = trim(alts); return memo["s", k]
    def trim(alts):
        alts.sort(key=len)
        seen, kept = set(), []
        for t in alts:
            sig = tuple(x.stress for x in t)
            if sig in seen: continue
            seen.add(sig); kept.append(t)
            if len(kept) == 20: break
        return kept
    return weak(k) + strong(k)

def bits(n): return [k for k in range(n.bit_length()) if n >> k & 1]

MEMO = {}
def expr(v, neg, parity):
    """alternatives spelling +-v (v > 0) that start on a weak ("w") or strong ("s")
    syllable: a power of two is a term; anything else is "the sum of X and Y" (weak
    start) or "twice X" (strong start, v even).  Odd sums have no strong-start spelling,
    which the planner mends with a filler sentence."""
    key = (v, neg, parity)
    if key in MEMO: return MEMO[key]
    alts = []
    if v & (v - 1) == 0:
        k = v.bit_length() - 1
        alts = [t for t in term(k, neg) if (t[0].stress[0] == "0") == (parity == "w")]
    elif parity == "w":
        bs = bits(v)
        splits = set()
        for m in range(1, 1 << len(bs)):
            x = sum(1 << bs[i] for i in range(len(bs)) if m >> i & 1)
            if x != v: splits.add((x, v - x))
        for x, y in sorted(splits):
            for t in expr(x, neg, "s"):
                for u in expr(y, neg, "s"):
                    alts.append(lit("the", "sum", "of") + t + lit("and") + u)
    elif v % 2 == 0:
        alts = [[Tok("twice", extra=0.1)] + t for t in expr(v // 2, neg, "w")]
    alts.sort(key=len)
    seen, kept = set(), []
    for t in alts:
        sig = tuple(x.stress for x in t)
        if sig in seen: continue
        seen.add(sig); kept.append(t)
        if len(kept) == 60: break
    MEMO[key] = kept
    return kept

def forms(b, cur, first):
    """candidate sentences (lists of segments; a segment is a list of alternative token
    lists) that make the listener b; the last token of the sentence gets the full stop"""
    out = []
    if b == 0:
        out.append([[lit("Thou", "art", "nothing")]])
    else:
        out.append([[lit("Thou", "art")], expr(b, False, "w") + expr(b, False, "s")])
    if not first and b != cur:
        d = b - cur
        out.append([[lit("Thou", "art", "the", "sum", "of", "thee", "and")], expr(abs(d), d < 0, "s")])
        out.append([[lit("Thou", "art", "the", "sum", "of")], expr(abs(d), d < 0, "s"), [lit("and", "thee")]])
    return out

SPEAK = [Tok("Speak"), Tok("thy"), Tok("mind!")]
# Harmless sentences the planner may insert to mend the metre or move a line end onto a
# rhymable word: a question sets the condition flag, which nothing reads; "Thou art thee"
# assigns the listener to himself.
def fillers():
    f = [lit("Art", "thou", "as", "good", "as", "thee?"),
         lit("Art", "thou", "worse", "than", "thee?"),
         lit("Thou", "art", "thee."),
         lit("Thou", "art", "thyself."),
         lit("Thou", "art", "as", "good", "as", "thee."),
         lit("Is", "thee", "as", "good", "as", "thee?"),
         lit("Art", "thou", "as", "good", "as", "a") + [slot("N", punct="?")],
         lit("Art", "thou", "worse", "than", "a") + [slot("N", punct="?")],
         lit("Art", "thou", "as", "good", "as", "a") + [slot("A"), slot("N", punct="?")],
         lit("Art", "thou", "worse", "than", "a") + [slot("A"), slot("N", punct="?")],
         lit("Is", "a") + [slot("N")] + lit("as", "good", "as", "thee?"),
         lit("Is", "a") + [slot("N")] + lit("worse", "than", "thee?"),
         lit("Is", "thy") + [slot("N")] + lit("as", "good", "as", "thee?"),
         lit("Is", "a") + [slot("A"), slot("N")] + lit("worse", "than", "thee?")]
    for alt in f:
        for t in alt: t.extra += 0.03
    return [[]] + f

def syl(stress): return len(stress)

def walk(segs, states, ends_cost, mark_end):
    """Advance every (pos, parity, prev) state through the segments, keeping the cheapest
    token list per resulting state.  pos is the syllable position in the line (0-9); a
    word may only be placed where its stressed syllables fall on odd positions and its
    unstressed on even ones; a word reaching position 10 ends the line."""
    cur = states
    for seg in segs:
        nxt = {}
        for (pos, parity, prev), (cost, toks) in cur.items():
            for alt in seg:
                p, par, pr, c, ok = pos, parity, prev, cost, True
                for t in alt:
                    st = t.stress
                    n = syl(st)
                    if p + n > 10 and not (p == 9 and st == "10"): ok = False; break
                    for i, ch in enumerate(st):
                        want = "1" if (p + i) % 2 else "0"
                        if (p + i) == 10: want = "0"
                        if ch != "x" and ch != want: ok = False; break
                    if not ok: break
                    c += 0.02 * n + t.extra
                    p += n
                    if p >= 10:
                        k = mark_end(t)
                        if par == 1: c += ends_cost(pr, k)
                        pr, par, p = k, 1 - par, 0
                if not ok: continue
                key = (p, par, pr if par == 1 else None)
                if key not in nxt or c < nxt[key][0]: nxt[key] = (c, toks + list(alt))
        cur = nxt
    return cur

def layout(steps, classes, enders):
    """steps: list of (forms, run); choose for each a form and up to two fillers"""
    pairable = set()   # (kind or key, kind or key) pairs that some rhyme class can serve
    keys_of = collections.defaultdict(set)
    for k, cl in classes.items():
        for key, ws in cl.items(): keys_of[key].add((k, len(ws)))
    for key, ks in keys_of.items():
        for a, na in ks:
            for b, nb in ks:
                if a != b or na >= 2: pairable.add((a, b))
        for w, ek in enders.items():
            if ek == key:
                for a, _ in ks: pairable.add((a, ek)); pairable.add((ek, a))
    def ends_cost(a, b):
        if a is None or b is None: return 3.0          # no rhyme
        if (a, b) in pairable: return 0.0
        return 0.5 if a == b and a in enders.values() else 3.0   # the same word twice: a poor rhyme
    def mark_end(t):
        if t.kind in classes: return t.kind
        c = t.text.rstrip(PUNCT).lower()
        return enders.get(c)
    fill = fillers()
    best = {(0, 0, None): (0.0, [])}
    history = []
    for fs, run in steps:
        nxt = {}
        for (pos, parity, prev), (cost, toks) in cur.items():
            for alt in seg:
                p, par, pr, c, ok = pos, parity, prev, cost, True
                for t in alt:
                    st = t.stress
                    n = syl(st)
                    if p + n > 10 and not (p == 9 and st == "10"): ok = False; break
                    for i, ch in enumerate(st):
                        want = "1" if (p + i) % 2 else "0"
                        if (p + i) == 10: want = "0"
                        if ch != "x" and ch != want: ok = False; break
                    if not ok: break
                    c += 0.02 * n + t.extra
                    p += n
                    if p >= 10:
                        k = mark_end(t)
                        if par == 1: c += ends_cost(pr, k)
                        pr, par, p = k, 1 - par, 0
                if not ok: continue
                key = (p, par, pr if par == 1 else None)
                if key not in nxt or c < nxt[key][0]: nxt[key] = (c, toks + list(alt))
        cur = nxt
    return cur

def layout(steps, classes, enders):
    """steps: list of (forms, run); choose for each a form and up to two fillers"""
    pairable = set()   # (kind or key, kind or key) pairs that some rhyme class can serve
    keys_of = collections.defaultdict(set)
    for k, cl in classes.items():
        for key, ws in cl.items(): keys_of[key].add((k, len(ws)))
    for key, ks in keys_of.items():
        for a, na in ks:
            for b, nb in ks:
                if a != b or na >= 2: pairable.add((a, b))
        for w, ek in enders.items():
            if ek == key:
                for a, _ in ks: pairable.add((a, ek)); pairable.add((ek, a))
    def ends_cost(a, b):
        if a is None or b is None: return 3.0          # no rhyme
        if (a, b) in pairable: return 0.0
        return 0.5 if a == b and a in enders.values() else 3.0   # the same word twice: a poor rhyme
    def mark_end(t):
        if t.kind in classes: return t.kind
        c = t.text.rstrip(PUNCT).lower()
        return enders.get(c)
    fill = fillers()
    best = {(0, 0, None): (0.0, [])}
    history = []
    for fs, run in steps:
        merged = {}
        for f in fs:
            segs = list(f)
            if segs:
                res = walk(segs, {k: (0.0, []) for k in best}, ends_cost, mark_end)
                # the full stop goes on the last word of the sentence
                res = {k: (c, toks[:-1] + [Tok(toks[-1].text + ".", toks[-1].stress, toks[-1].kind)]) for k, (c, toks) in res.items()}
                # restart the walk from these states so that the punctuation is settled
                res = walk([[SPEAK] * run] if run else [], res, ends_cost, mark_end) if run else res
            else:
                res = walk([[SPEAK] * run], {k: (0.0, []) for k in best}, ends_cost, mark_end)
            res = walk([fill, fill], res, ends_cost, mark_end)
            for k, (c, toks) in res.items():
                for st0 in best:
                    pass
                if k not in merged or c < merged[k][0]: merged[k] = (c, toks)
        # merged holds, per end state, the best continuation from *some* start state; we
        # need it per start state, so redo the walk per start state (cheap: few states)
        nxt = {}
        for st0, (c0, _) in best.items():
            for f in fs:
                segs = list(f)
                res = {st0: (0.0, [])}
                if segs:
                    res = walk([fill, fill] + segs, res, ends_cost, mark_end)
                    res = {k: (c, toks[:-1] + [Tok(toks[-1].text + ".", toks[-1].stress, toks[-1].kind)]) for k, (c, toks) in res.items()}
                for _ in range(run): res = walk([fill, [SPEAK]], res, ends_cost, mark_end)
                res = walk([fill], res, ends_cost, mark_end)
                for k, (c, toks) in res.items():
                    if k not in nxt or c0 + c < nxt[k][0]: nxt[k] = (c0 + c, toks, st0)
        history.append(nxt)
        best = {k: (c, None) for k, (c, _, _) in nxt.items()}
    state = min(best, key=lambda k: best[k][0])
    chosen = []
    for nxt in reversed(history):
        cost, toks, prev = nxt[state]
        chosen.append(toks)
        state = prev
    chosen.reverse()
    return [t for toks in chosen for t in toks]

def verse(text, title, speaker, listener):
    classes, enders, adjs = load_pools()
    data = text.encode("utf-8")
    steps = []
    cur, first, j = 0, True, 0
    while j < len(data):
        b = data[j]
        run = 1
        while j + run < len(data) and data[j + run] == b: run += 1
        steps.append((forms(b, cur, first) if (b != cur or first) else [[]], run))
        cur, first = b, False
        j += run
    toks = [Tok(t.text, t.stress, t.kind, t.extra) for t in layout(steps, classes, enders)]  # the planner shares token objects
    # cut into lines
    lines, line, p = [], [], 0
    for t in toks:
        line.append(t); p += syl(t.stress)
        if p >= 10: lines.append(line); line, p = [], 0
    if line: lines.append(line)
    # rhymes
    used = collections.Counter()
    def fresh(kind, key):
        v = classes[kind][key]; w = v[used[kind, key] % len(v)]; used[kind, key] += 1; return w
    def endkey(t):
        if t.kind in classes: return None
        return enders.get(t.text.rstrip(PUNCT).lower())
    def put(t, w): t.text = w + t.text[len(t.kind):]; t.kind = None
    keys_for = collections.defaultdict(list)
    for k, cl in classes.items():
        for key in cl: keys_for[k].append(key)
    ci = collections.Counter()
    rhymed = 0
    for i in range(0, len(lines) - 1, 2):
        a, b = lines[i][-1], lines[i + 1][-1]
        if a.kind in classes and b.kind in classes:
            keys = [key for key in keys_for[a.kind] if key in classes[b.kind] and (a.kind != b.kind or len(classes[a.kind][key]) >= 2)]
            if not keys: continue
            key = keys[ci[a.kind, b.kind] % len(keys)]; ci[a.kind, b.kind] += 1
            put(a, fresh(a.kind, key)); put(b, fresh(b.kind, key)); rhymed += 1
        elif a.kind in classes and endkey(b) in classes[a.kind]: put(a, fresh(a.kind, endkey(b))); rhymed += 1
        elif b.kind in classes and endkey(a) in classes[b.kind]: put(b, fresh(b.kind, endkey(a))); rhymed += 1
        elif endkey(a) is not None and endkey(a) == endkey(b): rhymed += 1
    # the remaining slots, spread over the pools
    cycles = {k: itertools.cycle([w for key in sorted(cl) for w in cl[key]]) for k, cl in classes.items()}
    cycles.update({k: itertools.cycle(v) for k, v in adjs.items()})
    for line in lines:
        for t in line:
            if t.kind: put(t, next(cycles[t.kind]))
        for i in range(len(line) - 1):
            if line[i].text == "a" and line[i + 1].text[0] in "aeiou": line[i].text = "an"
    body = "\n".join(" " + " ".join(t.text for t in line) for line in lines)
    print(f"{len(lines)} lines, {rhymed} rhymed couplets of {len(lines) // 2}", file=sys.stderr)
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
