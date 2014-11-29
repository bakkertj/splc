
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <set>

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,

  // commands
  tok_character = -2,
  tok_be = -3,
  tok_identifier = -4,
  tok_article = -5,
  tok_def = -6,
  tok_number = -7,
  tok_first_person = -8,
  tok_first_person_possessive = -9,
  tok_person_reflexive = -10,
  tok_neg_adjective = -11,
  tok_negative_comparative = -12,
  token_negative_noun = -13,
  tok_neutral_adjective = -14,
  tok_neutral_noun = -15,
  tok_nothing = -16,
  tok_positive_adjective = -17,
  tok_positive_comparative = -18,
  tok_positive_noun = -19,
  tok_second_person = -20,
  tok_second_person_possesive = -21,
  tok_second_person_reflexive = -22,
  tok_third_person_possessive = -23,
  tok_colon = -24,
  tok_comma = -25,
  tok_exclamation_mark = -26,
  tok_left_bracket = -27,
  tok_period = -28,
  tok_question_mark = -29,
  tok_right_bracket = -30,
  tok_and = -31,
  tok_as = -32,
  tok_enter = -33,
  tok_exeunt = -34,
  tok_exit = -35,
  tok_heart = -36,
  tok_if_not = -37,
  tok_if_so = -38,
  tok_less = -39,
  tok_let_us = -40,
  tok_listen_to = -41,
  tok_mind = -42,
  tok_more = -43,
  tok_not = -44,
  tok_open = -45,
  tok_proceed_to = -46,
  tok_recall = -47,
  tok_remember = -48,
  tok_return_to = -49,
  tok_speak = -50,
  tok_than = -51,
  tok_the_cube_of = -52,
  tok_the_difference_between = -53,
  tok_the_factorial_of = -54,
  tok_the_product_of = -55,
  tok_the_quotient_between = -56,
  tok_the_remainder_of_the_quotient = -57,
  tok_the_square_of = -58,
  tok_the_square_root_of = -59,
  tok_the_sum_of = -60,
  tok_twice = -61,
  tok_we_must = -62,
  tok_we_shall = -63,
  tok_act_roman = -64,
  tok_scene_roman = -65,
  tok_roman_number = -66,
  tok_nonmatch = -67
};

static std::string IdentifierStr;  // Filled in if tok_identifier
static double NumVal;              // Filled in if tok_number

static std::set<std::string> ArticleSet;
static std::set<std::string> FirstPersonSet;
static std::set<std::string> CharacterSet;
static std::set<std::string> BeSet;
static std::set<std::string> FirstPersonPossessiveSet;
static std::set<std::string> PositiveAdjectiveSet;

static void chomp(char *s) {
  while(*s && *s != '\n' && *s != '\r') s++;
  *s = 0;
}

static int loadLists()
{
  FILE * fp = fopen ("include/character.wordlist", "r" );
  
  if ( fp != NULL )
  {
    char line [ 128 ];
    while ( fgets ( line, sizeof line, fp ) != NULL )
    {
      chomp(line);
      CharacterSet.insert( line );
    }
    fclose ( fp );
  }

  fp = fopen ("include/be.wordlist", "r" );
  
  if ( fp != NULL )
  {
    char line [ 128 ];
    while ( fgets ( line, sizeof line, fp ) != NULL )
    {
      chomp(line);
      BeSet.insert( line );
    }
    fclose ( fp );
  }
  
  fp = fopen ("include/article.wordlist", "r" );
  
  if ( fp != NULL )
  {
    char line [ 128 ];
    while ( fgets ( line, sizeof line, fp ) != NULL )
    {
      chomp(line);
      ArticleSet.insert( line );
    }
    fclose ( fp );
  }
  
  fp = fopen ("include/first_person.wordlist", "r" );
  
  if ( fp != NULL )
  {
    char line [ 128 ];
    while ( fgets ( line, sizeof line, fp ) != NULL )
    {
      chomp(line);
      FirstPersonSet.insert( line );
    }
    fclose ( fp );
  }
  
  fp = fopen ("include/first_person_possessive.wordlist", "r" );
  
  if ( fp != NULL )
  {
    char line [ 128 ];
    while ( fgets ( line, sizeof line, fp ) != NULL )
    {
      chomp(line);
      FirstPersonPossessiveSet.insert( line );
    }
    fclose ( fp );
  }
  
  fp = fopen ("include/positive_adjective.wordlist.wordlist", "r" );
  
  if ( fp != NULL )
  {
    char line [ 128 ];
    while ( fgets ( line, sizeof line, fp ) != NULL )
    {
      chomp(line);
      PositiveAdjectiveSet.insert( line );
    }
    fclose ( fp );
  }
  return 0;
}

static int isCharacter( std::string name )
{
  return CharacterSet.find(name) != CharacterSet.end();
}

static int isBe( std::string word )
{
  return BeSet.find(word) != BeSet.end();
}

static int isArticle( std::string word )
{
  return ArticleSet.find(word) != ArticleSet.end();
}

static int isFirstPerson( std::string word )
{
  return FirstPersonSet.find(word) != FirstPersonSet.end();
}

static int isFirstPersonPossessive( std::string word )
{
  return FirstPersonPossessiveSet.find(word) != FirstPersonPossessiveSet.end();
}

static int isPositiveAdjective( std::string word )
{
  return PositiveAdjectiveSet.find(word) != PositiveAdjectiveSet.end();
}

/// gettok - Return the next token from standard input.
static int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getchar();

  if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if ( isCharacter( IdentifierStr) ) return tok_character;
    if ( isBe       ( IdentifierStr) ) return tok_be;
    if ( isArticle  ( IdentifierStr) ) return tok_article;
    if ( isFirstPerson  ( IdentifierStr) ) return tok_first_person;
    if ( isFirstPersonPossessive( IdentifierStr) ) return tok_first_person_possessive;
    if ( isPositiveAdjective( IdentifierStr) ) return tok_positive_adjective;
    if ( IdentifierStr == "Enter")
    {
      printf("Got enter\n");
    
      return tok_enter;
  }
    if (IdentifierStr == "def") return tok_def;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') {   // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }

  if (LastChar == '#') {
    // Comment until end of line.
    do LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    
    if (LastChar != EOF)
      return gettok();
  }
  
  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//
namespace {
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual Value *Codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;
public:
  NumberExprAST(double val) : Val(val) {}
  virtual Value *Codegen();
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(const std::string &name) : Name(name) {}
  virtual Value *Codegen();
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs)
  : Op(op), LHS(lhs), RHS(rhs) {}
  virtual Value *Codegen();
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}
  virtual Value *Codegen();
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args)
    : Name(name), Args(args) {}
  Function *Codegen();
  
};
  
class EnterAST : public ExprAST {
  std::string Name;
public:
  EnterAST(const std::string &name)
  : Name(name){}
  Value *Codegen();
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;
public:
  FunctionAST(PrototypeAST *proto, ExprAST *body)
  : Proto(proto), Body(body) {}
  
  Function *Codegen();
};
} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
  return CurTok = gettok();
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;
  
  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

/// Error* - These are little helper functions for error handling.
ExprAST *Error(const char *Str) { fprintf(stderr, "Error: %s\n", Str);return 0;}
PrototypeAST *ErrorP(const char *Str) { Error(Str); return 0; }
FunctionAST *ErrorF(const char *Str) { Error(Str); return 0; }

static ExprAST *ParseExpression();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  
  getNextToken();  // eat identifier.
  
  if (CurTok != '(') // Simple variable ref.
    return new VariableExprAST(IdName);
  
  // Call.
  getNextToken();  // eat (
  std::vector<ExprAST*> Args;
  if (CurTok != ')') {
    while (1) {
      ExprAST *Arg = ParseExpression();
      if (!Arg) return 0;
      Args.push_back(Arg);

      if (CurTok == ')') break;

      if (CurTok != ',')
        return Error("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();
  
  return new CallExprAST(IdName, Args);
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseCharacterExpr() {
  std::string IdName = IdentifierStr;
  printf("Parsing characeter expression\n");
  getNextToken();  // eat identifier.
  
  if (CurTok != '(') // Simple variable ref.
    return new VariableExprAST(IdName);
  
  // Call.
  getNextToken();  // eat (
  std::vector<ExprAST*> Args;
  if (CurTok != ')') {
    while (1) {
      ExprAST *Arg = ParseExpression();
      if (!Arg) return 0;
      Args.push_back(Arg);
      
      if (CurTok == ')') break;
      
      if (CurTok != ',')
        return Error("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }
  
  // Eat the ')'.
  getNextToken();
  
  return new CallExprAST(IdName, Args);
}

/// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
  ExprAST *Result = new NumberExprAST(NumVal);
  getNextToken(); // consume the number
  return Result;
}

/// parenexpr ::= '(' expression ')'
static ExprAST *ParseParenExpr() {
  getNextToken();  // eat (.
  ExprAST *V = ParseExpression();
  if (!V) return 0;
  
  if (CurTok != ')')
    return Error("expected ')'");
  getNextToken();  // eat ).
  return V;
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static ExprAST *ParsePrimary() {
  printf("Token: %d\n", CurTok);
  switch (CurTok) {
  default: return Error("unknown token when expecting an expression");
  case tok_character:  return ParseCharacterExpr();
  case tok_number:     return ParseNumberExpr();
  case '(':            return ParseParenExpr();
  }
}

/// binoprhs
///   ::= ('+' primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
  // If this is a binop, find its precedence.
  while (1) {
    int TokPrec = GetTokPrecedence();
    
    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;
    
    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken();  // eat binop
    
    // Parse the primary expression after the binary operator.
    ExprAST *RHS = ParsePrimary();
    if (!RHS) return 0;
    
    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec+1, RHS);
      if (RHS == 0) return 0;
    }
    
    // Merge LHS/RHS.
    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}

/// expression
///   ::= primary binoprhs
///
static ExprAST *ParseExpression() {
  ExprAST *LHS = ParsePrimary();
  if (!LHS) return 0;
  
  return ParseBinOpRHS(0, LHS);
}

/// prototype
///   ::= id '(' id* ')'
static PrototypeAST *ParsePrototype() {
  if (CurTok != tok_identifier)
    return ErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();
  
  if (CurTok != '(')
    return ErrorP("Expected '(' in prototype");
  
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return ErrorP("Expected ')' in prototype");
  
  // success.
  getNextToken();  // eat ')'.
  
  return new PrototypeAST(FnName, ArgNames);
}

/// definition ::= 'def' prototype expression
static FunctionAST *ParseDefinition() {
  getNextToken();  // eat def.
  PrototypeAST *Proto = ParsePrototype();
  if (Proto == 0) return 0;

  if (ExprAST *E = ParseExpression())
    return new FunctionAST(Proto, E);
  return 0;
}

/// toplevelexpr ::= expression
static FunctionAST *ParseTopLevelExpr() {
  if (ExprAST *E = ParseExpression()) {
    // Make an anonymous proto.
    PrototypeAST *Proto = new PrototypeAST("", std::vector<std::string>());
    return new FunctionAST(Proto, E);
  }
  return 0;
}

/// external ::= 'extern' prototype
static PrototypeAST *ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}

/// external ::= 'enter' prototype
static EnterAST *ParseEnter() {
  getNextToken();  // eat extern.
  std::string character = IdentifierStr;
  getNextToken();
  return new EnterAST(character);
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static Module *TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, AllocaInst*> NamedValues;


Value *ErrorV(const char *Str) { Error(Str); return 0; }

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          const std::string &VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(getGlobalContext()), 0,
                           VarName.c_str());
}

Value *NumberExprAST::Codegen() {
  return ConstantFP::get(getGlobalContext(), APFloat(Val));
}

Value *VariableExprAST::Codegen() {
  // Look this variable up in the function.
  Value *V = NamedValues[Name];
  return V ? V : ErrorV("Unknown variable name");
}

Value *BinaryExprAST::Codegen() {
  Value *L = LHS->Codegen();
  Value *R = RHS->Codegen();
  if (L == 0 || R == 0) return 0;
  
  switch (Op) {
    case '+': return Builder.CreateFAdd(L, R, "addtmp");
    case '-': return Builder.CreateFSub(L, R, "subtmp");
    case '*': return Builder.CreateFMul(L, R, "multmp");
    case '<':
      L = Builder.CreateFCmpULT(L, R, "cmptmp");
      // Convert bool 0/1 to double 0.0 or 1.0
      return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()),
                                  "booltmp");
    default: return ErrorV("invalid binary operator");
  }
}

Value *CallExprAST::Codegen() {
  // Look up the name in the global module table.
  Function *CalleeF = TheModule->getFunction(Callee);
  if (CalleeF == 0)
    return ErrorV("Unknown function referenced");
  
  // If argument mismatch error.
  if (CalleeF->arg_size() != Args.size())
    return ErrorV("Incorrect # arguments passed");
  
  std::vector<Value*> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->Codegen());
    if (ArgsV.back() == 0) return 0;
  }
  
  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::Codegen() {
  // Make the function type:  double(double,double) etc.
  std::vector<Type*> Doubles(Args.size(),
                             Type::getDoubleTy(getGlobalContext()));
  FunctionType *FT = FunctionType::get(Type::getDoubleTy(getGlobalContext()),
                                       Doubles, false);
  
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);
  
  // If F conflicted, there was already something named 'Name'.  If it has a
  // body, don't allow redefinition or reextern.
  if (F->getName() != Name) {
    // Delete the one we just made and get the existing one.
    F->eraseFromParent();
    F = TheModule->getFunction(Name);
    
    // If F already has a body, reject this.
    if (!F->empty()) {
      ErrorF("redefinition of function");
      return 0;
    }
    
    // If F took a different number of args, reject.
    if (F->arg_size() != Args.size()) {
      ErrorF("redefinition of function with different # args");
      return 0;
    }
  }
  
  // Set names for all arguments.
  unsigned Idx = 0;
  for (Function::arg_iterator AI = F->arg_begin(); Idx != Args.size();
       ++AI, ++Idx)
    AI->setName(Args[Idx]);
  
  return F;
}

Function *FunctionAST::Codegen() {
  NamedValues.clear();
  
  Function *TheFunction = Proto->Codegen();
  if (TheFunction == 0)
    return 0;
  
  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
  Builder.SetInsertPoint(BB);
  
  if (Value *RetVal = Body->Codegen()) {
    // Finish off the function.
    Builder.CreateRet(RetVal);
    
    // Validate the generated code, checking for consistency.
    verifyFunction(*TheFunction);
    
    return TheFunction;
  }
  
  // Error reading body, remove function.
  TheFunction->eraseFromParent();
  return 0;
}

Value *EnterAST::Codegen() {
  std::vector<AllocaInst *> OldBindings;
  
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  
  // Register all variables and emit their initializer.

  const std::string &VarName = Name;
  ExprAST *Init = 0;
    
  // Emit the initializer before adding the variable to scope, this prevents
  // the initializer from referencing the variable itself, and permits stuff
  // like this:
  //  var a = 1 in
  //    var a = a in ...   # refers to outer 'a'.
  Value *InitVal;
  if (Init) {
    InitVal = Init->Codegen();
    if (InitVal == 0) return 0;
  } else { // If not specified, use 0.0.
    InitVal = ConstantFP::get(getGlobalContext(), APFloat(0.0));
  }
    
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
  Builder.CreateStore(InitVal, Alloca);
    
  // Remember the old variable binding so that we can restore the binding when
  // we unrecurse.
    OldBindings.push_back(NamedValues[VarName]);
    
    // Remember this binding.
    NamedValues[VarName] = Alloca;
  
  
  // Codegen the body, now that all vars are in scope.
  Value *BodyVal = Body->Codegen();
  if (BodyVal == 0) return 0;
  
  // Pop all our variables from scope.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    NamedValues[VarNames[i].first] = OldBindings[i];
  
  // Return the body computation.
  return BodyVal;
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
  if (FunctionAST *F = ParseDefinition()) {
    if (Function *LF = F->Codegen()) {
      fprintf(stderr, "Read function definition:");
      LF->dump();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleEnter() {
  if (EnterAST *F = ParseEnter()) {
    if (Function *LF = F->Codegen()) {
      fprintf(stderr, "Parsed an enter");
      LF->dump();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleEntern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (1) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:    return;
    case ';':        getNextToken(); break;  // ignore top-level semicolons.
    case tok_def:    HandleDefinition(); break;
    case tok_enter:  HandleEnter(); break;
    default:         HandleTopLevelExpression(); break;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  // highest.
  
  loadLists();
  
  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();
 
  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}
