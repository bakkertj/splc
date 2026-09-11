// AST.h - abstract syntax of a Shakespeare program.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics.h"

namespace spl {

struct Character {
  std::string name;  // as declared, e.g. "Romeo", "Lady Macbeth"
  std::string description;
  Loc loc;
  int id = 0;
};

// ---- values -----------------------------------------------------------------
struct Value {
  enum Kind { Const, Me, You, Named, Unary, Binary } kind = Const;
  long long constant = 0;           // Const
  int character = -1;               // Named
  enum Op { Sum, Difference, Product, Quotient, Remainder, Square, Cube, SquareRoot, Factorial, Twice } op = Sum;
  std::unique_ptr<Value> lhs, rhs;  // Unary uses lhs only
  Loc loc;
};
using ValuePtr = std::unique_ptr<Value>;

// ---- sentences --------------------------------------------------------------
struct Sentence {
  enum Kind { Assign, OutputChar, OutputInt, InputChar, InputInt, Question, If, Goto, Push, Pop } kind;
  Loc loc;
  ValuePtr value;                     // Assign / Push
  ValuePtr lhs, rhs;                  // Question
  enum Cmp { EQ, NE, LT, GT, LE, GE } cmp = EQ;
  bool condition = true;              // If: "If so" (true) / "If not" (false)
  std::unique_ptr<Sentence> body;     // If
  bool gotoIsScene = true;            // Goto
  int gotoNumber = 0;
};
using SentencePtr = std::unique_ptr<Sentence>;

// ---- structure --------------------------------------------------------------
struct DialogueLine {  // one physical source line inside a speech, for scansion
  int line;
  std::vector<int> tokens;  // indices into the token vector (words only)
};

struct Item {  // a speech or a stage direction, in scene order
  enum Kind { Speech, Enter, Exit, Exeunt } kind;
  Loc loc;
  int speaker = -1;                 // Speech
  std::vector<SentencePtr> sentences;
  std::vector<DialogueLine> lines;  // Speech
  std::vector<int> characters;      // Enter/Exit/Exeunt (empty Exeunt = everyone)
};

struct Scene {
  int number;  // roman numeral value
  std::string description;
  Loc loc;
  std::vector<Item> items;
};

struct Act {
  int number;
  std::string description;
  Loc loc;
  std::vector<Scene> scenes;
};

struct Program {
  std::string title;
  std::vector<Character> characters;
  std::vector<Act> acts;
};

}  // namespace spl
