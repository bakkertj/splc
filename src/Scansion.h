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
  int tolerance = 1;        // stressed syllables allowed in weak positions (and vice versa)
  int minWords = 3;         // shorter lines (shared lines, "Ay.") are not checked
  bool allowFeminine = true;
  bool allowInitialTrochee = true;
  bool proseExemption = true;   // low-born characters may speak prose
  bool couplets = false;        // warn when a scene does not end in a rhyming couplet
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
  ScansionResult scanLine(const DialogueLine &line) const;

 private:
  const std::vector<Token> &t_;
  Diagnostics &diag_;
  ScansionOptions opts_;
};

}  // namespace spl
