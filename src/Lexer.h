// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Trevor Bakker
// Lexer.h - tokenises SPL source while preserving line structure.
#pragma once
#include <string>
#include <vector>

#include "Diagnostics.h"

namespace spl {

enum class Tok { Word, Punct, End };

struct Token {
  Tok kind;
  std::string text;   // word as written (may contain ' - è), or the single punctuation char
  std::string lower;  // normalised lower-case form (words only)
  Loc loc;
  bool graveAccent = false;  // contained è/é (metrical -èd)
};

class Lexer {
 public:
  Lexer(const SourceFile &src, Diagnostics &diag, bool lenient = false);
  std::vector<Token> tokenize();

 private:
  const SourceFile &src_;
  Diagnostics &diag_;
  bool lenient_;  // plain text, not a play: digits and odd characters are skipped silently
};

}  // namespace spl
