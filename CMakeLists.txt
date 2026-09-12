# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
cmake_minimum_required(VERSION 3.20)
project(splc CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# --- lexicon (regenerate with `cmake --build . --target lexicon`) -------------
find_package(Python3 COMPONENTS Interpreter)
add_custom_target(lexicon
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_lexicon.py
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Regenerating generated/Lexicon.inc from data/")

# --- runtime ------------------------------------------------------------------
add_library(splrt STATIC runtime/splrt.c)
target_compile_options(splrt PRIVATE -O2 -fPIC)

# --- compiler -----------------------------------------------------------------
add_executable(splc
  src/main.cpp src/Diagnostics.cpp src/Lexicon.cpp src/Lexer.cpp
  src/Parser.cpp src/Scansion.cpp src/CodeGen.cpp)
target_include_directories(splc PRIVATE src generated)
target_compile_definitions(splc PRIVATE SPLC_RUNTIME_DIR="${CMAKE_BINARY_DIR}")
# The lexicon table is large; keep its translation unit cheap to compile.
set_source_files_properties(src/Lexicon.cpp PROPERTIES COMPILE_OPTIONS "-O1")
llvm_map_components_to_libnames(llvm_libs core support native mc passes)
list(REMOVE_DUPLICATES llvm_libs)
target_link_libraries(splc PRIVATE ${llvm_libs})
if(APPLE)
  # The LLVM component archives overlap; Apple's ld ≥ 1015 warns about it unless told not to.
  target_link_options(splc PRIVATE LINKER:-no_warn_duplicate_libraries)
endif()
add_dependencies(splc splrt)

# --- tests --------------------------------------------------------------------
enable_testing()
add_test(NAME hello COMMAND sh ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello.spl "Hello World!")
add_test(NAME primes COMMAND sh ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/primes.spl "2 3 5 7 11 13 17 19 " 20)
add_test(NAME pentameter_error COMMAND sh ${CMAKE_SOURCE_DIR}/test/expect_fail.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello.spl -fpentameter=error)
add_test(NAME pentameter_ok COMMAND $<TARGET_FILE:splc> -fsyntax-only -fpentameter=error ${CMAKE_SOURCE_DIR}/examples/hello_verse.spl)
add_test(NAME hello_verse COMMAND sh ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello_verse.spl "Hello World!")
add_test(NAME prose_exempt COMMAND $<TARGET_FILE:splc> -fsyntax-only -fpentameter=error ${CMAKE_SOURCE_DIR}/examples/couplets.spl)
add_test(NAME couplet_missing COMMAND sh ${CMAKE_SOURCE_DIR}/test/expect_fail.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/couplets.spl -fpentameter=error -fcouplets)
add_test(NAME no_prose_exemption COMMAND sh ${CMAKE_SOURCE_DIR}/test/expect_fail.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/couplets.spl -fpentameter=error -fno-prose-exemption)
set(FIZZBUZZ_EXPECTED "1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\nBuzz\n11\nFizz\n13\n14\nFizzBuzz\n16\n17\nFizz\n19\nBuzz\nFizz\n22\n23\nFizz\nBuzz\n26\nFizz\n28\n29\nFizzBuzz\n31\n32\nFizz\n34\nBuzz\nFizz\n37\n38\nFizz\nBuzz\n41\nFizz\n43\n44\nFizzBuzz\n46\n47\nFizz\n49\nBuzz\nFizz\n52\n53\nFizz\nBuzz\n56\nFizz\n58\n59\nFizzBuzz\n61\n62\nFizz\n64\nBuzz\nFizz\n67\n68\nFizz\nBuzz\n71\nFizz\n73\n74\nFizzBuzz\n76\n77\nFizz\n79\nBuzz\nFizz\n82\n83\nFizz\nBuzz\n86\nFizz\n88\n89\nFizzBuzz\n91\n92\nFizz\n94\nBuzz\nFizz\n97\n98\nFizz\nBuzz")
add_test(NAME fizzbuzz COMMAND sh ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/fizzbuzz.spl "${FIZZBUZZ_EXPECTED}")
add_test(NAME reverse COMMAND sh ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/reverse.spl "olleh" hello)
set(SONNET_EXPECTED "Hi!\nHo!")
add_test(NAME sonnet COMMAND sh ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/sonnet.spl "${SONNET_EXPECTED}")
add_test(NAME sonnet_strict COMMAND $<TARGET_FILE:splc> -fsyntax-only -fsonnet -fpentameter=error ${CMAKE_SOURCE_DIR}/examples/sonnet.spl)
add_test(NAME sonnet_not COMMAND sh ${CMAKE_SOURCE_DIR}/test/expect_fail.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello_verse.spl -fsonnet -fpentameter=error)
