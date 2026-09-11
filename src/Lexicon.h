// Lexicon.h - the generated English/Shakespeare word table and lookups.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace spl {

enum WordFlags : uint8_t { W_NOUN = 1, W_ADJ = 2, W_NAME = 4, W_FUNCTION = 8 };

struct LexEntry {
  const char *word;
  uint8_t flags;
  int8_t polarity;      // -1, 0, +1
  const char *stress;   // "10|1" : options separated by '|', 'x' = flexible
};

class Lexicon {
 public:
  // Case-folded lookup with Elizabethan normalisation (call'd -> called, didst ...).
  static const LexEntry *lookup(const std::string &word);
  static size_t size();

  // All plausible stress patterns for a word in verse, including Elizabethan
  // variants (-èd as an extra syllable, syncope of heaven/power, elisions).
  // Returns empty if the word is unknown.  extraSyllable forces the grave-accent
  // reading (blessèd).
  static std::vector<std::string> stressOptions(const std::string &word, bool graveAccent);

  static std::string normalize(const std::string &word, bool *hadGrave = nullptr);
};

}  // namespace spl
