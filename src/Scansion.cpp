#include "Scansion.h"

#include <climits>
#include <functional>

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
  std::vector<int> targetsLen = {10};
  if (opts_.allowFeminine) targetsLen.push_back(11);

  // best[i][pos] = (min mismatches, pattern) after i words occupying pos syllables
  const int MAXS = 12;
  struct Cell { int cost = INT_MAX; std::string pat; };
  std::vector<std::vector<Cell>> best(opts.size() + 1, std::vector<Cell>(MAXS + 1));
  best[0][0].cost = 0;
  auto costOf = [&](const std::string &s, int start) {
    int c = 0;
    for (size_t k = 0; k < s.size(); ++k) {
      int p = start + (int)k;
      char want = (p % 2 == 1) ? '1' : '0';
      if (p == 10) want = '0';  // feminine ending
      if (opts_.allowInitialTrochee && p <= 1) continue;  // the first foot may be inverted (Shakespeare does it constantly)
      if (s[k] != 'x' && s[k] != want) ++c;
    }
    return c;
  };
  for (size_t i = 0; i < opts.size(); ++i)
    for (int pos = 0; pos <= MAXS; ++pos) {
      if (best[i][pos].cost == INT_MAX) continue;
      for (const std::string &o : opts[i]) {
        int np = pos + (int)o.size();
        if (np > MAXS) continue;
        int c = best[i][pos].cost + costOf(o, pos);
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

}  // namespace spl
