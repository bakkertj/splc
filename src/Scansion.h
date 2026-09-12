// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Trevor Bakker
// Scansion.h - iambic pentameter checking of dialogue lines.
#pragma once
#include <string>
#include <vector>

#include "AST.h"
#include "Diagnostics.h"
#include "Lexer.h"

namespace spl {

struct ScansionOptions {
  enum Mode { Off, Warn, Error } mode = Warn;
  int tolerance = 0;        // stressed syllables allowed in weak positions
  int minWords = 3;         // shorter lines (shared lines, "Ay.") are not checked
  bool allowFeminine = true;
  bool allowInitialTrochee = true;
  bool proseExemption = true;   // low-born characters may speak prose
  bool couplets = false;        // warn when a scene does not end in a rhyming couplet
  bool nearRhymes = false;      // pool Elizabethan vowel classes when comparing rhymes
  bool sonnets = false;         // every verse speech must be a sonnet: 14 lines, ABAB CDCD EFEF GG
  std::string rhymeScheme;      // e.g. "AABB" or "ABAB CDCD EFEF GG": applied to each scene's verse lines, repeating
};

struct ScansionResult {
  bool scans = true;
  bool unknownWords = false;
  int syllables = 0;
  int mismatches = 0;
  std::string pattern;      // best-fit stress pattern, e.g. "0101010101"
  std::string explanation;
};

class Scansion {
 public:
  Scansion(const std::vector<Token> &toks, Diagnostics &diag, const ScansionOptions &opts)
      : t_(toks), diag_(diag), opts_(opts) {}
  // Returns the number of lines that failed.
  int check(const Program &prog);
  int checkCouplets(const Program &prog);
  // Check a run of verse lines against a rhyme scheme; blocks of the scheme's length repeat.
  int checkRhymeScheme(const std::vector<const DialogueLine *> &lines, const std::string &scheme);
  int checkRhymeScheme(const Program &prog);
  int checkSonnets(const Program &prog);
  // The verse lines of a scene (prose speeches and [Prose] sections skipped), in order.
  std::vector<const DialogueLine *> verseLines(const Program &prog, const Scene &s) const;
  ScansionResult scanLine(const DialogueLine &line) const;
  // For a line that does not scan: which single word, given a different stress pattern,
  // would make it scan?  Returns "word=pattern" strings.  Used to mine Elizabethan
  // stress shifts from a corpus (--suggest-stress).
  std::vector<std::string> suggestStress(const DialogueLine &line) const;

 private:
  const std::vector<Token> &t_;
  Diagnostics &diag_;
  ScansionOptions opts_;
};

}  // namespace spl
