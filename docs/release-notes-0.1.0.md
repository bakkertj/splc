# splc 0.1.0

First release of `splc`, an LLVM front end for the Shakespeare Programming Language that
compiles plays to native code and can insist that they be written in verse.

## What it is

The Shakespeare Programming Language (Hasselström and Wiberg, 2001) is an esoteric
language in which programs are plays: characters are variables, dialogue is code, and
"Thou art the sum of thyself and a cat" is an increment. The original implementation
recognises a few hundred hand-picked words. `splc` recognises 171,534, generated from
CMUdict, WordNet and the VADER sentiment lexicon, so that any English noun or adjective
works and any word's polarity (a cat is 1, a pig is -1) comes from data rather than a
list. It emits LLVM IR, runs the standard optimisation pipeline, and links a small C
runtime that tracks who is on stage.

Because the lexicon also carries stress and pronunciation, the compiler can check the
poetry. `-fpentameter` scans every line of dialogue as iambic pentameter, with the
Elizabethan latitude a real editor allows: promoted syllables, inverted first feet and
feet after punctuation, feminine endings, syncope, elision across words, the period stress
of words like *aspect* and *revenue*, and the metrical stress of 596 Shakespearean names.
`-fcouplets`, `-frhyme-scheme` and `-fnear-rhymes` check rhyme, and `-fsonnet` requires
every speech to be a sonnet. `examples/sonnet.spl` is a program of two sonnets that prints
`Hi!` and `Ho!` and passes `-fsonnet -fpentameter=error`.

## Calibrated on the complete works

The checkers were tuned and measured against Shakespeare himself. The repository carries,
as plain text extracted from the shakespeare.mit.edu edition, all 154 sonnets, the verse
and prose speeches of all 37 plays (60,292 and 16,744 lines), and the three long poems as
stanzas.

At the default tolerance, 93% of sonnet lines and 86% of the plays' verse lines scan,
against 34% of the prose lines and 59% of sonnet lines with their words shuffled. The
rhyme checker recognises 93% of the sonnets' 1,078 rhyme pairs strictly and 98% with
`-fnear-rhymes`, with the same figures on Venus and Adonis and Lucrece, which it was not
tuned on.

Two results fell out of the calibration that are worth a look on their own. Ranking the
plays by how much of their verse scans reproduces the accepted chronology: the early
histories, Romeo and Juliet and King John at the top, The Tempest, Coriolanus and
Pericles at the bottom. And the share of lines with a feminine ending, which the checker
counts as a by-product, reproduces Spedding's 1850 test: 7 to 11% in the early plays, 29
to 32% in the late romances, and 44% in Henry VIII, the number Spedding used to argue that
half of that play is Fletcher's.

## Tools

* `--scan` prints the scansion of every line of a play; `--scan-text` scans any text file;
  `--suggest-stress` and `tools/mine_stress.py` mine a corpus for pronunciations the
  lexicon lacks.
* `tools/build_corpus.sh` rebuilds the corpora from a shakespeare.mit.edu download, and
  `tools/gen_lexicon.py` rebuilds the lexicon from `data/`.
* `editors/vscode/` provides syntax highlighting for `.spl`.
* `docs/writing-a-sonnet.md` explains how to write a program that is a sonnet.

## Building

CMake 3.20 or newer, a C++17 compiler and LLVM 18 or newer. Verified on Linux x86_64 with
LLVM 18.1.3 and on macOS (Apple silicon) with Homebrew LLVM 20.1.8; `build_mac.sh` does the
Homebrew configuration for you. CI runs the thirteen tests and a calibration check on both
platforms.

## Licence

GPL-3.0-or-later. The bundled dictionaries are under their own permissive licences
(CMUdict BSD, WordNet Princeton, VADER MIT) and the Shakespeare text is public domain;
see `THIRD_PARTY_LICENSES.md`.
