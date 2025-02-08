cmake_minimum_required(VERSION 3.15)

project(slang CXX)

include_directories(
    src
    src/include
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt REQUIRED)

add_executable(slang
    src/archives/file.cpp
    src/compiler/ast.cpp
    src/compiler/eval.cpp
    src/compiler/codegen.cpp
    src/compiler/emitter.cpp
    src/compiler/lexer.cpp
    src/compiler/parser.cpp
    src/compiler/typing.cpp
    src/interpreter/gc.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/module_loader.cpp
    src/shared/module.cpp
    src/shared/opcodes.cpp
    src/commandline.cpp
    src/compiler.cpp
    src/filemanager.cpp
    src/main.cpp
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
target_link_libraries(slang fmt::fmt)

add_subdirectory(examples)
add_subdirectory(test)
