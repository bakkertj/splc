#!/usr/bin/env python3
"""Generate generated/Lexicon.inc for splc.

Sources (all in data/):
  cmudict.dict          CMU Pronouncing Dictionary  -> syllable/stress options
  index.noun, index.adj WordNet 3.0 index files     -> part of speech
  vader_lexicon.txt     VADER sentiment lexicon     -> noun/adjective polarity
  shakespeare_names.txt one name per line           -> character names
  name_stress.tsv       name<TAB>stress             -> metrical stress of names (wins over CMUdict)
  overrides.tsv         word<TAB>flags<TAB>polarity<TAB>stress  (hand-maintained; wins)
  elizabethan_stress.tsv word<TAB>extra stress options          (added to CMUdict's, not replacing)

Entry layout emitted:  { "word", flags, polarity, "stress1|stress2", "rhyme1|rhyme2" }
  rhyme: CMU phones from the last stressed vowel to the end, e.g. "ayt" for night/bright.
  flags: NOUN=1 ADJ=2 NAME=4 FUNCTION=8 (metrically flexible function word)
  stress strings: '1' stressed, '0' unstressed, one char per syllable.
"""
import re, sys, os, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "data")
OUT = os.path.join(ROOT, "generated", "Lexicon.inc")

NOUN, ADJ, NAME, FUNCTION = 1, 2, 4, 8

FUNCTION_WORDS = set("""a an the and or but nor of to in on at by for with from as if than then so no not
yet is are was were be been am do does did has have had can may will shall would should could must
i you he she it we they me him her us them my your thy thine his its our their this that these those
there here what who whom which when where how why all some any each o oh ah up out off into unto
upon over under till until ere thou thee ye art dost doth hath wilt shalt canst""".split())

def rhyme_key(phones):
    """Phones from the last primary-stressed vowel (else last vowel) to the end, stress stripped."""
    idx = [i for i, p in enumerate(phones) if p[-1:] == "1"] or [i for i, p in enumerate(phones) if p[-1:].isdigit()]
    if not idx: return ""
    return "".join(p.rstrip("012") for p in phones[idx[-1]:]).lower()

def load_cmu():
    stress = collections.defaultdict(list)
    rhymes = collections.defaultdict(list)
    with open(os.path.join(DATA, "cmudict.dict"), encoding="latin-1") as f:
        for line in f:
            if not line.strip() or line.startswith(";;;"): continue
            parts = line.split()
            w = re.sub(r"\(\d+\)$", "", parts[0]).lower()
            if not re.fullmatch(r"[a-z][a-z'\-]*", w): continue
            phones = [p for p in parts[1:] if not p.startswith("#")]
            s = "".join(ch for ph in phones for ch in ph if ch.isdigit()).replace("2", "x")  # secondary stress may fill either position
            if s and s not in stress[w]: stress[w].append(s)
            r = rhyme_key(phones)
            if r and r not in rhymes[w]: rhymes[w].append(r)
    return stress, rhymes

def load_wordnet(fname):
    words = set()
    with open(os.path.join(DATA, fname), encoding="latin-1") as f:
        for line in f:
            if line.startswith(" "): continue
            w = line.split()[0]
            if re.fullmatch(r"[a-z][a-z'\-]*", w): words.add(w)
    return words

def load_vader():
    pol = {}
    with open(os.path.join(DATA, "vader_lexicon.txt"), encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 2 or not re.fullmatch(r"[a-z][a-z'\-]*", parts[0]): continue
            v = float(parts[1])
            pol[parts[0]] = 1 if v >= 0.5 else (-1 if v <= -0.5 else 0)
    return pol

def load_names():
    """name -> stress options (may be empty string if unknown)."""
    names = {}
    p = os.path.join(DATA, "shakespeare_names.txt")
    if os.path.exists(p):
        for l in open(p):
            if l.strip(): names[l.strip().lower()] = ""
    p = os.path.join(DATA, "name_stress.tsv")
    if os.path.exists(p):
        for l in open(p):
            if not l.strip() or l.startswith("#"): continue
            n, st = (l.rstrip("\n").split("\t") + [""])[:2]
            names[n.lower()] = st.strip()
    return names

def load_elizabethan():
    """word -> extra stress options to add alongside CMUdict's."""
    extra = {}
    p = os.path.join(DATA, "elizabethan_stress.tsv")
    if os.path.exists(p):
        for l in open(p):
            if not l.strip() or l.startswith("#"): continue
            w, st = (l.rstrip("\n").split("\t") + [""])[:2]
            if st.strip(): extra[w.lower()] = st.strip()
    return extra

def load_overrides():
    ov = {}
    p = os.path.join(DATA, "overrides.tsv")
    if not os.path.exists(p): return ov
    for line in open(p):
        if not line.strip() or line.startswith("#"): continue
        w, flags, pol, stress = (line.rstrip("\n").split("\t") + ["", "", ""])[:4]
        ov[w.lower()] = (flags, pol, stress)
    return ov

def heuristic_stress(w):
    """Very rough fallback: one syllable per vowel group, all flexible."""
    n = len(re.findall(r"[aeiouy]+", w)) or 1
    if w.endswith("e") and n > 1 and not w.endswith(("le", "ee", "ye")): n -= 1
    return "x" * n

def main():
    stress, rhymes = load_cmu()
    nouns = load_wordnet("index.noun")
    adjs = load_wordnet("index.adj")
    pol = load_vader()
    names = load_names()
    ov = load_overrides()
    eliz = load_elizabethan()

    words = set(stress) | nouns | adjs | set(pol) | set(names) | set(ov) | set(eliz) | FUNCTION_WORDS
    entries = []
    for w in sorted(words):
        flags = (NOUN if w in nouns else 0) | (ADJ if w in adjs else 0) | (NAME if w in names else 0) \
                | (FUNCTION if w in FUNCTION_WORDS else 0)
        p = pol.get(w, 0)
        st = "|".join(stress[w]) if w in stress else heuristic_stress(w)
        if w in names and names[w]: st = names[w]   # the verse's own pronunciation of a name
        if w in eliz:  # Elizabethan stress shifts are alternatives, not replacements
            have = st.split("|")
            st = "|".join(have + [o for o in eliz[w].split("|") if o not in have])
        if w in ov:
            f2, p2, s2 = ov[w]
            if f2:
                flags = 0
                for tok in f2.split(","):
                    flags |= {"noun": NOUN, "adj": ADJ, "name": NAME, "function": FUNCTION}[tok.strip()]
            if p2: p = int(p2)
            if s2: st = s2
        entries.append((w, flags, p, st, "|".join(rhymes.get(w, []))))

    with open(OUT, "w") as out:
        out.write("// GENERATED by tools/gen_lexicon.py -- do not edit. %d entries.\n" % len(entries))
        out.write("// Sources: CMUdict, WordNet 3.0, VADER, data/shakespeare_names.txt, data/overrides.tsv\n")
        for w, f, p, s, r in entries:
            out.write('{"%s",%d,%d,"%s","%s"},\n' % (w.replace('\\', '\\\\').replace('"', '\\"'), f, p, s, r))
    n_noun = sum(1 for e in entries if e[1] & NOUN); n_adj = sum(1 for e in entries if e[1] & ADJ)
    n_pos = sum(1 for e in entries if e[2] > 0); n_neg = sum(1 for e in entries if e[2] < 0)
    print(f"wrote {OUT}: {len(entries):,} entries ({os.path.getsize(OUT)/1e6:.1f} MB source); "
          f"nouns={n_noun:,} adjs={n_adj:,} names={len(names):,} positive={n_pos:,} negative={n_neg:,}")

if __name__ == "__main__":
    main()
