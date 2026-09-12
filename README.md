# splc: an LLVM front end for the Shakespeare Programming Language

`splc` compiles plays written in the [Shakespeare Programming Language](https://shakespearelang.com)
to native code through LLVM. Unlike the original, it understands a large English
vocabulary (171,534 words: all of CMUdict, WordNet nouns and adjectives, Shakespeare's
dramatis personae) rather than a few hundred hand-picked words, and it can check,
or insist, that every line of dialogue is in iambic pentameter and that scenes rhyme.

```
$ splc examples/hello_verse.spl -fpentameter=error -o hello && ./hello
Hello World!
$ splc examples/hello.spl
examples/hello.spl:13:2: warning: line does not scan as iambic pentameter (more than 12 syllables)
 You are as lovely as the sum of a big big big big big big cat and a big big big cat.
 ^
...
```

Source: https://github.com/bakkertj/splc

## Building

Requires CMake 3.20 or newer, a C++17 compiler, LLVM 18 or newer development files, and
Python 3 (only to regenerate the lexicon or the sonnet corpus).

```
git clone https://github.com/bakkertj/splc.git && cd spl
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # add -DLLVM_DIR=/path/to/lib/cmake/llvm if needed
cmake --build build
ctest --test-dir build
```

On macOS with Homebrew LLVM: `-DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm`, or run
`./build_mac.sh`, which configures against Homebrew's LLVM, builds into `build-mac/`, runs
the tests and the examples, and logs everything to `build_mac.log`.

Verified on Linux x86_64 with LLVM 18.1.3 and 20.1.2, and on macOS 26 (arm64) with Homebrew LLVM 20.1.8;
CI builds against Homebrew's current LLVM (23 at the time of writing), so the LLVM 21 API changes
are covered.
The build produces `splc` and the runtime library `libsplrt.a`; `splc` links finished plays
against the runtime with the system `cc`.

## Usage

```
splc [options] play.spl
  -o <file>                     output (default a.out; play.o with -c; play.ll with -emit-llvm)
  -c                            compile to an object file, do not link
  -emit-llvm                    write LLVM IR instead of an object
  -O0 -O1 -O2 -O3               optimisation level (default -O2)
  -fpentameter=off|warn|error   scansion check (default warn)
  -fpentameter-tolerance=N      metrical cost allowed per line (default 0)
  -fno-feminine-endings         forbid an 11th unstressed syllable
  -fno-initial-trochee          forbid an inverted first foot
  -fno-prose-exemption          scan low-born characters too (see Prose below)
  -fcouplets                    require every scene to end in a rhyming couplet
  -frhyme-scheme=SCHEME         require a rhyme scheme, e.g. AABB or "ABAB CDCD EFEF GG"
  -fnear-rhymes                 accept Elizabethan near-rhymes (come/doom, were/bear)
  -fsonnet                      every speech must be a sonnet: fourteen lines, ABAB CDCD EFEF GG
  -fsyntax-only                 parse and scan, produce nothing
  --scan                        print the scansion of every line of dialogue and exit
  --scan-text                   scan a plain text file (every line is verse) and exit
  --suggest-stress              for each failing line of a text file, print the one-word stress
                                changes that would make it scan (corpus mining)
  --lexicon-size                print the number of words in the lexicon and exit
  --version
  --runtime <dir>               where to find libsplrt.a (default: the build directory)
```

`-fpentameter=error` also turns couplet and rhyme-scheme failures into errors.

## The language in brief

A play is a title, a dramatis personae, and acts made of scenes. Characters are integer
variables; whoever is on stage with the speaker is *you*. Stage state is tracked at run
time, so a speech to an empty or crowded stage is a runtime error, not a compile error.

| sentence | meaning |
|---|---|
| `Thou art a big big cat.` / `You are as lovely as the sum of thyself and a cat.` | assignment to the addressee |
| `Speak thy mind!` / `Open thy heart!` | print the addressee's value as a character / as a number |
| `Open thy mind!` / `Listen to thy heart!` | read a character / a number into the addressee |
| `Am I better than thou?` / `Is X as bad as nothing?` | comparison; the answer is remembered |
| `If so, ...` / `If not, ...` | run the sentence that follows if the last answer was yes / no |
| `Let us return to scene II.` / `We shall proceed to act III.` | goto |
| `Remember thyself.` / `Recall thy former self.` | push onto / pop from the addressee's stack |
| `[Enter Romeo and Juliet]` `[Exit Romeo]` `[Exeunt]` | stage directions |

Values: `nothing` and `zero` are 0; a noun is 1 (or -1 if it is an insult) and each
adjective in front of it doubles it, so `a big big cat` is 4 and `a vile pig` is -2;
`me`/`myself`, `thou`/`thyself`/`you`, and character names read variables; `the sum of X
and Y`, `the difference between X and Y`, `the product of X and Y`, `the quotient between
X and Y`, `the remainder of the quotient between X and Y`, `the square of X`, `the cube of
X`, `the square root of X`, `the factorial of X`, and `twice X` are arithmetic.

The grammar is SPL 1.2.1 with these liberties:

* Any word WordNet calls a noun or adjective is one. Polarity (positive / negative / neutral)
  comes from the VADER sentiment lexicon, overridden by `data/overrides.tsv`. Neutral and
  positive nouns are 1, negative nouns -1, every adjective doubles. A flattering adjective
  on an insulting noun earns a warning, not an error.
* Character names are whatever the dramatis personae declares (multi-word names are fine);
  they need not be Shakespeare's.
* Comparatives are derived: `-er` forms of known adjectives, `more/less ADJ than`, and
  `better/worse/bigger/smaller...`. A neutral comparative with no size sense is an error.
* A sentence may open with a poetic connective (`And`, `But`, `O`, `Now`, `Then`, `Yet`).
* `Recall` ignores the rest of its sentence, as in the original.
* `[Prose]` and `[Verse]` are accepted as stage directions: every speech after `[Prose]` is
  exempt from scansion and rhyme checks until `[Verse]`, whoever speaks. They generate no code.

`examples/balcony.spl` is the language at its most literal: Juliet, addressing Romeo,
recites "O Romeo, Romeo! wherefore art thou Romeo?" and the rest of her balcony speech by
assigning him each letter in turn and telling him to speak his mind, 1,251 lines of prose
for 627 bytes of verse. `examples/balcony_verse.spl` prints the same 627 bytes in 1,789
lines of iambic pentameter rhyming in couplets, and passes
`-fpentameter=error -frhyme-scheme=AABB`. `tools/text_to_spl.py` made both and will do
the same for any text:

```
python3 tools/text_to_spl.py --title "A Greeting." --speaker Juliet --listener Romeo hello.txt > hello.spl
python3 tools/text_to_spl.py --verse --title "A Greeting." hello.txt > hello_verse.spl
```

The verse mode works because the generator only ever uses monosyllables (the listener is
`thee`, subtraction adds an insulting noun, questions such as `Art thou worse than a fox?`
are harmless padding), and every monosyllable is metrically flexible, so any ten words
scan. Rhyme is a dynamic programme over the spelling of each constant (direct or as a
delta, terms in any order, with or without padding) that lands each couplet's two line
ends on words that can rhyme: a noun slot takes any rhyme, and a fixed word such as
*thee*, *of* or *and* rhymes with a slot filled from its own class (*tree*, *love*,
*hand*), with the classes drawn from the compiler's lexicon.

## Layout

| path | what |
|---|---|
| `src/Lexer` | tokenises, keeping line structure for scansion; normalises `'d`, `e` with a grave accent, curly quotes, dashes, quotation marks |
| `src/Parser` | recursive descent over SPL's sentence frames; nouns, adjectives and names come from the lexicon |
| `src/Scansion` | pentameter check (dynamic programming over per-word stress options), couplets, rhyme schemes |
| `src/CodeGen` | LLVM IR via `IRBuilder`; acts and scenes are basic blocks; character values are a module global; the standard PassBuilder pipeline runs at the chosen `-O` level |
| `src/Lexicon` | binary search over the generated table; Elizabethan spelling normalisation; stress and rhyme lookups |
| `src/Diagnostics` | clang-style `file:line:col: warning:` output with the source line and a caret |
| `runtime/splrt.c` | stage tracking, stacks, I/O, checked arithmetic, runtime errors |
| `tools/gen_lexicon.py` | builds `generated/Lexicon.inc` from `data/` |
| `tools/sonnets_to_text.py` | turns the shakespeare.mit.edu sonnet pages into `shakespeare/sonnets.txt` and `sonnets_lines.txt` |
| `tools/mine_stress.py` | runs `--suggest-stress` over corpora and proposes `elizabethan_stress.tsv` lines with counts |
| `tools/play_to_lines.py` | extracts a play's lines (shakespeare.mit.edu HTML or plain text), classifying each speech as verse or prose |
| `tools/poem_to_stanzas.py` | turns a long-poem page into blank-line-separated stanzas |
| `tools/build_corpus.sh` | runs both over a shakespeare.mit.edu download to fill `shakespeare/plays/` and `shakespeare/poems/` |
| `tools/text_to_spl.py` | writes a play that prints a given text, in prose or (`--verse`) in rhymed iambic pentameter; how the balcony examples were made |
| `docs/writing-a-sonnet.md` | how `examples/sonnet.spl` was written, and how to write your own |
| `data/` | CMUdict, WordNet index files, VADER, `shakespeare_names.txt`, `name_stress.tsv` (596 Shakespearean names with their metrical stress), `elizabethan_stress.tsv` (words Shakespeare stressed differently), `overrides.tsv` |
| `generated/Lexicon.inc` | the word table compiled into `splc` (about 5.5 MB of source, about 2 MB in the binary) |
| `shakespeare/` | the 154 sonnets, Richard II, `plays/` with the verse and prose lines of all 37 plays (60,292 verse, 16,744 prose), and `poems/` with Venus and Adonis, Lucrece and A Lover's Complaint as stanzas; all used for calibration |
| `examples/` | `hello.spl` (prose), `hello_verse.spl` (strict pentameter), `primes.spl` (loops, I/O, stack), `fizzbuzz.spl`, `reverse.spl` (a string reversed through the stack), `couplets.spl` (couplets and a prose-speaking servant), `sonnet.spl` (two speakers, a sonnet each), `balcony.spl` and `balcony_verse.spl` (Juliet recites her balcony speech, in prose and in rhyming pentameter, both generated from `balcony.txt`) |
| `test/` | the shell helpers `ctest` uses (seventeen tests are declared in `CMakeLists.txt`) and `calibration.sh`, which CI runs to make sure the checker still tells verse from prose |
| `editors/vscode/` | a VS Code extension with syntax highlighting for `.spl` (see its README to install) |
| `.github/workflows/ci.yml` | GitHub Actions: build and test on Ubuntu (apt LLVM 18) and macOS (Homebrew LLVM), plus the calibration check |

## How a play is compiled

`main` holds one `alloca` for the last question's answer and a global array
`@characters` of `i64`, one slot per character. Every act and scene is a basic block, so a
goto is a branch and falling off the end of a scene branches to the next. Within a speech
the stage cannot change, so the addressee is computed once per speech with a call to
`spl_addressee` and reused. Runtime functions are declared with `nounwind` and precise
memory effects (stage bookkeeping and I/O are inaccessible memory; the arithmetic helpers
touch no memory) so that LLVM can fold constants straight into `spl_out_char` calls,
merge addressee lookups across arithmetic, and thread the branches of a loop. Compile with
`-emit-llvm` to see the result; Hello World becomes a straight line of stores and calls,
and `primes.spl` becomes a real CFG with phis.

The runtime is tiny C: `spl_init(n, names, values)` receives the name table and the
value array; `spl_enter`, `spl_exit`, `spl_exeunt_all` and `spl_addressee` keep the stage;
`spl_push`/`spl_pop` are the per-character stacks; `spl_out_*`/`spl_in_*` do I/O; and
`spl_div`, `spl_mod`, `spl_sqrt`, `spl_factorial` abort the play with a message on
division by zero, negative roots and the like.

## Scansion rules

Each word contributes its CMUdict stress pattern(s): primary stress is `1`, unstressed
`0`, and secondary stress is flexible. Shakespearean names take theirs from
`data/name_stress.tsv` (*Aumerle* 01, *Romeo* 100 or 10), and words whose stress has
moved since 1600 get both readings from `data/elizabethan_stress.tsv` (*aspect*,
*complete*, *revenue*, *welcome*, *therein*, *unknown*). Monosyllables and function
words are metrically flexible. Elizabethan variants are allowed automatically: `-ed` as
a full syllable (*determined* as four), `-ion` as two, `-est`/`-eth` as a syllable
(*vilest*, *presenteth*), syncope in *heaven, power, spirit, every, glorious, general,
dangerous, flattering*, the contractions `o'er`, `e'er`, `'gainst`, `'tis`, and cross-word
elisions (*th'expense*, *t'assist*, *I'm*, *thou'rt*, *we're*, *'tis*, *i'th'*). Write
the word with a grave accent, as editors do (*blessèd*), to force the extra syllable.

A line scans if some choice of variants yields 10 syllables (11 with a feminine ending)
at a metrical cost of at most `tolerance` (default 0). The cost model is the prosodist's:
a stressed syllable in a weak position costs 1; an unstressed syllable in a strong
position is *promoted* and costs nothing; the first foot may be inverted, and so may a
foot that opens a new phrase after punctuation (*Admit impediments. Love is not love*).
A short line opening or closing a speech is treated as a shared line and not checked.

**Prose.** Shakespeare's nobles speak verse and his servants, clowns and fools speak
prose. `splc` reads each character's station from the dramatis personae: a description
containing *servant, clown, fool, porter, nurse, gravedigger, peasant, shepherd, tapster,
citizen, rogue...* (or the word *prose*) exempts that character from scansion; the word
*verse* overrides. `--scan` marks such lines `prose`; `-fno-prose-exemption` scans everyone.
A `[Prose]` stage direction does the same for a stretch of a scene, and `[Verse]` ends it.

**Couplets.** With `-fcouplets`, every scene must end in a rhyming couplet, meaning the
last two lines of dialogue, whoever speaks them. Rhymes are compared on CMU phones from
the last stressed vowel; an identical word rhymes (the bard allows it), and a spelling
rhyme is accepted for eye-rhymes and shifted vowels (*love/move*). Elizabethan latitude is
built in: the final syllable may carry the rhyme whatever the stress (*thee/posterity*),
and voicing is ignored (*is/amiss*).

**Rhyme schemes.** `-frhyme-scheme=SCHEME` checks each scene's verse lines against a
pattern that repeats: `AABB` for couplets throughout, `"ABAB CDCD EFEF GG"` for sonnets.
With `--scan-text` the scheme is applied to each blank-line-separated stanza of the file.
`-fnear-rhymes` pools the vowels Elizabethan ears let rhyme (*come/doom*, *wrong/young*,
*were/bear*, *past/waste*, *die/memory*) while still requiring the same consonants.

**Sonnets.** `-fsonnet` requires every verse speech to be a sonnet: fourteen lines rhyming
ABAB CDCD EFEF GG, and scanning if `-fpentameter` is on. `examples/sonnet.spl` is a
program of two sonnets, one per speaker; Juliet's prints `Hi!` and Romeo's answers `Ho!`
(`docs/writing-a-sonnet.md` explains how it was written):

```
$ splc -fsonnet -fpentameter=error examples/sonnet.spl -o hi && ./hi
Hi!
Ho!
```

## Calibration

Everything under `shakespeare/` comes from the shakespeare.mit.edu edition via the tools
above: `sonnets_lines.txt` (154 sonnets, 2,155 lines), `plays/<play>_verse.txt` and
`_prose.txt` for all 37 plays, and `poems/` for the three long poems as stanzas. The
edition does not mark prose, but it wraps prose at a fixed width so continuation lines
begin in lower case, while every verse line begins with a capital; `play_to_lines.py`
classifies each speech that way. The prose is therefore a real control: Shakespeare's
own sentences, cut by a typesetter into lines of roughly ten syllables.

Metre, share of lines that scan at tolerance 0:

| corpus | lines | scan |
|---|---|---|
| Sonnets | 2,155 | 93% |
| All plays, verse speeches | 60,292 | 86% |
| All plays, prose speeches (control) | 16,744 | 34% |
| Sonnets, words shuffled (control) | 2,155 | 59% |

Tolerance 0 is the default because it is where the checker still tells verse from prose
and from shuffled verse; with tolerance 1 the shuffled control passes 91%. Most of
Shakespeare's own misses are syncopes no rule covers, lines the editors joined, or the
irregular lines he simply wrote. The default mode is therefore `warn`; `-fpentameter=error`
is for the purist.

Ranked by play (`shakespeare/plays/CALIBRATION.txt` has all 37, with tolerance 0 and 1),
the verse pass rate follows the chronology any editor would give: the early histories,
Romeo and Juliet, King John and Julius Caesar score 91 to 93%, and the late plays, whose
verse is looser, trail (Tempest 79%, Coriolanus 78%, Timon 77%, Pericles 71%). Raising the
tolerance to 1 lifts every play by four or five points and leaves the order alone, so the
late looseness is not a matter of a stray stress; it is structural. Prose sits between 29%
and 42% for every play with more than a hundred prose lines.

The same table gives the share of scanning lines that carry a feminine ending, which is the
oldest metrical test of Shakespeare's chronology (Spedding, 1850), and the checker
reproduces it without being told the dates:

| play | feminine endings |
|---|---|
| King John, 1 Henry IV, Love's Labour's Lost | 7 to 9% |
| Romeo and Juliet, Titus, Richard II, Midsummer | 10 to 11% |
| Hamlet, Twelfth Night, Troilus | 22 to 23% |
| Lear, Cymbeline, Tempest, Winter's Tale | 29 to 32% |
| Henry VIII | 44% |

Henry VIII stands alone because half of it is Fletcher's, whose verse runs to feminine
endings far more than Shakespeare's; that figure is the evidence Spedding used to say so.

Rhyme, with `--scan-text -frhyme-scheme=...` over blank-line-separated stanzas:

| poem | scheme | rhyme pairs | strict | `-fnear-rhymes` |
|---|---|---|---|---|
| Sonnets | ABAB CDCD EFEF GG | 1,078 | 93% | 98% |
| Venus and Adonis | ABABCC | 597 | 92% | 97% |
| The Rape of Lucrece | ABABBCC | 1,060 | 91% | 96% |
| A Lover's Complaint | ABABBCC | 188 | 94% | 97% |

```
grep -v '^Sonnet' shakespeare/sonnets.txt | splc --scan-text "-frhyme-scheme=ABAB CDCD EFEF GG" /dev/stdin
splc --scan-text -frhyme-scheme=ABABBCC shakespeare/poems/lucrece.txt
```

The strict misses are Shakespeare's own near-rhymes (*come/doom*, *tongue/wrong*). The
sonnet lines with their words shuffled produce 1,004 violations (974 with `-fnear-rhymes`).

`--suggest-stress` is how `data/elizabethan_stress.tsv` was seeded: run over a corpus it
lists, for every failing line, the single words whose stress would have to move for the
line to scan; the words that recur (*antique*, *therein*, *welcome*) are real Elizabethan
stress, the rest are mid-line trochees. `tools/mine_stress.py corpus.txt ...` wraps it:
it counts the suggestions, drops readings the table already has and the suffix-stressed
noise, and prints candidate `.tsv` lines with their counts for you to paste in.

## Regenerating the lexicon and the corpus

```
python3 tools/gen_lexicon.py         # or: cmake --build build --target lexicon
python3 tools/sonnets_to_text.py shakespeare/shakespeare.mit.edu/Poetry shakespeare
sh tools/build_corpus.sh shakespeare/shakespeare.mit.edu     # plays/ and poems/
```

Edit `data/overrides.tsv` (word, flags, polarity, stress) to correct a word, or
`data/name_stress.tsv` (name, stress) to add a name; both win over CMUdict. The lexicon
generator prints a summary of what it built.

## Licence

`splc` is free software under the GNU General Public License, version 3 or later; see
`LICENSE`. The dictionaries it is built from (CMUdict, WordNet, VADER) are bundled under
their own permissive licences and the Shakespeare text is public domain; the details are
in `THIRD_PARTY_LICENSES.md`.
