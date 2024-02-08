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
    src/ast.cpp
    src/codegen.cpp
    src/commandline.cpp
    src/compiler.cpp
    src/lexer.cpp
    src/main.cpp
    src/package.cpp
    src/parser.cpp
    src/utils.cpp
)
target_link_libraries(slang fmt::fmt)

add_subdirectory(test)
