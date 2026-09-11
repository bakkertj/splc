#include "Diagnostics.h"

#include <fstream>
#include <sstream>

namespace spl {

bool SourceFile::load(const std::string &p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  std::stringstream ss;
  ss << in.rdbuf();
  text = ss.str();
  path = p;
  std::string cur;
  for (char c : text) {
    if (c == '\n') { lines.push_back(cur); cur.clear(); }
    else if (c != '\r') cur.push_back(c);
  }
  lines.push_back(cur);
  return true;
}

void Diagnostics::emit(const char *kind, Loc l, const std::string &msg) {
  std::fprintf(stderr, "%s:%d:%d: %s: %s\n", src_.path.c_str(), l.line, l.col, kind, msg.c_str());
  const std::string &lt = src_.lineText(l.line);
  if (!lt.empty()) {
    std::fprintf(stderr, "%s\n", lt.c_str());
    std::string caret(l.col > 1 ? l.col - 1 : 0, ' ');
    std::fprintf(stderr, "%s^\n", caret.c_str());
  }
}

}  // namespace spl
