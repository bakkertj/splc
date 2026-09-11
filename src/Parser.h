// Parser.h - recursive-descent parser for the Shakespeare Programming Language.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "AST.h"
#include "Diagnostics.h"
#include "Lexer.h"

namespace spl {

class Parser {
 public:
  Parser(const std::vector<Token> &toks, Diagnostics &diag) : t_(toks), diag_(diag) {}
  Program parse();

 private:
  // token helpers
  const Token &cur() const { return t_[pos_]; }
  const Token &peek(int n = 1) const { return t_[std::min(pos_ + n, t_.size() - 1)]; }
  bool atEnd() const { return cur().kind == Tok::End; }
  bool isWord(const char *w, int n = 0) const;
  bool isPunct(char c, int n = 0) const;
  bool accept(const char *w);
  bool acceptPunct(char c);
  bool isTerminator(int n = 0) const;
  bool inSentence() const { return pos_ < limit_ && !atEnd(); }
  std::string textUntil(char stop, bool consumeStop);
  void skipSentence();
  int roman(const Token &t, bool *ok) const;

  // structure
  void parseTitle();
  void parseDramatisPersonae();
  bool atActHeader() const { return isWord("act") && isPunct(':', 2) && pos_ + 2 < t_.size(); }
  bool atSceneHeader() const { return isWord("scene") && isPunct(':', 2); }
  void parseAct();
  void parseScene(Act &act);
  void parseStageDirection(Scene &scene);
  void parseSpeech(Scene &scene, int speaker);
  int matchCharacter(size_t at, size_t *len) const;  // longest declared name at token `at`, -1 if none
  static bool speaksProse(const std::string &description);

  // sentences (within [pos_, limit_))
  SentencePtr parseSentence();
  SentencePtr parseQuestion();
  SentencePtr parseGoto();
  SentencePtr parseAssignment();
  ValuePtr parseValue();
  ValuePtr parseConstant();
  bool parseComparison(Sentence::Cmp *cmp);
  int adjectivePolarity(const std::string &lower, bool *isComparative) const;

  const std::vector<Token> &t_;
  Diagnostics &diag_;
  size_t pos_ = 0;
  size_t limit_ = 0;
  Program prog_;
  std::map<std::vector<std::string>, int> names_;  // lower-cased name words -> character id
  size_t maxNameLen_ = 1;
};

}  // namespace spl
