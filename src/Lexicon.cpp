#include "Lexicon.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace spl {

static const LexEntry kTable[] = {
#include "Lexicon.inc"
};
static const size_t kTableSize = sizeof(kTable) / sizeof(kTable[0]);

size_t Lexicon::size() { return kTableSize; }

static bool endsWith(const std::string &s, const char *suf) {
  size_t n = std::strlen(suf);
  return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

std::string Lexicon::normalize(const std::string &in, bool *hadGrave) {
  std::string w;
  if (hadGrave) *hadGrave = false;
  for (size_t i = 0; i < in.size(); ++i) {
    unsigned char c = in[i];
    // UTF-8 grave/acute e (è U+00E8 = C3 A8, é U+00E9 = C3 A9) -> 'e' + flag
    if (c == 0xC3 && i + 1 < in.size() && ((unsigned char)in[i + 1] == 0xA8 || (unsigned char)in[i + 1] == 0xA9)) {
      w.push_back('e');
      if (hadGrave) *hadGrave = true;
      ++i;
      continue;
    }
    if (c == 0xE2 && i + 2 < in.size()) {  // curly apostrophes U+2018/2019 -> '
      w.push_back('\'');
      i += 2;
      continue;
    }
    w.push_back((char)std::tolower(c));
  }
  return w;
}

static const LexEntry *rawLookup(const std::string &w) {
  auto it = std::lower_bound(kTable, kTable + kTableSize, w.c_str(),
                             [](const LexEntry &e, const char *s) { return std::strcmp(e.word, s) < 0; });
  if (it != kTable + kTableSize && std::strcmp(it->word, w.c_str()) == 0) return it;
  return nullptr;
}

const LexEntry *Lexicon::lookup(const std::string &word, int *extraSyllables) {
  std::string w = normalize(word);
  if (extraSyllables) *extraSyllables = 0;
  if (const LexEntry *e = rawLookup(w)) return e;
  // Elizabethan spellings and contractions.
  struct Rule { const char *suf; const char *rep; int syllables; };
  static const Rule rules[] = {
      {"'d", "ed", 0}, {"'st", "", 0}, {"est", "", 1}, {"eth", "s", 1}, {"st", "", 0}, {"'s", "", 0}, {"s'", "s", 0},
  };
  for (const Rule &r : rules) {
    if (endsWith(w, r.suf) && w.size() > std::strlen(r.suf) + 2) {
      std::string base = w.substr(0, w.size() - std::strlen(r.suf)) + r.rep;
      if (const LexEntry *e = rawLookup(base)) {
        if (extraSyllables) *extraSyllables = r.syllables;
        return e;
      }
    }
  }
  // o'ercharged, ne'er-do-well: the contraction is one syllable on the front of a known word
  for (const char *pre : {"o'er", "e'er", "ne'er"}) {
    size_t n = std::strlen(pre);
    if (w.size() > n + 2 && w.compare(0, n, pre) == 0) {
      std::string rest = w.substr(n);
      if (!rest.empty() && rest[0] == '-') rest.erase(0, 1);
      if (const LexEntry *e = rawLookup(rest)) {
        if (extraSyllables) *extraSyllables = 1;
        return e;
      }
    }
  }
  std::string nohy = w;
  nohy.erase(std::remove(nohy.begin(), nohy.end(), '-'), nohy.end());
  if (nohy != w) if (const LexEntry *e = rawLookup(nohy)) return e;
  if (w.size() > 1 && w.front() == '\'') if (const LexEntry *e = rawLookup(w.substr(1))) return e;
  return nullptr;
}

static void splitOptions(const char *s, std::vector<std::string> &out) {
  std::string cur;
  for (; *s; ++s) {
    if (*s == '|') { out.push_back(cur); cur.clear(); }
    else cur.push_back(*s);
  }
  if (!cur.empty()) out.push_back(cur);
}

std::vector<std::string> Lexicon::stressOptions(const std::string &word, bool graveAccent) {
  bool grave = false;
  std::string w = normalize(word, &grave);
  graveAccent = graveAccent || grave;
  int extra = 0;
  const LexEntry *e = lookup(word, &extra);
  std::set<std::string> res;
  if (!e) {
    // Unknown word: one flexible syllable per vowel group (lenient).
    int n = 0;
    bool inV = false;
    for (char c : w) {
      bool v = std::strchr("aeiouy", c) != nullptr;
      if (v && !inV) ++n;
      inV = v;
    }
    if (n > 1 && endsWith(w, "e") && !endsWith(w, "le") && !endsWith(w, "ee")) --n;
    if (n < 1) n = 1;
    if (graveAccent) ++n;
    res.insert(std::string(n, 'x'));
    return {res.begin(), res.end()};
  }
  std::vector<std::string> opts;
  splitOptions(e->stress, opts);
  if (extra) {  // vilest, presenteth, o'ercharged: the affix is a syllable of its own
    for (std::string &o : opts) o = (w.compare(0, 1, "o") == 0 || w.compare(0, 1, "e") == 0 || w.compare(0, 1, "n") == 0) && w.find("'er") != std::string::npos ? "x" + o : o + "0";
  }
  for (std::string o : opts) {
    bool contracted = endsWith(w, "'d") || endsWith(w, "'st");
    if (graveAccent) {
      // blessèd: the -ed is a full extra unstressed syllable
      if (!endsWith(o, "0")) o += "0";
      res.insert(o);
      continue;
    }
    if (o.size() == 1) { res.insert("x"); continue; }   // monosyllables are flexible
    if (e->flags & W_FUNCTION) { res.insert(std::string(o.size(), 'x')); continue; }
    res.insert(o);
    if (contracted) continue;
    // -ed may be a full syllable (bless-ed, determin-ed); -ion/-ious may be two (na-ti-on)
    if (endsWith(w, "ed")) res.insert(o + "0");
    if (endsWith(w, "ion") || endsWith(w, "ious") || endsWith(w, "ience")) res.insert(o + "0");
    // syncope: heaven, even, power, flower, spirit, being, every, -ual, -ious -> one fewer
    static const char *syn[] = {"aven", "even", "ower", "irit", "eing", "ery", "ary", "ual", "ious", "eous", "ier", "ior", "eor",
                                "eral", "erous", "ering", "ening", "oral", "ural", "ident", "ience", "ienc", "iance", "iant", "ient", "uous", "eath", "ison", "uel"};
    for (const char *s : syn)
      if (w.find(s) != std::string::npos && o.size() > 1) {
        // drop one unstressed syllable, preferring the last
        size_t pos = o.rfind('0');
        if (pos != std::string::npos) { std::string t = o; t.erase(pos, 1); res.insert(t); }
        break;
      }
  }
  return {res.begin(), res.end()};
}

static std::string spellingRhyme(const std::string &w) {
  // from the last vowel group to the end, with a trailing silent 'e' folded in
  size_t i = w.size();
  while (i > 0 && !std::strchr("aeiouy", w[i - 1])) --i;
  size_t j = i;
  while (j > 0 && std::strchr("aeiouy", w[j - 1])) --j;
  return j < w.size() ? w.substr(j) : w;
}

bool Lexicon::rhymes(const std::string &a, const std::string &b) {
  std::string wa = normalize(a), wb = normalize(b);
  if (wa == wb) return true;  // identical rhyme: the bard allows it
  const LexEntry *ea = lookup(a), *eb = lookup(b);
  if (ea && eb && *ea->rhyme && *eb->rhyme) {
    std::vector<std::string> ra, rb;
    splitOptions(ea->rhyme, ra);
    splitOptions(eb->rhyme, rb);
    for (const std::string &x : ra)
      for (const std::string &y : rb)
        if (x == y) return true;
    // Elizabethan eye-rhymes and shifted vowels (love/move, eye/die): accept a spelling rhyme too
  }
  return spellingRhyme(wa) == spellingRhyme(wb);
}

}  // namespace spl
