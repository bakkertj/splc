# splc: an LLVM front end for the Shakespeare Programming Language

`splc` compiles plays written in the [Shakespeare Programming Language](https://shakespearelang.com)
to native code through LLVM. Unlike the original, it understands a large English
vocabulary (171,534 words: all of CMUdict, WordNet nouns and adjectives, Shakespeare's
dramatis personae) rather than a few hundred hand-picked words, and it can check,
or insist, that every line of dialogue is in iambic pentameter.

```
$ splc examples/hello_verse.spl -fpentameter=error -o hello && ./hello
Hello World!
$ splc examples/hello.spl
examples/hello.spl:13:2: warning: line does not scan as iambic pentameter (more than 12 syllables)
 You are as lovely as the sum of a big big big big big big cat and a big big big cat.
 ^
...
```

## Building

Requires CMake ≥ 3.20, a C++17 compiler, LLVM ≥ 18 development files, and Python 3 (only
to regenerate the lexicon).

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # add -DLLVM_DIR=/path/to/lib/cmake/llvm if needed
cmake --build build
ctest --test-dir build
```

On macOS with Homebrew LLVM: `-DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm`.

## Usage

```
splc [options] play.spl
  -o <file>                     output (default a.out; play.o with -c; play.ll with -emit-llvm)
  -c / -emit-llvm               stop at an object file / LLVM IR
  -O0 -O1 -O2 -O3               optimisation level (default -O2)
  -fpentameter=off|warn|error   scansion check (default warn)
  -fpentameter-tolerance=N      metrical cost allowed per line (default 0)
  -fno-feminine-endings         forbid an 11th unstressed syllable
  -fno-initial-trochee          forbid an inverted first foot
  -fno-prose-exemption          scan low-born characters too (see below)
  -fcouplets                    require every scene to end in a rhyming couplet
  -fsyntax-only                 parse and scan only
  --scan                        print the scansion of every line of dialogue
  --scan-text                   scan any text file, one verse line per line
  --lexicon-size
```

## Layout

| path | what |
|---|---|
| `tools/gen_lexicon.py` | builds `generated/Lexicon.inc` from `data/` |
| `data/` | CMUdict, WordNet index files, VADER, `name_stress.tsv` (596 Shakespearean names with their metrical stress), `overrides.tsv` |
| `generated/Lexicon.inc` | the word table compiled into `splc` (~5.5 MB of source, about 2 MB in the binary) |
| `src/Lexer` | tokenises, keeping line structure for scansion; normalises `'d`, `è`, curly quotes, dashes |
| `src/Parser` | recursive descent over SPL's sentence frames; nouns/adjectives/names come from the lexicon |
| `src/Scansion` | dynamic-programming pentameter check with Elizabethan syllable rules |
| `src/CodeGen` | LLVM IR via `IRBuilder`; acts and scenes are basic blocks; character values are a module global so the standard `-O2` pipeline folds and threads them; stage state is a runtime concern |
| `runtime/splrt.c` | characters, stacks, stage tracking, I/O, checked arithmetic |
| `examples/` | `hello.spl` (prose), `hello_verse.spl` (strict pentameter), `primes.spl` (loops, I/O, stack), `couplets.spl` (couplets and a prose-speaking servant) |

## Language notes

The grammar is SPL 1.2.1 with these liberties:

* Any word WordNet calls a noun or adjective is one. Polarity (positive / negative / neutral)
  comes from the VADER sentiment lexicon, overridden by `data/overrides.tsv`. Neutral and
  positive nouns are 1, negative nouns −1, every adjective doubles. A flattering adjective
  on an insulting noun earns a warning, not an error.
* Character names are whatever the dramatis personae declares (multi-word names are fine);
  they need not be Shakespeare's.
* Comparatives are derived: `-er` forms of known adjectives, `more/less ADJ than`, and
  `better/worse/bigger/smaller…`. A neutral comparative with no size sense is an error.
* A sentence may open with a poetic connective (`And`, `But`, `O`, `Now`, `Then`, `Yet`).
* `Recall` ignores the rest of its sentence, as in the original.

## Scansion rules

Each word contributes its CMUdict stress pattern(s): primary stress is `1`, unstressed
`0`, and secondary stress is flexible. Shakespearean names take theirs from
`data/name_stress.tsv` (*Aumerle* 01, *Romeo* 100 or 10). Monosyllables and function
words are metrically flexible. Elizabethan variants are allowed automatically: `-ed` as
a full syllable (*determinèd*), `-ion` as two, `-est`/`-eth` as a syllable (*vilest*,
*presenteth*), syncope in *heaven, power, spirit, every, glorious, general, dangerous,
flattering*, the contractions `o'er`, `e'er`, `'gainst`, `'tis`, and cross-word elisions
(*th'expense*, *t'assist*, *I'm*, *thou'rt*, *we're*, *'tis*, *i'th'*). Write `blessèd`
to force the extra syllable.

A line scans if some choice of variants yields 10 syllables (11 with a feminine ending)
at a metrical cost of at most `tolerance` (default 0). The cost model is the prosodist's:
a stressed syllable in a weak position costs 1; an unstressed syllable in a strong
position is *promoted* and costs nothing; the first foot may be inverted, and so may a
foot that opens a new phrase after punctuation (*Admit impediments. Love is not love*).
A short line opening or closing a speech is treated as a shared line and not checked.

**Prose.** Shakespeare's nobles speak verse and his servants, clowns and fools speak
prose. `splc` reads each character's station from the dramatis personae: a description
containing *servant, clown, fool, porter, nurse, gravedigger, peasant, shepherd, tapster,
citizen, rogue…* (or the word *prose*) exempts that character from scansion; the word
*verse* overrides. `--scan` marks such lines `prose`; `-fno-prose-exemption` scans everyone.

**Couplets.** With `-fcouplets`, every scene must end in a rhyming couplet, meaning the last two
lines of dialogue, whoever speaks them. Rhymes are compared on CMU phones from the last
stressed vowel; an identical word rhymes (the bard allows it), and a spelling rhyme is
accepted for eye-rhymes and shifted vowels (*love/move*).

### Calibration

`shakespeare/sonnets_lines.txt` (all 154 sonnets, 2,155 lines, made from the
shakespeare.mit.edu pages by `tools/sonnets_to_text.py`) and Richard II (entirely verse,
2,606 lines of six or more words) are the reference corpora; the controls are the same
sonnet lines with their words shuffled, and Hamlet's prose wrapped to ten-ish syllables.

| corpus | tolerance 0 | tolerance 1 |
|---|---|---|
| Sonnets | 93% | 97% |
| Richard II | 89% | 94% |
| Sonnets, words shuffled (control) | 58% | 91% |
| Hamlet prose (control) | 12% | 41% |

Tolerance 0 is the default because it is where the checker still tells verse from
shuffled verse; most of Shakespeare's own misses are lines the editors joined, syncopes
no rule covers, or the irregular lines he simply wrote. The default mode is therefore
`warn`; `-fpentameter=error` is for the purist.

## Regenerating the lexicon

```
python3 tools/gen_lexicon.py         # or: cmake --build build --target lexicon
```

Edit `data/overrides.tsv` (word, flags, polarity, stress) to correct a word.
