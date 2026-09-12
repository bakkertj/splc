# splc 0.1.1

A small release on top of 0.1.0: the compiler builds and passes its tests on every
LLVM from 18 to 23, and the verse generator now writes iambic pentameter that reads as
such, not only one that passes the checker.

## Changes

* `tools/text_to_spl.py --verse` plans a speech syllable by syllable from the dictionary
  stress of every word (function words weak, nouns strong, adjectives and two-syllable
  nouns trochaic), keeps the stress strictly alternating through the whole speech and
  never splits a word across a line. Powers of two are spelled with trochaic adjectives
  (`a lovely golden cat`) and larger values with `twice`, `the square of`, `the cube of`
  and `the sum of` as the position in the line allows. The old mode relied on the
  checker's rule that a monosyllable may fall in either position, which let lines like
  `a big big big big cat` through.
* `examples/balcony_verse.spl` is regenerated: 2,166 lines, all 1,083 couplets rhymed,
  passes `-fpentameter=error -frhyme-scheme=AABB`, output unchanged.
* Code generation no longer relies on `BasicBlock::getTerminator()` to detect an empty
  block, which on LLVM 23 left the blocks after a `goto` inside an `If` unterminated and
  failed module verification. The LLVM 21 and later target API changes (Triple-typed
  lookups) are handled as well.
* Release notes and README refreshed; `docs/releasing.md` describes how a release is cut.

## Building

CMake 3.20 or newer, a C++17 compiler and LLVM 18 or newer. CI runs the seventeen tests
and a calibration check on Ubuntu (apt LLVM 18) and macOS (Homebrew LLVM, currently 23).

## Licence

GPL-3.0-or-later; see `LICENSE` and `THIRD_PARTY_LICENSES.md`.
