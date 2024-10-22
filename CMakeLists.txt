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
    src/ast.cpp
    src/codegen.cpp
    src/commandline.cpp
    src/compiler.cpp
    src/emitter.cpp
    src/filemanager.cpp
    src/gc.cpp
    src/interpreter.cpp
    src/lexer.cpp
    src/main.cpp
    src/module.cpp
    src/package.cpp
    src/parser.cpp
    src/opcodes.cpp
    src/resolve.cpp
    src/runtime/arrays.cpp
    src/runtime/debug.cpp
    src/runtime/strings.cpp
    src/runtime/utils.cpp
    src/typing.cpp
    src/utils.cpp
)
target_link_libraries(slang fmt::fmt)

add_subdirectory(test)
