// main.cpp - splc driver: splc [options] play.spl
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "CodeGen.h"
#include "Diagnostics.h"
#include "Lexer.h"
#include "Lexicon.h"
#include "Parser.h"
#include "Scansion.h"

#ifndef SPLC_RUNTIME_DIR
#define SPLC_RUNTIME_DIR "."
#endif

static void usage() {
  std::fprintf(stderr,
               "usage: splc [options] play.spl\n"
               "  -o <file>                 output file (default: a.out, or play.o / play.ll)\n"
               "  -c                        compile to an object file, do not link\n"
               "  -emit-llvm                write LLVM IR (.ll) instead of an object\n"
               "  -fpentameter=off|warn|error   check that every line of dialogue scans (default: warn)\n"
               "  -fpentameter-tolerance=N  stressed syllables allowed out of place (default: 1)\n"
               "  -fno-feminine-endings     disallow an 11th unstressed syllable\n"
               "  -fno-initial-trochee      disallow an inverted first foot\n"
               "  -fsyntax-only             parse and scan, produce nothing\n"
               "  --scan                    print the scansion of every line of dialogue and exit\n"
               "  --scan-text               scan a plain text file (every line is verse) and exit\n"
               "  --lexicon-size            print the number of words in the lexicon and exit\n"
               "  --runtime <dir>           where to find libsplrt.a (default: " SPLC_RUNTIME_DIR ")\n");
}

int main(int argc, char **argv) {
  std::string input, output, runtimeDir = SPLC_RUNTIME_DIR;
  bool compileOnly = false, emitLLVM = false, syntaxOnly = false, scanOnly = false, scanText = false;
  spl::ScansionOptions sopts;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) output = argv[++i];
    else if (a == "-c") compileOnly = true;
    else if (a == "-emit-llvm") emitLLVM = true;
    else if (a == "-fsyntax-only") syntaxOnly = true;
    else if (a == "--scan") scanOnly = true;
    else if (a == "--scan-text") scanText = true;
    else if (a == "--lexicon-size") { std::printf("%zu words\n", spl::Lexicon::size()); return 0; }
    else if (a == "--runtime" && i + 1 < argc) runtimeDir = argv[++i];
    else if (a == "-fpentameter=off") sopts.mode = spl::ScansionOptions::Off;
    else if (a == "-fpentameter=warn") sopts.mode = spl::ScansionOptions::Warn;
    else if (a == "-fpentameter=error") sopts.mode = spl::ScansionOptions::Error;
    else if (a.rfind("-fpentameter-tolerance=", 0) == 0) sopts.tolerance = std::atoi(a.c_str() + 23);
    else if (a == "-fno-feminine-endings") sopts.allowFeminine = false;
    else if (a == "-fno-initial-trochee") sopts.allowInitialTrochee = false;
    else if (a == "-h" || a == "--help") { usage(); return 0; }
    else if (a[0] == '-') { std::fprintf(stderr, "splc: unknown option %s\n", a.c_str()); usage(); return 2; }
    else if (input.empty()) input = a;
    else { std::fprintf(stderr, "splc: only one play at a time, please\n"); return 2; }
  }
  if (input.empty()) { usage(); return 2; }

  spl::SourceFile src;
  if (!src.load(input)) { std::fprintf(stderr, "splc: cannot read %s\n", input.c_str()); return 1; }
  spl::Diagnostics diag(src);
  spl::Lexer lexer(src, diag);
  std::vector<spl::Token> toks = lexer.tokenize();
  spl::Program prog;
  if (!scanText) {
    spl::Parser parser(toks, diag);
    prog = parser.parse();
  }

  if (scanOnly || scanText) {
    spl::ScansionOptions so = sopts;
    so.minWords = 1;
    spl::Scansion sc(toks, diag, so);
    std::vector<spl::DialogueLine> lines;
    if (scanText) {  // every line of the file is dialogue
      for (size_t i = 0; i < toks.size(); ++i) {
        if (toks[i].kind != spl::Tok::Word) continue;
        if (lines.empty() || lines.back().line != toks[i].loc.line) lines.push_back({toks[i].loc.line, {}});
        lines.back().tokens.push_back((int)i);
      }
    } else {
      for (const spl::Act &a : prog.acts)
        for (const spl::Scene &s : a.scenes)
          for (const spl::Item &it : s.items)
            if (it.kind == spl::Item::Speech) lines.insert(lines.end(), it.lines.begin(), it.lines.end());
    }
    int ok = 0;
    for (const spl::DialogueLine &dl : lines) {
      spl::ScansionResult r = sc.scanLine(dl);
      ok += r.scans;
      std::printf("%5d  %-12s %-4s  %s%s%s\n", dl.line, r.pattern.c_str(), r.scans ? "ok" : "FAIL",
                  src.lineText(dl.line).c_str(), r.scans ? "" : "   <-- ", r.explanation.c_str());
    }
    std::printf("%d of %zu lines scan\n", ok, lines.size());
    return 0;
  }

  spl::Scansion scansion(toks, diag, sopts);
  scansion.check(prog);
  if (diag.errors()) {
    std::fprintf(stderr, "%d error(s) generated.\n", diag.errors());
    return 1;
  }
  if (syntaxOnly) return 0;

  spl::CodeGen cg(prog, diag);
  if (!cg.generate()) return 1;

  std::string base = input;
  if (base.size() > 4 && base.compare(base.size() - 4, 4, ".spl") == 0) base.resize(base.size() - 4);
  if (emitLLVM) return cg.writeIR(output.empty() ? base + ".ll" : output) ? 0 : 1;
  if (compileOnly) return cg.writeObject(output.empty() ? base + ".o" : output) ? 0 : 1;

  std::string obj = base + ".o";
  if (!cg.writeObject(obj)) return 1;
  std::string exe = output.empty() ? "a.out" : output;
  std::string cmd = "cc -o '" + exe + "' '" + obj + "' '" + runtimeDir + "/libsplrt.a' -lm";
  int rc = std::system(cmd.c_str());
  unlink(obj.c_str());
  if (rc != 0) { std::fprintf(stderr, "splc: linking failed: %s\n", cmd.c_str()); return 1; }
  return 0;
}
