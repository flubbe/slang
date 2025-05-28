cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0110 NEW)

project(slang CXX)

enable_testing()

include_directories(
    src
    src/include
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(cxxopts REQUIRED)
find_package(fmt REQUIRED)
find_package(Microsoft.GSL REQUIRED)

add_executable(slang
    src/archives/file.cpp
    src/compiler/builtins/format.cpp
    src/compiler/ast/ast.cpp
    src/compiler/ast/eval.cpp
    src/compiler/ast/macros.cpp
    src/compiler/ast/node_registry.cpp
    src/compiler/codegen.cpp
    src/compiler/emitter.cpp
    src/compiler/lexer.cpp
    src/compiler/parser.cpp
    src/compiler/token.cpp
    src/compiler/typing.cpp
    src/interpreter/gc.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/module_loader.cpp
    src/shared/module.cpp
    src/shared/opcodes.cpp
    src/commandline/commandline.cpp
    src/commandline/compile.cpp
    src/commandline/run.cpp
    src/filemanager.cpp
    src/main.cpp
    src/module_resolver.cpp
    src/package.cpp
    src/resolve.cpp
    src/runtime/arrays.cpp
    src/runtime/debug.cpp
    src/runtime/math.cpp
    src/runtime/strings.cpp
    src/runtime/types.cpp
    src/runtime/utils.cpp
    src/utils.cpp
)
target_link_libraries(slang cxxopts::cxxopts fmt::fmt Microsoft.GSL::GSL)

add_subdirectory(examples)
add_subdirectory(test)
