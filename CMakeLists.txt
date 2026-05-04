cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0110 NEW)

project(slang CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    src
    src/include
)

include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
    "
    #include <format>
    #include <print>
    #include <ranges>
    #include <vector>

    struct sample
    {
        int value{0};
        int get(this sample& self) { return self.value; }
    };

    int main()
    {
        auto out = std::vector<int>{1, 2, 3}
            | std::views::transform([](int v) { return v + 1; })
            | std::ranges::to<std::vector>();

        std::print(\"{}\", std::format(\"{}\", out[0]));

        sample s{42};
        return s.get() == 42 ? 0 : 1;
    }"
    SLANG_CXX23_FEATURES_AVAILABLE)

if(NOT SLANG_CXX23_FEATURES_AVAILABLE)
    message(FATAL_ERROR
        "slang requires a C++23 toolchain with <format>, <print>, std::ranges::to "
        "and explicit object parameter support. "
        "Detected ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}. "
        "Please use a newer toolchain (for example GCC 14+).")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(cxxopts REQUIRED)
find_package(Microsoft.GSL REQUIRED)

add_executable(slang
    src/archives/file.cpp
    src/compiler/ast/ast.cpp
    src/compiler/ast/builtins.cpp
    src/compiler/ast/eval.cpp
    src/compiler/ast/macros.cpp
    src/compiler/ast/node_ids.cpp
    src/compiler/ast/node_registry.cpp
    src/compiler/ast/resolve.cpp
    src/compiler/attribute.cpp
    src/compiler/cfg/simplify.cpp
    src/compiler/cfg/verify.cpp
    src/compiler/codegen/codegen.cpp
    src/compiler/collect.cpp
    src/compiler/constant.cpp
    src/compiler/emitter.cpp
    src/compiler/lexer.cpp
    src/compiler/loader.cpp
    src/compiler/location.cpp
    src/compiler/lowering.cpp
    src/compiler/macro.cpp
    src/compiler/parser.cpp
    src/compiler/resolve.cpp
    src/compiler/sema.cpp
    src/compiler/token.cpp
    src/compiler/typing.cpp
    src/interpreter/gc.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/module_loader.cpp
    src/shared/module.cpp
    src/shared/opcodes.cpp
    src/commandline/commandline.cpp
    src/commandline/compile.cpp
    src/commandline/disassemble.cpp
    src/commandline/format.cpp
    src/commandline/run.cpp
    src/commandline/runtime.cpp
    src/filemanager.cpp
    src/formatter/formatter.cpp
    src/main.cpp
    src/module_resolver.cpp
    src/package.cpp
    src/runtime/arrays.cpp
    src/runtime/debug.cpp
    src/runtime/gc.cpp
    src/runtime/math.cpp
    src/runtime/strings.cpp
    src/runtime/types.cpp
    src/runtime/utils.cpp
    src/utils.cpp
)
target_link_libraries(slang cxxopts::cxxopts Microsoft.GSL::GSL)

add_subdirectory(examples)
add_subdirectory(test)
