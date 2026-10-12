project(tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED CONFIG)

#
# Common warning flags interface target
#
add_library(slang_warning_flags INTERFACE)
target_compile_options(slang_warning_flags INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wdouble-promotion -Wchar-subscripts -Wnull-dereference -Wmisleading-indentation -Wuninitialized>
)

include_directories(
    ../src
)

#
# Core test libraries
#
add_library(slang_test_frontend_core STATIC
    ../src/archives/file.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/builtins.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_ids.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/ast/resolve.cpp
    ../src/compiler/attribute.cpp
    ../src/compiler/codegen/codegen.cpp
    ../src/compiler/collect.cpp
    ../src/compiler/constant.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/loader.cpp
    ../src/compiler/location.cpp
    ../src/compiler/lowering.cpp
    ../src/compiler/macro.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/resolve.cpp
    ../src/compiler/sema.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/filemanager.cpp
    ../src/module_resolver.cpp
    ../src/utils.cpp
)
target_link_libraries(slang_test_frontend_core PUBLIC Microsoft.GSL::GSL)

add_library(slang_test_emitter_core STATIC
    ../src/compiler/cfg/simplify.cpp
    ../src/compiler/cfg/verify.cpp
    ../src/compiler/emitter.cpp
    ../src/shared/opcodes.cpp
)
target_link_libraries(slang_test_emitter_core PUBLIC slang_test_frontend_core)

#
# Helper for creating GTest executables & registering CTest targets
#
function(add_gtest_executable target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE GTest::gtest GTest::gtest_main slang_warning_flags)

    string(REGEX REPLACE "^test_" "" clean_test_name "${target_name}")
    add_test(NAME "${clean_test_name}" COMMAND ${target_name})
    set_tests_properties("${clean_test_name}" PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
endfunction()

#
# Unit tests
#

add_gtest_executable(test_package
    test_package.cpp
    ../src/package.cpp
    ../src/utils.cpp
)

add_gtest_executable(test_lexer
    test_lexer.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/location.cpp
    ../src/compiler/token.cpp
)

add_gtest_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE slang_test_frontend_core)

add_gtest_executable(test_codegen
    test_codegen.cpp
    ../src/compiler/codegen/codegen.cpp
    ../src/compiler/collect.cpp
    ../src/compiler/constant.cpp
    ../src/compiler/location.cpp
    ../src/compiler/lowering.cpp
    ../src/compiler/sema.cpp
    ../src/compiler/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_codegen PRIVATE Microsoft.GSL::GSL)

add_gtest_executable(test_compile_ir test_compile_ir.cpp ../src/compiler/cfg/verify.cpp)
target_link_libraries(test_compile_ir PRIVATE slang_test_frontend_core)

add_gtest_executable(test_consteval test_consteval.cpp)
target_link_libraries(test_consteval PRIVATE slang_test_frontend_core)

add_gtest_executable(test_type_system test_type_system.cpp)
target_link_libraries(test_type_system PRIVATE slang_test_frontend_core)
set_tests_properties(type_system PROPERTIES FIXTURES_REQUIRED needs_std)

add_gtest_executable(test_resolve test_resolve.cpp)
target_link_libraries(test_resolve PRIVATE slang_test_frontend_core)

add_gtest_executable(test_cfg_simplify test_cfg_simplify.cpp)
target_link_libraries(test_cfg_simplify PRIVATE slang_test_emitter_core)

add_gtest_executable(test_output test_output.cpp)
target_link_libraries(test_output PRIVATE slang_test_emitter_core)
set_tests_properties(output PROPERTIES
    FIXTURES_REQUIRED needs_std
    FIXTURES_SETUP output
)

add_gtest_executable(test_serialization
    test_serialization.cpp
    ../src/archives/file.cpp
)

add_gtest_executable(test_ast_serialization test_ast_serialization.cpp)
target_link_libraries(test_ast_serialization PRIVATE slang_test_frontend_core)
set_tests_properties(ast_serialization PROPERTIES FIXTURES_REQUIRED needs_std)

add_gtest_executable(test_filemanager
    test_filemanager.cpp
    ../src/archives/file.cpp
    ../src/filemanager.cpp
)

add_gtest_executable(test_formatter
    test_formatter.cpp
    ../src/formatter/formatter.cpp
)
target_link_libraries(test_formatter PRIVATE slang_test_frontend_core)

add_gtest_executable(test_interpreter
    test_interpreter.cpp
    ../src/archives/file.cpp
    ../src/runtime/arrays.cpp
    ../src/runtime/debug.cpp
    ../src/runtime/gc.cpp
    ../src/runtime/math.cpp
    ../src/runtime/strings.cpp
    ../src/runtime/types.cpp
    ../src/runtime/utils.cpp
    ../src/interpreter/gc.cpp
    ../src/interpreter/interpreter.cpp
    ../src/interpreter/module_loader.cpp
    ../src/shared/module.cpp
    ../src/shared/opcodes.cpp
    ../src/filemanager.cpp
    ../src/module_resolver.cpp
)
target_link_libraries(test_interpreter PRIVATE Microsoft.GSL::GSL)
set_tests_properties(interpreter PROPERTIES FIXTURES_REQUIRED output)

add_gtest_executable(test_vector test_vector.cpp)

if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    add_gtest_executable(test_jit_aarch64
        ../src/archives/file.cpp
        ../src/interpreter/gc.cpp
        ../src/jit/aarch64/cfg.cpp
        ../src/jit/aarch64/compiler.cpp
        ../src/jit/aarch64/emitter.cpp
        ../src/jit/aarch64/module_loader.cpp
        ../src/shared/module.cpp
        ../src/shared/opcodes.cpp
        ../src/filemanager.cpp
        test_jit_aarch64.cpp
    )
    target_link_libraries(test_jit_aarch64 PRIVATE Microsoft.GSL::GSL)
    set_tests_properties(jit_aarch64 PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} FIXTURES_REQUIRED generate_jit_test)
endif()

#
# Command line tests
#

include(cmake/TestHelpers.cmake)

add_compile_test(
    "build std"
    FALSE
    src/lang/std -o lang/std
)
set_tests_properties(
    "build std (default, compile)"
    PROPERTIES FIXTURES_SETUP needs_std
)

add_compile_test(
    "build gc"
    FALSE
    src/lang/gc -o lang/gc
)
set_tests_properties(
    "build gc (default, compile)"
    PROPERTIES FIXTURES_SETUP needs_gc
)

if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    add_test(
        NAME "build jit library (default, compile)"
        COMMAND ${CMAKE_BINARY_DIR}/slang compile test/test_jit_library
    )
    set_tests_properties("build jit library (default, compile)" PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        FIXTURES_SETUP generate_jit_library
    )

    add_compile_test(
        "build jit test"
        FALSE
        --search-path test
        test/test_jit
    )
    set_tests_properties(
        "build jit test (default, compile)"
        PROPERTIES
        FIXTURES_REQUIRED generate_jit_library
        FIXTURES_SETUP generate_jit_test
    )
    set_tests_properties(
        "build jit test (--no-eval-const-subexpr, compile)"
        PROPERTIES FIXTURES_REQUIRED generate_jit_library
    )
endif()

add_compile_exec_test("slang array" TRUE test/test_array)
add_jit_compile_test("slang array" FALSE test/test_array)

add_compile_fail_test("slang array fail 1" TRUE test/test_array_fail1)
add_compile_fail_test("slang array fail 2" TRUE test/test_array_fail2)
add_compile_exec_fail_test("slang array fail 3" TRUE test/test_array_fail3)

add_compile_exec_test("slang builtin types" TRUE test/test_builtin_types)
add_jit_compile_test("slang builtin types" TRUE test/test_builtin_types)

add_compile_exec_test("slang cast" TRUE test/test_cast)
add_jit_compile_test("slang cast" TRUE test/test_cast)

add_compile_exec_fail_test("slang cast fail" TRUE test/test_cast_fail)

add_compile_fail_test("slang const assign fail" FALSE test/test_const_assign_fail)

add_compile_exec_test("slang const export" TRUE test/test_const_export)
add_jit_compile_test("slang const export" TRUE test/test_const_export)

add_compile_exec_test("slang const import" TRUE test/test_const_import)
add_jit_compile_test("slang const import" TRUE test/test_const_import)

add_compile_exec_test("slang conversions" TRUE test/test_conversions)
add_jit_compile_test("slang conversions" TRUE test/test_conversions)

add_compile_exec_test("slang eval subexpr" TRUE test/test_eval_subexpr)
add_jit_compile_test("slang eval subexpr" TRUE test/test_eval_subexpr)

add_compile_exec_test("slang expressions" TRUE test/test_expressions)
add_jit_compile_test("slang expressions" TRUE test/test_expressions)

add_compile_exec_test("slang integer wrap" TRUE test/test_integer_wrap)
add_jit_compile_test("slang integer wrap" TRUE test/test_integer_wrap)

add_compile_exec_fail_test("slang invalid main signature 1" FALSE test/test_invalid_main_signature1)
add_compile_exec_fail_test("slang invalid main signature 2" FALSE test/test_invalid_main_signature2)

add_compile_exec_test("slang macros" TRUE test/macros/test_macros)
add_jit_compile_test("slang macros" TRUE test/macros/test_macros)

add_compile_exec_test("slang macros recursion" TRUE test/macros/test_recursion)
add_jit_compile_test("slang macros recursion" TRUE test/macros/test_recursion)

add_compile_exec_test("slang macros const" TRUE test/macros/test_const)
add_jit_compile_test("slang macros const" TRUE test/macros/test_const)

add_compile_fail_test("slang macros fail 1" FALSE test/macros/test_fail1)
add_compile_fail_test("slang macros fail 2" FALSE test/macros/test_fail2)
add_compile_fail_test("slang macros fail 3" FALSE test/macros/test_fail3)
add_compile_fail_test("slang macros fail 4" FALSE test/macros/test_fail4)
add_compile_fail_test("slang macros fail 5" FALSE test/macros/test_fail5)
add_compile_fail_test("slang macros fail 6" FALSE test/macros/test_fail6 --search-path test/macros)
add_compile_fail_test("slang macros fail 7" FALSE test/macros/test_fail7 --search-path test/macros)
add_compile_fail_test("slang macros fail 8" FALSE test/macros/test_fail8)
add_compile_fail_test("slang macros fail 9" FALSE test/macros/test_fail9)
add_compile_fail_test("slang macros fail 10" FALSE test/macros/test_fail10)
add_compile_fail_test("slang macros fail 11" FALSE test/macros/test_fail11)
add_compile_fail_test("slang macros fail 12" FALSE test/macros/test_fail12)

add_compile_fail_test("slang macros format fail 1" TRUE test/macros/test_format_fail1)
add_compile_fail_test("slang macros format fail 2" TRUE test/macros/test_format_fail2)
add_compile_fail_test("slang macros format fail 3" TRUE test/macros/test_format_fail3)

add_compile_exec_test("slang macro import" TRUE test/macros/test_import --search-path test/macros)
set_tests_properties("slang macro import (default, compile)" PROPERTIES FIXTURES_REQUIRED test_macros)

add_jit_compile_test("slang macro import" TRUE test/macros/test_import --search-path test/macros)
set_tests_properties("slang macro import (JIT)" PROPERTIES FIXTURES_REQUIRED test_macros)

add_compile_exec_test("slang macro transitive import 1" TRUE test/macros/test_transitive_import1 --search-path test/macros)
set_tests_properties("slang macro transitive import 1 (default, compile)" PROPERTIES FIXTURES_REQUIRED test_import)

add_jit_compile_test("slang macro transitive import 1" TRUE test/macros/test_transitive_import1 --search-path test/macros)
set_tests_properties("slang macro transitive import 1 (JIT)" PROPERTIES FIXTURES_REQUIRED test_import)

add_compile_exec_test("slang macro transitive import 2" TRUE test/macros/test_transitive_import2 --search-path test/macros)
set_tests_properties("slang macro transitive import 2 (default, compile)" PROPERTIES FIXTURES_REQUIRED test_transitive_import1)

add_jit_compile_test("slang macro transitive import 2" TRUE test/macros/test_transitive_import2 --search-path test/macros)
set_tests_properties("slang macro transitive import 2 (JIT)" PROPERTIES FIXTURES_REQUIRED test_transitive_import1)

add_compile_exec_test("slang macro struct" TRUE test/macros/test_struct)
set_tests_properties("slang macro struct (default, compile)" PROPERTIES FIXTURES_REQUIRED needs_std)

add_jit_compile_test("slang macro struct" TRUE test/macros/test_struct)
set_tests_properties("slang macro struct (JIT)" PROPERTIES FIXTURES_REQUIRED needs_std)

add_compile_exec_test("slang math" TRUE test/test_math)
add_jit_compile_test("slang math" TRUE test/test_math)

add_compile_exec_test("slang expression without effect" FALSE test/test_expr_no_effect)
add_jit_compile_test("slang expression without effect" FALSE test/test_expr_no_effect)

add_compile_exec_test("slang operators" TRUE test/test_operators)
add_jit_compile_test("slang operators" TRUE test/test_operators)

add_compile_exec_test("slang scopes" TRUE test/test_scopes)
add_jit_compile_test("slang scopes" TRUE test/test_scopes)

add_compile_exec_test("slang strings" TRUE test/test_strings)
add_jit_compile_test("slang strings" TRUE test/test_strings)

add_compile_exec_test("slang structs" TRUE test/test_structs)
add_jit_compile_test("slang structs" TRUE test/test_structs)

add_compile_exec_test("slang struct import" TRUE test/test_struct_import)
add_jit_compile_test("slang struct import" TRUE test/test_struct_import)

add_compile_exec_test("slang unreachable" TRUE test/test_unreachable)
add_jit_compile_test("slang unreachable" TRUE test/test_unreachable)

#
# Tests with other package dependencies
#

add_compile_test("slang example_package/hello" TRUE test/example_package/hello)
set_tests_properties("slang example_package/hello (default, compile)" PROPERTIES FIXTURES_SETUP example_package_hello)

add_compile_exec_test("slang test_import" TRUE test/test_import --search-path test)
set_tests_properties(
    "slang test_import (default, compile)"
    "slang test_import (--no-eval-const-subexpr, compile)"
    "slang test_import (default, run)"
    "slang test_import (--no-eval-const-subexpr, run)"
    PROPERTIES FIXTURES_REQUIRED example_package_hello
)

add_jit_compile_test("slang test_import" TRUE test/test_import --search-path test)
set_tests_properties("slang test_import (JIT)" PROPERTIES FIXTURES_REQUIRED example_package_hello)

#
# GC interface test
#

add_my_test("slang gc interface (default, compile)" ${CMAKE_BINARY_DIR}/slang compile test/test_gc_interface)
add_my_test("slang gc interface (default, run)" ${CMAKE_BINARY_DIR}/slang run test/test_gc_interface)

set_tests_properties(
    "slang gc interface (default, compile)"
    "slang gc interface (default, run)"
    PROPERTIES FIXTURES_REQUIRED needs_gc
)
set_tests_properties(
    "slang gc interface (default, run)"
    PROPERTIES FIXTURES_REQUIRED "slang gc interface (default, compile)"
)

add_jit_compile_test("slang gc interface" FALSE test/test_gc_interface)
set_tests_properties("slang gc interface (JIT)" PROPERTIES FIXTURES_SETUP needs_gc)

#
# Examples
#

add_compile_exec_test("slang examples/hello_world" TRUE examples/hello_world)
add_jit_compile_test("slang examples/hello_world" TRUE examples/hello_world)

add_compile_exec_test("slang examples/array_loop" TRUE examples/array_loop)
add_jit_compile_test("slang examples/array_loop" TRUE examples/array_loop)

add_compile_exec_test("slang examples/structs" TRUE examples/structs)
add_jit_compile_test("slang examples/structs" TRUE examples/structs)

add_compile_exec_test("slang examples/linked_list" TRUE examples/linked_list)
add_jit_compile_test("slang examples/linked_list" TRUE examples/linked_list)

add_compile_exec_test("slang examples/print_args" TRUE examples/print_args)
add_jit_compile_test("slang examples/print_args" TRUE examples/print_args)

add_my_test("slang examples/print_args -- Hello World" ${CMAKE_BINARY_DIR}/slang run examples/print_args -- Hello World)
set_tests_properties("slang examples/print_args -- Hello World" PROPERTIES FIXTURES_REQUIRED needs_std)

add_my_jit_test("slang examples/print_args -- Hello World (JIT)" ${CMAKE_BINARY_DIR}/slang run -j examples/print_args -- Hello World)
set_tests_properties("slang examples/print_args -- Hello World (JIT)" PROPERTIES FIXTURES_REQUIRED needs_std)

add_compile_exec_test("slang examples/string_conversion" TRUE examples/string_conversion)
add_jit_compile_test("slang examples/string_conversion" TRUE examples/string_conversion)

add_my_test("slang examples/string_conversion -- 3.141 a -1" ${CMAKE_BINARY_DIR}/slang run examples/string_conversion -- 3.141 a -1)
set_tests_properties("slang examples/string_conversion -- 3.141 a -1" PROPERTIES FIXTURES_REQUIRED needs_std)

add_my_jit_test("slang examples/string_conversion -- 3.141 a -1 (JIT)" ${CMAKE_BINARY_DIR}/slang run -j examples/string_conversion -- 3.141 a -1)
set_tests_properties("slang examples/string_conversion -- 3.141 a -1 (JIT)" PROPERTIES FIXTURES_REQUIRED needs_std)

add_compile_exec_fail_test("slang examples/assert" TRUE examples/assert)

#
# Native integration example/test
#

add_compile_test("slang native_integration" FALSE examples/native_integration)
set_tests_properties("slang native_integration (default, compile)" PROPERTIES FIXTURES_SETUP native_integration)

add_test(NAME "native_integration" COMMAND ${CMAKE_BINARY_DIR}/examples/native_integration)
set_tests_properties("native_integration" PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    FIXTURES_REQUIRED native_integration
)

#
# Formatter CLI end-to-end test
#

add_test(
    NAME formatter_cli_e2e
    COMMAND ${CMAKE_COMMAND}
    -DSLANG_BIN=${CMAKE_BINARY_DIR}/slang
    -DTEST_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/..
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FmtCliE2E.cmake
)
set_tests_properties(formatter_cli_e2e PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

#
# Formatting check targets
#

add_test(NAME "check formatting (examples)" COMMAND ${CMAKE_BINARY_DIR}/slang fmt --check examples)
set_tests_properties("check formatting (examples)" PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_test(NAME "check formatting (src)" COMMAND ${CMAKE_BINARY_DIR}/slang fmt --check src)
set_tests_properties("check formatting (src)" PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_test(NAME "check formatting (test)" COMMAND ${CMAKE_BINARY_DIR}/slang fmt --check test)
set_tests_properties("check formatting (test)" PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
