#include "CodeGen.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"


namespace spl {

struct CodeGen::Impl {
  const Program &prog;
  Diagnostics &diag;
  llvm::LLVMContext ctx;
  std::unique_ptr<llvm::Module> mod;
  llvm::IRBuilder<> b;
  llvm::Function *mainFn = nullptr;
  llvm::AllocaInst *condVar = nullptr;
  llvm::GlobalVariable *values = nullptr;   // [N x i64] character values
  llvm::Value *curYou = nullptr;            // addressee of the current speech, computed once
  std::unique_ptr<llvm::TargetMachine> tm;
  int curSpeaker = -1;

  // runtime functions
  std::map<std::string, llvm::FunctionCallee> rt;
  std::map<std::pair<int, int>, llvm::BasicBlock *> sceneBlocks;  // (act, scene) -> block
  std::map<int, llvm::BasicBlock *> actBlocks;

  Impl(const Program &p, Diagnostics &d) : prog(p), diag(d), b(ctx) {
    mod = std::make_unique<llvm::Module>("shakespeare", ctx);
  }

  llvm::Type *i64() { return llvm::Type::getInt64Ty(ctx); }
  llvm::Type *i32() { return llvm::Type::getInt32Ty(ctx); }

  void declareRuntime() {
    auto decl = [&](const char *name, llvm::Type *ret, std::vector<llvm::Type *> args) {
      rt[name] = mod->getOrInsertFunction(name, llvm::FunctionType::get(ret, args, false));
    };
    llvm::Type *ptr = llvm::PointerType::getUnqual(ctx);
    decl("spl_init", llvm::Type::getVoidTy(ctx), {i32(), ptr, ptr});
    decl("spl_enter", llvm::Type::getVoidTy(ctx), {i32()});
    decl("spl_exit", llvm::Type::getVoidTy(ctx), {i32()});
    decl("spl_exeunt_all", llvm::Type::getVoidTy(ctx), {});
    decl("spl_addressee", i32(), {i32()});
    decl("spl_push", llvm::Type::getVoidTy(ctx), {i32(), i64()});
    decl("spl_pop", llvm::Type::getVoidTy(ctx), {i32()});
    decl("spl_out_char", llvm::Type::getVoidTy(ctx), {i64()});
    decl("spl_out_int", llvm::Type::getVoidTy(ctx), {i64()});
    decl("spl_in_char", i64(), {});
    decl("spl_in_int", i64(), {});
    decl("spl_div", i64(), {i64(), i64()});
    decl("spl_mod", i64(), {i64(), i64()});
    decl("spl_sqrt", i64(), {i64()});
    decl("spl_factorial", i64(), {i64()});
  }

  llvm::Value *speakerId() { return llvm::ConstantInt::get(i32(), curSpeaker); }
  // The stage cannot change during a speech, so the addressee is computed once per speech.
  llvm::Value *addressee() {
    if (!curYou) curYou = b.CreateCall(rt["spl_addressee"], {speakerId()}, "you");
    return curYou;
  }
  llvm::Value *slot(llvm::Value *id) {
    return b.CreateInBoundsGEP(values->getValueType(), values, {llvm::ConstantInt::get(i32(), 0), id});
  }
  llvm::Value *load(llvm::Value *id, const char *name) { return b.CreateLoad(i64(), slot(id), name); }
  void store(llvm::Value *id, llvm::Value *v) { b.CreateStore(v, slot(id)); }

  llvm::Value *gen(const spl::Value &v) {
    switch (v.kind) {
      case spl::Value::Const: return llvm::ConstantInt::get(i64(), v.constant);
      case spl::Value::Me: return load(speakerId(), "me");
      case spl::Value::You: return load(addressee(), "thee");
      case spl::Value::Named: return load(llvm::ConstantInt::get(i32(), v.character), prog.characters[v.character].name.c_str());
      case spl::Value::Unary: {
        llvm::Value *x = gen(*v.lhs);
        switch (v.op) {
          case spl::Value::Square: return b.CreateMul(x, x, "square");
          case spl::Value::Cube: return b.CreateMul(b.CreateMul(x, x), x, "cube");
          case spl::Value::SquareRoot: return b.CreateCall(rt["spl_sqrt"], {x}, "root");
          case spl::Value::Factorial: return b.CreateCall(rt["spl_factorial"], {x}, "factorial");
          case spl::Value::Twice: return b.CreateShl(x, llvm::ConstantInt::get(i64(), 1), "twice");
          default: break;
        }
        break;
      }
      case spl::Value::Binary: {
        llvm::Value *x = gen(*v.lhs);
        llvm::Value *y = gen(*v.rhs);
        switch (v.op) {
          case spl::Value::Sum: return b.CreateAdd(x, y, "sum");
          case spl::Value::Difference: return b.CreateSub(x, y, "difference");
          case spl::Value::Product: return b.CreateMul(x, y, "product");
          case spl::Value::Quotient: return b.CreateCall(rt["spl_div"], {x, y}, "quotient");
          case spl::Value::Remainder: return b.CreateCall(rt["spl_mod"], {x, y}, "remainder");
          default: break;
        }
        break;
      }
    }
    diag.error(v.loc, "internal: unhandled value");
    return llvm::ConstantInt::get(i64(), 0);
  }

  llvm::BasicBlock *targetOf(const Sentence &s, int curAct) {
    if (s.gotoIsScene) {
      auto it = sceneBlocks.find({curAct, s.gotoNumber});
      if (it == sceneBlocks.end()) { diag.error(s.loc, "there is no such scene in this act"); return nullptr; }
      return it->second;
    }
    auto it = actBlocks.find(s.gotoNumber);
    if (it == actBlocks.end()) { diag.error(s.loc, "there is no such act"); return nullptr; }
    return it->second;
  }

  void genSentence(const Sentence &s, int curAct) {
    switch (s.kind) {
      case Sentence::Assign: store(addressee(), gen(*s.value)); break;
      case Sentence::OutputChar: b.CreateCall(rt["spl_out_char"], {load(addressee(), "thee")}); break;
      case Sentence::OutputInt: b.CreateCall(rt["spl_out_int"], {load(addressee(), "thee")}); break;
      case Sentence::InputChar: store(addressee(), b.CreateCall(rt["spl_in_char"])); break;
      case Sentence::InputInt: store(addressee(), b.CreateCall(rt["spl_in_int"])); break;
      case Sentence::Push: { llvm::Value *you = addressee(); b.CreateCall(rt["spl_push"], {you, gen(*s.value)}); break; }
      case Sentence::Pop: b.CreateCall(rt["spl_pop"], {addressee()}); break;
      case Sentence::Question: {
        llvm::Value *l = gen(*s.lhs);
        llvm::Value *r = gen(*s.rhs);
        llvm::Value *c = nullptr;
        switch (s.cmp) {
          case Sentence::EQ: c = b.CreateICmpEQ(l, r, "eq"); break;
          case Sentence::NE: c = b.CreateICmpNE(l, r, "ne"); break;
          case Sentence::LT: c = b.CreateICmpSLT(l, r, "lt"); break;
          case Sentence::GT: c = b.CreateICmpSGT(l, r, "gt"); break;
          case Sentence::LE: c = b.CreateICmpSLE(l, r, "le"); break;
          case Sentence::GE: c = b.CreateICmpSGE(l, r, "ge"); break;
        }
        b.CreateStore(c, condVar);
        break;
      }
      case Sentence::If: {
        llvm::Value *c = b.CreateLoad(llvm::Type::getInt1Ty(ctx), condVar, "cond");
        if (!s.condition) c = b.CreateNot(c);
        llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(ctx, "then", mainFn);
        llvm::BasicBlock *contBB = llvm::BasicBlock::Create(ctx, "cont", mainFn);
        b.CreateCondBr(c, thenBB, contBB);
        b.SetInsertPoint(thenBB);
        genSentence(*s.body, curAct);
        if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(contBB);
        b.SetInsertPoint(contBB);
        break;
      }
      case Sentence::Goto: {
        llvm::BasicBlock *t = targetOf(s, curAct);
        if (t) b.CreateBr(t);
        // anything after an unconditional goto in the same speech is unreachable
        b.SetInsertPoint(llvm::BasicBlock::Create(ctx, "after_goto", mainFn));
        break;
      }
    }
  }

  bool generate() {
    declareRuntime();
    llvm::FunctionType *mainTy = llvm::FunctionType::get(i32(), false);
    mainFn = llvm::Function::Create(mainTy, llvm::Function::ExternalLinkage, "main", mod.get());
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx, "entry", mainFn);
    b.SetInsertPoint(entry);
    condVar = b.CreateAlloca(llvm::Type::getInt1Ty(ctx), nullptr, "condition");
    b.CreateStore(llvm::ConstantInt::getFalse(ctx), condVar);
    llvm::ArrayType *valTy = llvm::ArrayType::get(i64(), prog.characters.size());
    values = new llvm::GlobalVariable(*mod, valTy, false, llvm::GlobalValue::InternalLinkage,
                                      llvm::ConstantAggregateZero::get(valTy), "characters");

    // character name table for runtime diagnostics
    std::vector<llvm::Constant *> names;
    for (const Character &c : prog.characters) names.push_back(b.CreateGlobalString(c.name, "name"));
    llvm::ArrayType *arrTy = llvm::ArrayType::get(llvm::PointerType::getUnqual(ctx), names.size());
    llvm::GlobalVariable *nameTab = new llvm::GlobalVariable(*mod, arrTy, true, llvm::GlobalValue::PrivateLinkage,
                                                 llvm::ConstantArray::get(arrTy, names), "dramatis_personae");
    b.CreateCall(rt["spl_init"], {llvm::ConstantInt::get(i32(), (int)prog.characters.size()), nameTab, values});

    // pre-create act and scene blocks so gotos can be forward references
    for (const Act &a : prog.acts) {
      actBlocks[a.number] = llvm::BasicBlock::Create(ctx, "act_" + std::to_string(a.number), mainFn);
      for (const Scene &s : a.scenes)
        sceneBlocks[{a.number, s.number}] =
            llvm::BasicBlock::Create(ctx, "act_" + std::to_string(a.number) + "_scene_" + std::to_string(s.number), mainFn);
    }
    b.CreateBr(actBlocks[prog.acts.front().number]);

    for (const Act &a : prog.acts) {
      b.SetInsertPoint(actBlocks[a.number]);
      b.CreateBr(sceneBlocks[{a.number, a.scenes.front().number}]);
      for (size_t si = 0; si < a.scenes.size(); ++si) {
        const Scene &s = a.scenes[si];
        b.SetInsertPoint(sceneBlocks[{a.number, s.number}]);
        for (const Item &it : s.items) {
          switch (it.kind) {
            case Item::Enter:
              for (int c : it.characters) b.CreateCall(rt["spl_enter"], {llvm::ConstantInt::get(i32(), c)});
              break;
            case Item::Exit:
              for (int c : it.characters) b.CreateCall(rt["spl_exit"], {llvm::ConstantInt::get(i32(), c)});
              break;
            case Item::Exeunt:
              if (it.characters.empty()) b.CreateCall(rt["spl_exeunt_all"]);
              else for (int c : it.characters) b.CreateCall(rt["spl_exit"], {llvm::ConstantInt::get(i32(), c)});
              break;
            case Item::Speech:
              curSpeaker = it.speaker;
              curYou = nullptr;
              for (const SentencePtr &sp : it.sentences) genSentence(*sp, a.number);
              break;
          }
        }
        // fall through to the next scene / act, or finish the play
        llvm::BasicBlock *next = nullptr;
        if (si + 1 < a.scenes.size()) next = sceneBlocks[{a.number, a.scenes[si + 1].number}];
        else {
          auto ai = actBlocks.upper_bound(a.number);
          if (ai != actBlocks.end()) next = ai->second;
        }
        if (next) b.CreateBr(next);
        else b.CreateRet(llvm::ConstantInt::get(i32(), 0));
      }
    }
    std::string err;
    llvm::raw_string_ostream os(err);
    if (llvm::verifyModule(*mod, &os)) {
      diag.error({0, 0}, "internal: LLVM module verification failed:\n" + os.str());
      return false;
    }
    return diag.errors() == 0;
  }
};

CodeGen::CodeGen(const Program &prog, Diagnostics &diag) : p_(std::make_unique<Impl>(prog, diag)) {}
CodeGen::~CodeGen() = default;
bool CodeGen::generate() { return p_->generate(); }

static bool initTarget(CodeGen::Impl &p) {
  if (p.tm) return true;
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  std::string triple = llvm::sys::getDefaultTargetTriple();
  std::string err;
  const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (!target) { p.diag.error({0, 0}, err); return false; }
  llvm::TargetOptions opt;
  p.tm.reset(target->createTargetMachine(triple, "generic", "", opt, llvm::Reloc::PIC_));
  p.mod->setDataLayout(p.tm->createDataLayout());
  p.mod->setTargetTriple(triple);
  return true;
}

bool CodeGen::optimize(int level) {
  if (!initTarget(*p_)) return false;
  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;
  llvm::PassBuilder pb(p_->tm.get());
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);
  llvm::OptimizationLevel ol = level <= 0 ? llvm::OptimizationLevel::O0
                             : level == 1 ? llvm::OptimizationLevel::O1
                             : level == 2 ? llvm::OptimizationLevel::O2 : llvm::OptimizationLevel::O3;
  llvm::ModulePassManager mpm = level <= 0 ? pb.buildO0DefaultPipeline(ol) : pb.buildPerModuleDefaultPipeline(ol);
  mpm.run(*p_->mod, mam);
  return true;
}

bool CodeGen::writeIR(const std::string &path) {
  std::error_code ec;
  llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
  if (ec) { p_->diag.error({0, 0}, "cannot write " + path + ": " + ec.message()); return false; }
  p_->mod->print(out, nullptr);
  return true;
}

bool CodeGen::writeObject(const std::string &path) {
  if (!initTarget(*p_)) return false;
  llvm::TargetMachine *tm = p_->tm.get();
  std::error_code ec;
  llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
  if (ec) { p_->diag.error({0, 0}, "cannot write " + path + ": " + ec.message()); return false; }
  llvm::legacy::PassManager pm;
  if (tm->addPassesToEmitFile(pm, out, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    p_->diag.error({0, 0}, "target cannot emit object files");
    return false;
  }
  pm.run(*p_->mod);
  out.flush();
  return true;
}

}  // namespace spl
