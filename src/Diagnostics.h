// Diagnostics.h - source locations and clang-style diagnostics.
#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace spl {

struct Loc {
  int line = 0, col = 0;  // 1-based
};

class SourceFile {
 public:
  std::string path;
  std::string text;
  std::vector<std::string> lines;

  bool load(const std::string &p);
  const std::string &lineText(int line) const {
    static const std::string empty;
    return (line >= 1 && line <= (int)lines.size()) ? lines[line - 1] : empty;
  }
};

class Diagnostics {
 public:
  explicit Diagnostics(const SourceFile &src) : src_(src) {}
  void error(Loc l, const std::string &msg) { emit("error", l, msg); ++errors_; }
  void warning(Loc l, const std::string &msg) { emit("warning", l, msg); ++warnings_; }
  void note(Loc l, const std::string &msg) { emit("note", l, msg); }
  int errors() const { return errors_; }
  int warnings() const { return warnings_; }

 private:
  void emit(const char *kind, Loc l, const std::string &msg);
  const SourceFile &src_;
  int errors_ = 0, warnings_ = 0;
};

}  // namespace spl
