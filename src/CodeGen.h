// CodeGen.h - lowers a Program to LLVM IR.
#pragma once
#include <map>
#include <memory>
#include <string>

#include "AST.h"
#include "Diagnostics.h"

namespace llvm {
class LLVMContext;
class Module;
class Function;
class BasicBlock;
class Value;
class AllocaInst;
template <typename T, typename Inserter> class IRBuilder;
}  // namespace llvm

namespace spl {

class CodeGen {
 public:
  CodeGen(const Program &prog, Diagnostics &diag);
  ~CodeGen();
  bool generate();
  bool writeIR(const std::string &path);
  bool writeObject(const std::string &path);

 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};

}  // namespace spl
