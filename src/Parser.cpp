#include "Parser.h"

#include <cctype>
#include <cstring>
#include <set>

#include "Lexicon.h"

namespace spl {

// ---------------------------------------------------------------- helpers ----

bool Parser::isWord(const char *w, int n) const {
  const Token &t = peek(n);
  return t.kind == Tok::Word && t.lower == w;
}
bool Parser::isPunct(char c, int n) const {
  const Token &t = peek(n);
  return t.kind == Tok::Punct && t.text[0] == c;
}
bool Parser::accept(const char *w) {
  if (isWord(w)) { ++pos_; return true; }
  return false;
}
bool Parser::acceptPunct(char c) {
  if (isPunct(c)) { ++pos_; return true; }
  return false;
}
bool Parser::isTerminator(int n) const {
  const Token &t = peek(n);
  return t.kind == Tok::End || (t.kind == Tok::Punct && std::strchr(".!?", t.text[0]));
}

std::string Parser::textUntil(char stop, bool consumeStop) {
  std::string s;
  while (!atEnd() && !isPunct(stop)) {
    if (!s.empty() && !(cur().kind == Tok::Punct)) s += ' ';
    s += cur().text;
    ++pos_;
  }
  if (consumeStop && isPunct(stop)) ++pos_;
  return s;
}

void Parser::skipSentence() {
  while (!atEnd() && !isTerminator()) ++pos_;
  if (!atEnd()) ++pos_;
}

int Parser::roman(const Token &t, bool *ok) const {
  static const std::map<char, int> v = {{'i', 1}, {'v', 5}, {'x', 10}, {'l', 50}, {'c', 100}, {'d', 500}, {'m', 1000}};
  int total = 0, prev = 0;
  *ok = t.kind == Tok::Word && !t.lower.empty();
  if (!*ok) return 0;
  for (auto it = t.lower.rbegin(); it != t.lower.rend(); ++it) {
    auto f = v.find(*it);
    if (f == v.end()) { *ok = false; return 0; }
    if (f->second < prev) total -= f->second; else { total += f->second; prev = f->second; }
  }
  return total;
}

int Parser::matchCharacter(size_t at, size_t *len) const {
  std::vector<std::string> key;
  int best = -1;
  size_t bestLen = 0;
  for (size_t n = 0; n < maxNameLen_ && at + n < t_.size() && t_[at + n].kind == Tok::Word; ++n) {
    key.push_back(t_[at + n].lower);
    auto it = names_.find(key);
    if (it != names_.end()) { best = it->second; bestLen = n + 1; }
  }
  if (len) *len = bestLen;
  return best;
}

// -------------------------------------------------------------- structure ----

// Shakespeare's convention: the well-born speak verse, servants and clowns speak prose.
// We read the character's station from the dramatis personae description.
bool Parser::speaksProse(const std::string &description) {
  std::string d;
  for (char c : description) d.push_back((char)std::tolower((unsigned char)c));
  if (d.find("verse") != std::string::npos) return false;  // "who speaks in verse" overrides
  if (d.find("prose") != std::string::npos) return true;
  static const char *lowBorn[] = {"servant", "clown", "fool", "porter", "gravedigger", "grave-digger", "peasant", "nurse",
                                  "drunkard", "thief", "bawd", "page", "groom", "jester", "shepherd", "cobbler", "carpenter",
                                  "tinker", "tapster", "pedlar", "peddler", "rogue", "constable", "watchman", "citizen",
                                  "wench", "pirate", "sailor", "fisherman", "murderer", "beggar", "rustic", "low-born",
                                  "lowborn", "commoner", "low degree", "vagabond", "knave", "varlet", "gardener", "cook"};
  for (const char *k : lowBorn)
    if (d.find(k) != std::string::npos) return true;
  return false;
}

Program Parser::parse() {
  limit_ = t_.size();
  parseTitle();
  parseDramatisPersonae();
  if (!atActHeader()) diag_.error(cur().loc, "expected 'Act I:' after the dramatis personae");
  while (!atEnd()) {
    if (atActHeader()) parseAct();
    else { diag_.error(cur().loc, "expected an act"); skipSentence(); }
  }
  if (prog_.acts.empty()) diag_.error(cur().loc, "a play needs at least one act");
  return std::move(prog_);
}

void Parser::parseTitle() {
  prog_.title = textUntil('.', true);
  if (prog_.title.empty()) diag_.error(cur().loc, "expected a title ending in a full stop");
}

void Parser::parseDramatisPersonae() {
  while (!atEnd() && !atActHeader()) {
    Character c;
    c.loc = cur().loc;
    std::vector<std::string> key;
    while (!atEnd() && cur().kind == Tok::Word) {
      key.push_back(cur().lower);
      c.name += (c.name.empty() ? "" : " ") + cur().text;
      ++pos_;
    }
    if (!acceptPunct(',')) {
      diag_.error(cur().loc, "expected ',' after character name in the dramatis personae");
      skipSentence();
      continue;
    }
    c.description = textUntil('.', true);
    if (key.empty()) { diag_.error(c.loc, "empty character name"); continue; }
    if (names_.count(key)) { diag_.error(c.loc, "character '" + c.name + "' declared twice"); continue; }
    c.prose = speaksProse(c.description);
    c.id = (int)prog_.characters.size();
    names_[key] = c.id;
    maxNameLen_ = std::max(maxNameLen_, key.size());
    prog_.characters.push_back(c);
  }
  if (prog_.characters.empty()) diag_.error(cur().loc, "a play needs a dramatis personae");
}

void Parser::parseAct() {
  Act act;
  act.loc = cur().loc;
  ++pos_;  // 'act'
  bool ok;
  act.number = roman(cur(), &ok);
  if (!ok) diag_.error(cur().loc, "expected a roman numeral after 'Act'");
  ++pos_;
  acceptPunct(':');
  act.description = textUntil('.', true);
  for (const Act &a : prog_.acts)
    if (a.number == act.number) diag_.error(act.loc, "duplicate act number");
  while (!atEnd() && !atActHeader()) {
    if (atSceneHeader()) parseScene(act);
    else { diag_.error(cur().loc, "expected 'Scene I:' at the start of an act"); skipSentence(); }
  }
  if (act.scenes.empty()) diag_.error(act.loc, "an act needs at least one scene");
  prog_.acts.push_back(std::move(act));
}

void Parser::parseScene(Act &act) {
  Scene scene;
  scene.loc = cur().loc;
  ++pos_;  // 'scene'
  bool ok;
  scene.number = roman(cur(), &ok);
  if (!ok) diag_.error(cur().loc, "expected a roman numeral after 'Scene'");
  ++pos_;
  acceptPunct(':');
  scene.description = textUntil('.', true);
  for (const Scene &s : act.scenes)
    if (s.number == scene.number) diag_.error(scene.loc, "duplicate scene number in this act");
  while (!atEnd() && !atActHeader() && !atSceneHeader()) {
    if (isPunct('[')) { parseStageDirection(scene); continue; }
    size_t len;
    int who = matchCharacter(pos_, &len);
    if (who >= 0 && isPunct(':', (int)len)) {
      pos_ += len + 1;
      parseSpeech(scene, who);
      continue;
    }
    if (cur().kind == Tok::Word && isPunct(':', 1))
      diag_.error(cur().loc, "'" + cur().text + "' is not in the dramatis personae");
    else
      diag_.error(cur().loc, "expected a speaker, a stage direction, an act or a scene");
    skipSentence();
  }
  act.scenes.push_back(std::move(scene));
}

void Parser::parseStageDirection(Scene &scene) {
  Item item;
  item.loc = cur().loc;
  ++pos_;  // '['
  if (accept("enter")) item.kind = Item::Enter;
  else if (accept("exit")) item.kind = Item::Exit;
  else if (accept("exeunt")) item.kind = Item::Exeunt;
  else {
    diag_.error(cur().loc, "stage direction must begin with Enter, Exit or Exeunt");
    while (!atEnd() && !isPunct(']')) ++pos_;
    acceptPunct(']');
    return;
  }
  while (!atEnd() && !isPunct(']')) {
    if (acceptPunct(',') || accept("and")) continue;
    size_t len;
    int who = matchCharacter(pos_, &len);
    if (who < 0) {
      diag_.error(cur().loc, "'" + cur().text + "' is not in the dramatis personae");
      ++pos_;
      continue;
    }
    item.characters.push_back(who);
    pos_ += len;
  }
  if (!acceptPunct(']')) diag_.error(cur().loc, "expected ']' to close the stage direction");
  if (item.kind == Item::Enter && item.characters.empty()) diag_.error(item.loc, "Enter whom?");
  if (item.kind == Item::Exit && item.characters.size() != 1) diag_.error(item.loc, "Exit takes exactly one character (use Exeunt for several)");
  scene.items.push_back(std::move(item));
}

void Parser::parseSpeech(Scene &scene, int speaker) {
  Item item;
  item.kind = Item::Speech;
  item.loc = t_[pos_ - 1].loc;
  item.speaker = speaker;
  size_t start = pos_;
  while (!atEnd() && !isPunct('[') && !atActHeader() && !atSceneHeader()) {
    size_t len;
    if (matchCharacter(pos_, &len) >= 0 && isPunct(':', (int)len)) break;
    // find the end of this sentence
    size_t end = pos_;
    while (end < t_.size() && !(t_[end].kind == Tok::End) && !(t_[end].kind == Tok::Punct && std::strchr(".!?", t_[end].text[0]))) ++end;
    limit_ = end;
    SentencePtr s = parseSentence();
    if (s) {
      if (inSentence()) diag_.error(cur().loc, "unexpected '" + cur().text + "' before the end of the sentence");
      item.sentences.push_back(std::move(s));
    }
    pos_ = end < t_.size() && t_[end].kind != Tok::End ? end + 1 : end;
    limit_ = t_.size();
  }
  // physical lines for scansion
  for (size_t i = start; i < pos_; ++i) {
    if (t_[i].kind != Tok::Word) continue;
    if (item.lines.empty() || item.lines.back().line != t_[i].loc.line) item.lines.push_back({t_[i].loc.line, {}});
    item.lines.back().tokens.push_back((int)i);
  }
  scene.items.push_back(std::move(item));
}

// -------------------------------------------------------------- sentences ----

SentencePtr Parser::parseSentence() {
  if (!inSentence()) return nullptr;
  // A sentence may open with a poetic connective; it carries no meaning.
  while (inSentence() && (isWord("and") || isWord("but") || isWord("o") || isWord("oh") || isWord("now") || isWord("then") || isWord("so") || isWord("yet")) && !isWord("so", 1) && !isWord("not", 1)) ++pos_;
  Loc loc = cur().loc;
  auto mk = [&](Sentence::Kind k) { auto s = std::make_unique<Sentence>(); s->kind = k; s->loc = loc; return s; };

  if (accept("if")) {
    auto s = mk(Sentence::If);
    if (accept("so")) s->condition = true;
    else if (accept("not")) s->condition = false;
    else { diag_.error(cur().loc, "expected 'so' or 'not' after 'If'"); return nullptr; }
    acceptPunct(',');
    s->body = parseSentence();
    if (!s->body) return nullptr;
    if (s->body->kind == Sentence::If) diag_.error(s->body->loc, "conditionals cannot be nested");
    return s;
  }
  if (isWord("let") || isWord("we")) return parseGoto();
  if (accept("speak")) {
    if ((accept("your") || accept("thy")) && accept("mind")) return mk(Sentence::OutputChar);
    diag_.error(loc, "expected 'Speak your mind'"); return nullptr;
  }
  if (accept("open")) {
    if (accept("your") || accept("thy")) {
      if (accept("heart")) return mk(Sentence::OutputInt);
      if (accept("mind")) return mk(Sentence::InputChar);
    }
    diag_.error(loc, "expected 'Open your heart' or 'Open your mind'"); return nullptr;
  }
  if (accept("listen")) {
    if (accept("to") && (accept("your") || accept("thy")) && accept("heart")) return mk(Sentence::InputInt);
    diag_.error(loc, "expected 'Listen to your heart'"); return nullptr;
  }
  if (accept("remember")) {
    auto s = mk(Sentence::Push);
    s->value = parseValue();
    return s->value ? std::move(s) : nullptr;
  }
  if (accept("recall")) {
    auto s = mk(Sentence::Pop);
    pos_ = limit_;  // whatever follows "Recall" is decorative
    return s;
  }
  if (isWord("am") || isWord("art") || isWord("is") || isWord("are")) return parseQuestion();
  if (isWord("you") || isWord("thou")) return parseAssignment();
  diag_.error(loc, "I know not what '" + cur().text + "' means here");
  return nullptr;
}

SentencePtr Parser::parseGoto() {
  auto s = std::make_unique<Sentence>();
  s->kind = Sentence::Goto;
  s->loc = cur().loc;
  bool ok = (accept("let") && accept("us")) || (accept("we") && (accept("shall") || accept("must")));
  ok = ok && (accept("return") || accept("proceed")) && accept("to");
  if (!ok) { diag_.error(s->loc, "expected 'Let us return to scene II' or 'We shall proceed to act III'"); return nullptr; }
  if (accept("act")) s->gotoIsScene = false;
  else if (accept("scene")) s->gotoIsScene = true;
  else { diag_.error(cur().loc, "expected 'act' or 'scene'"); return nullptr; }
  bool rok;
  s->gotoNumber = roman(cur(), &rok);
  if (!rok || !inSentence()) { diag_.error(cur().loc, "expected a roman numeral"); return nullptr; }
  ++pos_;
  return s;
}

SentencePtr Parser::parseAssignment() {
  auto s = std::make_unique<Sentence>();
  s->kind = Sentence::Assign;
  s->loc = cur().loc;
  ++pos_;  // you / thou
  if (!(accept("are") || accept("art"))) {
    // "You nothing!" / "You lying stupid coward!" — the value follows directly
  }
  if (isWord("as") && peek(2).lower == "as") {
    ++pos_;
    const LexEntry *e = Lexicon::lookup(cur().lower);
    if (!e || !(e->flags & W_ADJ)) diag_.warning(cur().loc, "'" + cur().text + "' is not an adjective I know");
    pos_ += 2;
  }
  s->value = parseValue();
  return s->value ? std::move(s) : nullptr;
}

int Parser::adjectivePolarity(const std::string &lower, bool *isComparative) const {
  *isComparative = false;
  static const std::set<std::string> gt = {"better", "bigger", "larger", "greater", "higher", "longer", "taller", "older", "faster", "richer", "more", "stronger", "wiser", "braver"};
  static const std::set<std::string> lt = {"worse", "smaller", "lesser", "lower", "shorter", "younger", "poorer", "fewer", "less", "punier", "weaker", "slower"};
  if (gt.count(lower)) { *isComparative = true; return 1; }
  if (lt.count(lower)) { *isComparative = true; return -1; }
  const LexEntry *e = Lexicon::lookup(lower);
  if (e && (e->flags & W_ADJ) && !(lower.size() > 2 && lower.compare(lower.size() - 2, 2, "er") == 0)) return e->polarity;
  // comparative of a known adjective: -er, -r, -ier
  if (lower.size() > 3 && lower.compare(lower.size() - 2, 2, "er") == 0) {
    std::string base = lower.substr(0, lower.size() - 2);
    for (const std::string &b : {base, base + "e", base.substr(0, base.size() - 1) + "y", base.substr(0, base.size() - 1)}) {
      const LexEntry *be = Lexicon::lookup(b);
      if (be && (be->flags & W_ADJ)) { *isComparative = true; return be->polarity; }
    }
  }
  return e ? e->polarity : -2;  // -2: unknown word
}

bool Parser::parseComparison(Sentence::Cmp *cmp) {
  bool negate = accept("not");
  Loc loc = cur().loc;
  if (accept("as")) {
    bool comp;
    int p = adjectivePolarity(cur().lower, &comp);
    if (p == -2) diag_.warning(loc, "'" + cur().text + "' is not an adjective I know");
    ++pos_;
    if (!accept("as")) { diag_.error(cur().loc, "expected 'as ADJECTIVE as'"); return false; }
    *cmp = negate ? Sentence::NE : Sentence::EQ;
    return true;
  }
  bool more = accept("more"), less = accept("less");
  bool comp;
  int p = adjectivePolarity(cur().lower, &comp);
  if (p == -2) { diag_.error(loc, "'" + cur().text + "' is not a comparison I understand"); return false; }
  if (!more && !less && !comp) { diag_.error(loc, "expected a comparative such as 'better than' or 'more cunning than'"); return false; }
  if (p == 0 && !more && !less) { diag_.error(loc, "'" + cur().text + "' is neither good nor bad; I cannot tell greater from less"); return false; }
  ++pos_;
  if (!accept("than")) { diag_.error(cur().loc, "expected 'than'"); return false; }
  bool greater = (p >= 0);
  if (less) greater = !greater;
  if (negate) *cmp = greater ? Sentence::LE : Sentence::GE;
  else *cmp = greater ? Sentence::GT : Sentence::LT;
  return true;
}

SentencePtr Parser::parseQuestion() {
  auto s = std::make_unique<Sentence>();
  s->kind = Sentence::Question;
  s->loc = cur().loc;
  ++pos_;  // am/art/is/are
  s->lhs = parseValue();
  if (!s->lhs) return nullptr;
  if (!parseComparison(&s->cmp)) return nullptr;
  s->rhs = parseValue();
  if (!s->rhs) return nullptr;
  if (!(t_[limit_].kind == Tok::Punct && t_[limit_].text == "?")) diag_.warning(s->loc, "a question should end with '?'");
  return s;
}

// ------------------------------------------------------------------ values ----

ValuePtr Parser::parseValue() {
  if (!inSentence()) { diag_.error(cur().loc, "expected a value"); return nullptr; }
  auto v = std::make_unique<Value>();
  v->loc = cur().loc;
  const std::string &w = cur().lower;
  if (w == "nothing" || w == "zero") { ++pos_; v->kind = Value::Const; v->constant = 0; return v; }
  if (w == "me" || w == "myself" || w == "i") { ++pos_; v->kind = Value::Me; return v; }
  if (w == "you" || w == "thou" || w == "thee" || w == "thyself" || w == "yourself") { ++pos_; v->kind = Value::You; return v; }
  if (w == "twice") {
    ++pos_; v->kind = Value::Unary; v->op = Value::Twice;
    v->lhs = parseValue();
    return v->lhs ? std::move(v) : nullptr;
  }
  if (w == "the") {
    const std::string &n = peek().lower;
    auto binary = [&](Value::Op op, const char *prep) -> ValuePtr {
      pos_ += 2;
      if (!accept(prep)) { diag_.error(cur().loc, std::string("expected '") + prep + "'"); return nullptr; }
      v->kind = Value::Binary; v->op = op;
      v->lhs = parseValue();
      if (!v->lhs) return nullptr;
      if (!accept("and")) { diag_.error(cur().loc, "expected 'and'"); return nullptr; }
      v->rhs = parseValue();
      return v->rhs ? std::move(v) : nullptr;
    };
    auto unary = [&](Value::Op op, int skip) -> ValuePtr {
      pos_ += skip;
      if (!accept("of")) { diag_.error(cur().loc, "expected 'of'"); return nullptr; }
      v->kind = Value::Unary; v->op = op;
      v->lhs = parseValue();
      return v->lhs ? std::move(v) : nullptr;
    };
    if (n == "sum") return binary(Value::Sum, "of");
    if (n == "difference") return binary(Value::Difference, "between");
    if (n == "product") return binary(Value::Product, "of");
    if (n == "quotient") return binary(Value::Quotient, "between");
    if (n == "remainder") {
      pos_ += 2;
      if (!(accept("of") && accept("the") && accept("quotient") && accept("between"))) {
        diag_.error(cur().loc, "expected 'the remainder of the quotient between X and Y'"); return nullptr;
      }
      v->kind = Value::Binary; v->op = Value::Remainder;
      v->lhs = parseValue();
      if (!v->lhs) return nullptr;
      if (!accept("and")) { diag_.error(cur().loc, "expected 'and'"); return nullptr; }
      v->rhs = parseValue();
      return v->rhs ? std::move(v) : nullptr;
    }
    if (n == "square" && peek(2).lower == "root") return unary(Value::SquareRoot, 3);
    if (n == "square") return unary(Value::Square, 2);
    if (n == "cube") return unary(Value::Cube, 2);
    if (n == "factorial") return unary(Value::Factorial, 2);
  }
  size_t len;
  int who = matchCharacter(pos_, &len);
  if (who >= 0) { pos_ += len; v->kind = Value::Named; v->character = who; return v; }
  return parseConstant();
}

ValuePtr Parser::parseConstant() {
  auto v = std::make_unique<Value>();
  v->loc = cur().loc;
  v->kind = Value::Const;
  static const std::set<std::string> articles = {"a", "an", "the", "my", "thy", "your", "his", "her", "our", "their", "mine", "thine", "this", "that"};
  if (articles.count(cur().lower)) ++pos_;
  std::vector<size_t> words;
  while (inSentence() && cur().kind == Tok::Word && !isWord("and") && !isWord("than") && !isWord("as")) {
    words.push_back(pos_);
    ++pos_;
  }
  if (words.empty()) { diag_.error(v->loc, "expected a noun"); return nullptr; }
  const Token &nounTok = t_[words.back()];
  const LexEntry *noun = Lexicon::lookup(nounTok.lower);
  if (!noun) { diag_.error(nounTok.loc, "'" + nounTok.text + "' is not a word I know"); return nullptr; }
  if (!(noun->flags & W_NOUN)) {
    if (noun->flags & W_NAME) diag_.error(nounTok.loc, "'" + nounTok.text + "' is a name, not a noun; add them to the dramatis personae");
    else diag_.error(nounTok.loc, "'" + nounTok.text + "' is not a noun");
    return nullptr;
  }
  long long value = noun->polarity < 0 ? -1 : 1;
  for (size_t i = 0; i + 1 < words.size(); ++i) {
    const Token &adj = t_[words[i]];
    const LexEntry *e = Lexicon::lookup(adj.lower);
    if (!e) { diag_.error(adj.loc, "'" + adj.text + "' is not a word I know"); return nullptr; }
    if (!(e->flags & W_ADJ)) { diag_.error(adj.loc, "'" + adj.text + "' is not an adjective"); return nullptr; }
    if (e->polarity * noun->polarity < 0)
      diag_.warning(adj.loc, "'" + adj.text + "' is " + (e->polarity > 0 ? "flattering" : "insulting") + " but '" + nounTok.text + "' is not; the bard would blush");
    value *= 2;
  }
  v->constant = value;
  return v;
}

}  // namespace spl
