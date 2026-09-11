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
  Lexer(const SourceFile &src, Diagnostics &diag);
  std::vector<Token> tokenize();

 private:
  const SourceFile &src_;
  Diagnostics &diag_;
};

}  // namespace spl
