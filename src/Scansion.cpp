#include "Scansion.h"

#include <climits>
#include <cstring>
#include <functional>
#include <set>

#include "Lexicon.h"

namespace spl {

// A line scans if some choice of per-word stress options concatenates to a
// pattern of 10 syllables (11 with a feminine ending) whose stressed syllables
// fall on even positions, allowing `tolerance` violations.  Flexible ('x')
// syllables never count as violations.  Dynamic programming over (word, syllable
// position) keeps this linear in practice.
ScansionResult Scansion::scanLine(const DialogueLine &line) const {
  ScansionResult r;
  std::vector<std::vector<std::string>> opts;
  for (int ti : line.tokens) {
    const Token &t = t_[ti];
    std::vector<std::string> o = Lexicon::stressOptions(t.text, t.graveAccent);
    if (!Lexicon::lookup(t.text)) r.unknownWords = true;
    opts.push_back(o);
  }
  // Cross-word elisions the verse allows: th'expense, t'assist, I'm, thou'rt, we're, 'tis, i'th'.
  static const std::set<std::string> elidable = {"the", "to", "thou", "thy", "my", "be", "he", "she", "we", "i", "you", "they", "so", "thee"};
  static const std::set<std::string> pronouns = {"i", "thou", "he", "she", "it", "we", "you", "they", "that", "there", "who", "what", "here", "this"};
  static const std::set<std::string> auxiliaries = {"am", "is", "are", "art", "will", "would", "shall", "had", "have", "has", "were", "was"};
  static const std::set<std::string> prepositions = {"in", "of", "on", "by", "to", "at"};
  for (size_t i = 0; i + 1 < opts.size(); ++i) {
    const std::string &a = t_[line.tokens[i]].lower, &b = t_[line.tokens[i + 1]].lower;
    bool bVowel = !b.empty() && (std::strchr("aeiou", b[0]) || (b[0] == 'h' && b.size() > 1 && std::strchr("aeiou", b[1])));
    if (elidable.count(a) && bVowel) opts[i].push_back("");
    if (pronouns.count(a) && auxiliaries.count(b)) opts[i + 1].push_back("");
    if (prepositions.count(a) && b == "the") opts[i + 1].push_back("");
  }
  std::vector<int> targetsLen = {10};
  if (opts_.allowFeminine) targetsLen.push_back(11);

  // best[i][pos] = (min mismatches, pattern) after i words occupying pos syllables
  const int MAXS = 12;
  struct Cell { int cost = INT_MAX; std::string pat; };
  std::vector<std::vector<Cell>> best(opts.size() + 1, std::vector<Cell>(MAXS + 1));
  best[0][0].cost = 0;
  // Which words open a new phrase (follow punctuation)?  A foot may be inverted there.
  std::vector<bool> afterCaesura(opts.size(), false);
  for (size_t i = 1; i < opts.size(); ++i) {
    int ti = line.tokens[i];
    if (ti > 0 && t_[ti - 1].kind == Tok::Punct && std::strchr(".,;:!?", t_[ti - 1].text[0])) afterCaesura[i] = true;
  }
  // Metrical cost: a stressed syllable in a weak position counts; an unstressed
  // syllable in a strong position is "promoted" and does not (standard prosody).
  // The first foot, and a foot beginning a new phrase after punctuation, may be
  // inverted for free.
  auto costOf = [&](const std::string &s, int start, bool caesura) {
    int c = 0;
    for (size_t k = 0; k < s.size(); ++k) {
      int p = start + (int)k;
      bool weak = (p % 2 == 0) || p == 10;  // odd positions are strong; the 11th syllable is a feminine ending
      if (opts_.allowInitialTrochee && p <= 1) continue;
      if (caesura && k <= 1 && start % 2 == 0) continue;
      if (s[k] == '1' && weak) ++c;
    }
    return c;
  };
  for (size_t i = 0; i < opts.size(); ++i)
    for (int pos = 0; pos <= MAXS; ++pos) {
      if (best[i][pos].cost == INT_MAX) continue;
      for (const std::string &o : opts[i]) {
        int np = pos + (int)o.size();
        if (np > MAXS) continue;
        int c = best[i][pos].cost + costOf(o, pos, afterCaesura[i]);
        if (c < best[i + 1][np].cost) { best[i + 1][np].cost = c; best[i + 1][np].pat = best[i][pos].pat + o; }
      }
    }
  int bestCost = INT_MAX, bestLen = 0;
  for (int len : targetsLen)
    if (best[opts.size()][len].cost < bestCost) { bestCost = best[opts.size()][len].cost; bestLen = len; }
  if (bestCost == INT_MAX) {
    // wrong syllable count: report the most plausible count
    int any = -1;
    for (int p = 0; p <= MAXS; ++p) if (best[opts.size()][p].cost != INT_MAX) { any = p; if (p >= 10) break; }
    r.scans = false;
    r.syllables = any;
    r.explanation = any < 0 ? "more than 12 syllables" : std::to_string(any) + " syllables";
    if (any >= 0) r.pattern = best[opts.size()][any].pat;
    return r;
  }
  r.syllables = bestLen;
  r.mismatches = bestCost;
  r.pattern = best[opts.size()][bestLen].pat;
  // resolve flexible syllables to the ideal for display
  for (size_t k = 0; k < r.pattern.size(); ++k)
    if (r.pattern[k] == 'x') r.pattern[k] = (k % 2 == 1 && k < 10) ? '1' : '0';
  r.scans = bestCost <= opts_.tolerance;
  if (!r.scans) r.explanation = std::to_string(bestCost) + " stressed syllable(s) out of place";
  return r;
}

int Scansion::check(const Program &prog) {
  if (opts_.mode == ScansionOptions::Off) return 0;
  int failed = 0;
  for (const Act &a : prog.acts)
    for (const Scene &s : a.scenes)
      for (const Item &it : s.items) {
        if (it.kind != Item::Speech) continue;
        if (opts_.proseExemption && prog.characters[it.speaker].prose) continue;
        for (size_t li = 0; li < it.lines.size(); ++li) {
          const DialogueLine &dl = it.lines[li];
          if ((int)dl.tokens.size() < opts_.minWords) continue;
          ScansionResult r = scanLine(dl);
          if (r.scans) continue;
          // A short line opening or closing a speech is a shared line: the other
          // half belongs to another speaker (or to silence).  Shakespeare's habit.
          bool edge = li == 0 || li + 1 == it.lines.size();
          if (edge && r.syllables >= 0 && r.syllables < 9) continue;
          ++failed;
          std::string msg = "line does not scan as iambic pentameter (" + r.explanation + ")";
          if (!r.pattern.empty()) msg += ": " + r.pattern;
          if (r.unknownWords) msg += " [contains words not in the lexicon; syllables guessed]";
          Loc loc = t_[dl.tokens.front()].loc;
          if (opts_.mode == ScansionOptions::Error) diag_.error(loc, msg);
          else diag_.warning(loc, msg);
        }
      }
  return failed;
}

int Scansion::checkCouplets(const Program &prog) {
  if (!opts_.couplets) return 0;
  int failed = 0;
  for (const Act &a : prog.acts)
    for (const Scene &s : a.scenes) {
      // the last two lines of dialogue in the scene, whoever speaks them
      std::vector<const DialogueLine *> tail;
      for (auto it = s.items.rbegin(); it != s.items.rend() && tail.size() < 2; ++it) {
        if (it->kind != Item::Speech) continue;
        for (auto li = it->lines.rbegin(); li != it->lines.rend() && tail.size() < 2; ++li) tail.push_back(&*li);
      }
      if (tail.size() < 2) continue;
      const Token &w1 = t_[tail[1]->tokens.back()], &w2 = t_[tail[0]->tokens.back()];
      if (Lexicon::rhymes(w1.text, w2.text)) continue;
      ++failed;
      std::string msg = "scene does not end in a rhyming couplet ('" + w1.text + "' / '" + w2.text + "')";
      if (opts_.mode == ScansionOptions::Error) diag_.error(w2.loc, msg);
      else diag_.warning(w2.loc, msg);
    }
  return failed;
}

}  // namespace spl
