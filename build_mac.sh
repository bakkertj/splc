#!/bin/sh
# Build and test splc on macOS with Homebrew LLVM; everything is logged to build_mac.log
cd "$(dirname "$0")"
{
  echo "== $(date) =="; sw_vers; uname -m
  echo "== toolchain =="; which cmake ninja clang python3; cmake --version | head -1
  LLVM_PREFIX=$(brew --prefix llvm 2>/dev/null || brew --prefix llvm@18 2>/dev/null)
  echo "LLVM_PREFIX=$LLVM_PREFIX"; "$LLVM_PREFIX/bin/llvm-config" --version
  GEN=""; command -v ninja >/dev/null && GEN="-G Ninja"
  echo "== configure =="
  cmake -S . -B build-mac $GEN -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm" || exit 1
  echo "== build =="
  cmake --build build-mac || exit 1
  echo "== tests =="
  ctest --test-dir build-mac --output-on-failure
  echo "== smoke =="
  ./build-mac/splc --lexicon-size
  ./build-mac/splc -fpentameter=error -o build-mac/hv examples/hello_verse.spl && ./build-mac/hv
  echo 30 | ./build-mac/primes 2>/dev/null || { ./build-mac/splc -fpentameter=off -o build-mac/primes examples/primes.spl && echo 30 | ./build-mac/primes; }
  echo "== done =="
} > build_mac.log 2>&1
tail -3 build_mac.log
