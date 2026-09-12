#include "Lexer.h"

#include <cctype>
#include <cstring>

#include "Lexicon.h"

namespace spl {

Lexer::Lexer(const SourceFile &src, Diagnostics &diag, bool lenient) : src_(src), diag_(diag), lenient_(lenient) {}

static bool isWordByte(unsigned char c) {
  return std::isalpha(c) || c == '\'' || c == '-' || c == 0xC3 || (c >= 0xA0 && c <= 0xBF);  // è/é and their continuation byte
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> out;
  for (size_t ln = 0; ln < src_.lines.size(); ++ln) {
    std::string s = src_.lines[ln];
    // Typographic punctuation: dashes separate words, curly apostrophes are apostrophes.
    for (const char *dash : {"\xE2\x80\x94", "\xE2\x80\x93", "\xE2\x80\xA6"})
      for (size_t k; (k = s.find(dash)) != std::string::npos;) s.replace(k, 3, "   ");
    for (const char *q : {"\xE2\x80\x99", "\xE2\x80\x98"})
      for (size_t k; (k = s.find(q)) != std::string::npos;) s.replace(k, 3, "'");
    for (const char *q : {"\xE2\x80\x9C", "\xE2\x80\x9D"})
      for (size_t k; (k = s.find(q)) != std::string::npos;) s.replace(k, 3, " ");
    size_t i = 0;
    while (i < s.size()) {
      unsigned char c = s[i];
      if (std::isspace(c)) { ++i; continue; }
      Token t;
      t.loc = {(int)ln + 1, (int)i + 1};
      if (isWordByte(c) && c != '-') {
        size_t j = i;
        while (j < s.size() && isWordByte((unsigned char)s[j])) ++j;
        // trim trailing hyphens/apostrophes that are really punctuation ("--", "word,'")
        while (j > i + 1 && s[j - 1] == '-') --j;
        t.kind = Tok::Word;
        t.text = s.substr(i, j - i);
        // quotation marks written as apostrophes: 'Will' -> Will; a bare ' is not a word
        // a trailing ' is a closing quote unless it marks a plural possessive (lords')
        if (t.text.size() > 1 && t.text.back() == '\'' && t.text[t.text.size() - 2] != 's') t.text.pop_back();
        if (t.text.size() > 1 && t.text[0] == '\'' && t.text.back() == '\'') { t.text = t.text.substr(1, t.text.size() - 2); t.loc.col += 1; }
        if (t.text == "'" || t.text.empty()) { i = j; continue; }
        t.lower = Lexicon::normalize(t.text, &t.graveAccent);
        i = j;
      } else if (std::strchr(".,!?:;[]()", c)) {
        t.kind = Tok::Punct;
        t.text = std::string(1, (char)c);
        ++i;
      } else if (c == '-') {
        ++i;  // dashes are whitespace to us
        continue;
      } else if (std::isdigit(c)) {
        if (!lenient_) diag_.error(t.loc, "digits are not permitted; numbers must be spoken of in words");
        while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
        continue;
      } else {
        if (!lenient_) diag_.error(t.loc, std::string("unexpected character '") + (char)c + "'");
        ++i;
        continue;
      }
      out.push_back(t);
    }
  }
  Token end;
  end.kind = Tok::End;
  end.loc = {(int)src_.lines.size(), 1};
  out.push_back(end);
  return out;
}

}  // namespace spl
