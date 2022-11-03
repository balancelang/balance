CMAKE_MINIMUM_REQUIRED(VERSION 3.7 FATAL_ERROR)
project(Balance VERSION 0.0.7)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g")       # enables debugging
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING 1)
# set(CMAKE_C_COMPILER "/usr/bin/gcc")
# set(CMAKE_CXX_COMPILER "/usr/bin/g++")

find_package(Threads REQUIRED)
# set(ENABLE_LLVM_SHARED ON)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")

include("rapidjson")

macro(find_program_or_die OUTPUT_VAR PROG_NAME DOCSTRING)
  find_program(${OUTPUT_VAR}
    NAMES "${PROG_NAME}${LLVM_BINARY_SUFFIX}${CMAKE_EXECUTABLE_SUFFIX}" "${PROG_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
    HINTS "${LLVM_BINDIR}" "${LLVM_CONFIG_LOCATION}" "${LLVM_PREFIX}" "${LLVM_PREFIX_BIN}"
    DOC "${DOCSTRING}"
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
  )
  if(${OUTPUT_VAR})
    message(STATUS "Found ${PROG_NAME}: ${${OUTPUT_VAR}}")
  else()
    message(FATAL_ERROR "${PROG_NAME} executable not found!")
  endif()
endmacro()

find_package(Clang REQUIRED)
find_program_or_die( CLANGXX "clang++-12" "clang++ binary")
execute_process(COMMAND "${CLANGXX}" "--version" OUTPUT_VARIABLE LLVM_CLANGXX_VERSION RESULT_VARIABLE CLANGXX_RES)

include_directories(${RAPIDJSON_INCLUDE_DIR})

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in.cmake ${CMAKE_BINARY_DIR}/generated/config.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ )

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
add_definitions(${CLANG_DEFINITIONS})


# required if linking to static library
add_definitions(-DANTLR4CPP_STATIC -DENABLE_LLVM_SHARED=ON)

# using /MD flag for antlr4_runtime (for Visual C++ compilers only)
set(ANTLR4_WITH_STATIC_CRT OFF)

# Specify the version of the antlr4 library needed for this project.
# By default the latest version of antlr4 will be used.  You can specify a
# specific, stable version by setting a repository tag value or a link
# to a zip file containing the libary source.
set(ANTLR4_TAG 4.10.1)
# set(ANTLR4_ZIP_REPOSITORY https://github.com/antlr/antlr4/archive/refs/tags/4.10.1.zip)

# add external build for antlrcpp
include(ExternalAntlr4Cpp)
# add antrl4cpp artifacts to project environment
include_directories(${ANTLR4_INCLUDE_DIRS})

# set variable pointing to the antlr tool that supports C++
# this is not required if the jar file can be found under PATH environment

include (FetchContent)
FetchContent_Declare(
  AntlrJar
  URL      https://www.antlr.org/download/antlr-4.10.1-complete.jar
  DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/dependencies
  DOWNLOAD_NO_EXTRACT TRUE
)

FetchContent_GetProperties(AntlrJar)
if(NOT AntlrJar_POPULATED)
  FetchContent_Populate(AntlrJar)
endif()

set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/dependencies/antlr-4.10.1-complete.jar)
# add macros to generate ANTLR Cpp code from grammar
find_package(ANTLR REQUIRED)

# Call macro to add lexer and grammar to your build dependencies.
antlr_target(BalanceGrammarLexer BalanceLexer.g4 LEXER
             PACKAGE antlrcpptest)
antlr_target(BalanceGrammarParser BalanceParser.g4 PARSER VISITOR
             PACKAGE antlrcpptest
             DEPENDS_ANTLR BalanceGrammarLexer
             COMPILE_FLAGS -lib ${ANTLR_BalanceGrammarLexer_OUTPUT_DIR})

# include generated files in project environment
include_directories(${ANTLR_BalanceGrammarLexer_OUTPUT_DIR})
include_directories(${ANTLR_BalanceGrammarParser_OUTPUT_DIR})


FetchContent_Declare(
    lspcpp
    GIT_REPOSITORY https://github.com/kuafuwang/LspCpp.git
)
FetchContent_MakeAvailable(lspcpp)

# add generated grammar to the binary target
add_executable(balance
               src/Main.cpp
               src/Builtins.cpp
               src/Package.cpp
               src/Utilities.cpp
               src/models/BalanceClass.cpp
               src/models/BalanceFunction.cpp
               src/models/BalanceModule.cpp
               src/models/BalanceParameter.cpp
               src/models/BalanceProperty.cpp
               src/models/BalanceTypeString.cpp
               src/visitors/Visitor.cpp
               src/visitors/PackageVisitor.cpp
               src/visitors/StructureVisitor.cpp
               src/visitors/LLVMTypeVisitor.cpp
               src/visitors/TypeVisitor.cpp
               src/visitors/ForwardDeclarationVisitor.cpp
               src/visitors/ConstructorVisitor.cpp
               src/builtins/File.cpp
               src/builtins/String.cpp
               src/builtins/Int.cpp
               src/builtins/Double.cpp
               src/builtins/Bool.cpp
               src/builtins/Array.cpp
               src/language-server/LanguageServer.cpp
               tests/ASTTests.h
               tests/ASTTests.cpp
               tests/CompileTests.h
               tests/CompileTests.cpp
               tests/ExampleTests.h
               tests/ExampleTests.cpp
               ${ANTLR_BalanceGrammarLexer_CXX_OUTPUTS}
               ${ANTLR_BalanceGrammarParser_CXX_OUTPUTS})

# run this to get list of components: llvm-config-12 --components
llvm_map_components_to_libnames(llvm_libs support core irreader mc target ${LLVM_TARGETS_TO_BUILD})

target_include_directories(balance PRIVATE lspcpp)
target_link_libraries(balance lspcpp)

target_link_libraries(balance antlr4_static)
target_link_libraries(balance Threads::Threads)
target_link_libraries(balance LLVM clangTooling clangFrontend clangSerialization clangDriver clangParse clangSema clangAnalysis clangAST clangBasic clangEdit clangLex)
add_dependencies(balance rapidjson)