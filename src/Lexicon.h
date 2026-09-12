// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Trevor Bakker
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
  const char *rhyme;    // CMU phones from the last stressed vowel, per pronunciation, '|'-separated; "" if unknown
};

class Lexicon {
 public:
  // Case-folded lookup with Elizabethan normalisation (call'd -> called, didst ...).
  // extraSyllables (optional) receives 1 when the word was found only by stripping an
  // -est/-eth suffix that carries its own syllable (vilest, presenteth).
  static const LexEntry *lookup(const std::string &word, int *extraSyllables = nullptr);
  static size_t size();

  // All plausible stress patterns for a word in verse, including Elizabethan
  // variants (-èd as an extra syllable, syncope of heaven/power, elisions).
  // Returns empty if the word is unknown.  extraSyllable forces the grave-accent
  // reading (blessèd).
  static std::vector<std::string> stressOptions(const std::string &word, bool graveAccent);

  static std::string normalize(const std::string &word, bool *hadGrave = nullptr);

  // Do two words rhyme?  Exact on CMU phones when both are known; otherwise a
  // spelling rhyme on the final vowel group and what follows it.
  // With `near`, vowels that Elizabethan ears (or spelling) let rhyme are pooled:
  // come/doom, wrong/young, were/bear, past/waste.
  static bool rhymes(const std::string &a, const std::string &b, bool near = false);
};

}  // namespace spl
