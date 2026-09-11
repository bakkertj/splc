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
target_link_libraries(splc PRIVATE ${llvm_libs})
add_dependencies(splc splrt)

# --- tests --------------------------------------------------------------------
enable_testing()
add_test(NAME hello COMMAND ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello.spl "Hello World!")
add_test(NAME primes COMMAND ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/primes.spl "2 3 5 7 11 13 17 19 " 20)
add_test(NAME pentameter_error COMMAND ${CMAKE_SOURCE_DIR}/test/expect_fail.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello.spl -fpentameter=error)
add_test(NAME pentameter_ok COMMAND $<TARGET_FILE:splc> -fsyntax-only -fpentameter=error ${CMAKE_SOURCE_DIR}/examples/hello_verse.spl)
add_test(NAME hello_verse COMMAND ${CMAKE_SOURCE_DIR}/test/run.sh $<TARGET_FILE:splc> ${CMAKE_SOURCE_DIR}/examples/hello_verse.spl "Hello World!")
