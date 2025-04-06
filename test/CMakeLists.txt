project(tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt REQUIRED)
find_package(GTest REQUIRED CONFIG)

set(COMPILE_OPTIONS_MSVC $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>)
set(COMPILE_OPTIONS_NOT_MSVC $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wdouble-promotion -Wchar-subscripts -Wnull-dereference -Wmisleading-indentation -Wuninitialized>)

include_directories(
    ../src
)

add_executable(test_package
    test_package.cpp
    ../src/package.cpp
    ../src/utils.cpp
)
target_link_libraries(test_package fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_package PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME package COMMAND test_package)

add_executable(test_lexer
    test_lexer.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/token.cpp
)
target_link_libraries(test_lexer fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_lexer PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME lexer COMMAND test_lexer)

add_executable(test_parser
    test_parser.cpp
    ../src/compiler/builtins/format.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/utils.cpp
)
target_link_libraries(test_parser fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_parser PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME parser COMMAND test_parser)

add_executable(test_codegen
    test_codegen.cpp
    ../src/compiler/builtins/format.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/utils.cpp
)
target_link_libraries(test_codegen fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_codegen PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME codegen COMMAND test_codegen)

add_executable(test_compile_ir
    test_compile_ir.cpp
    ../src/compiler/builtins/format.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/utils.cpp
)
target_link_libraries(test_compile_ir fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_compile_ir PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME compile_ir COMMAND test_compile_ir)

add_executable(test_type_system
    test_type_system.cpp
    ../src/compiler/builtins/format.cpp
    ../src/archives/file.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/filemanager.cpp
    ../src/module_resolver.cpp
    ../src/resolve.cpp
    ../src/utils.cpp
)
target_link_libraries(test_type_system fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_type_system PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME type_system COMMAND test_type_system)
set_tests_properties(type_system PROPERTIES 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    FIXTURES_REQUIRED needs_std
)

add_executable(test_resolve
    test_resolve.cpp
    ../src/compiler/builtins/format.cpp
    ../src/archives/file.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/filemanager.cpp
    ../src/module_resolver.cpp
    ../src/resolve.cpp
    ../src/utils.cpp
)
target_link_libraries(test_resolve fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_resolve PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME resolve COMMAND test_resolve)
set_tests_properties(resolve PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(type_system PROPERTIES 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    FIXTURES_REQUIRED needs_std
)

add_executable(test_output
    test_output.cpp
    ../src/archives/file.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/builtins/format.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/emitter.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/shared/opcodes.cpp
    ../src/filemanager.cpp
    ../src/module_resolver.cpp
    ../src/resolve.cpp
    ../src/utils.cpp
)
target_link_libraries(test_output fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_output PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME output COMMAND test_output)
set_tests_properties(output PROPERTIES 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    FIXTURES_REQUIRED needs_std
    FIXTURES_SETUP output
)

add_executable(test_serialization
    test_serialization.cpp
    ../src/archives/file.cpp
)
target_link_libraries(test_serialization fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_serialization PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME serialization COMMAND test_serialization)

add_executable(test_ast_serialization
    test_ast_serialization.cpp
    ../src/compiler/ast/ast.cpp
    ../src/compiler/ast/eval.cpp
    ../src/compiler/ast/macros.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/builtins/format.cpp
    ../src/archives/file.cpp
    ../src/compiler/ast/node_registry.cpp
    ../src/compiler/codegen.cpp
    ../src/compiler/lexer.cpp
    ../src/compiler/parser.cpp
    ../src/compiler/token.cpp
    ../src/compiler/typing.cpp
    ../src/shared/module.cpp
    ../src/filemanager.cpp
    ../src/module_resolver.cpp
    ../src/resolve.cpp
    ../src/utils.cpp
)
target_link_libraries(test_ast_serialization fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_ast_serialization PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME ast_serialization COMMAND test_ast_serialization)
set_tests_properties(ast_serialization PROPERTIES 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    FIXTURES_REQUIRED needs_std
)

add_executable(test_filemanager
    test_filemanager.cpp
    ../src/archives/file.cpp
    ../src/filemanager.cpp
)
target_link_libraries(test_filemanager fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_filemanager PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME filemanager COMMAND test_filemanager)
set_tests_properties(filemanager PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(test_interpreter
    test_interpreter.cpp
    ../src/archives/file.cpp
    ../src/runtime/arrays.cpp
    ../src/runtime/debug.cpp
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
target_link_libraries(test_interpreter fmt::fmt GTest::gtest GTest::gtest_main Microsoft.GSL::GSL)
target_compile_options(test_interpreter PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME interpreter COMMAND test_interpreter)
set_tests_properties(interpreter PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(type_system PROPERTIES 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    FIXTURES_REQUIRED output
)

add_executable(test_vector
    test_vector.cpp
)
target_link_libraries(test_vector fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_vector PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
add_test(NAME vector COMMAND test_vector)

#
# Command line tests.
#

include(cmake/TestHelpers.cmake)

add_compile_test(
    "build std"
    FALSE
    src/lang/std -o lang/std
)
set_tests_properties(
    "build std (default, compile)" 
    PROPERTIES FIXTURES_SETUP 
    needs_std
)

add_compile_exec_test(
    "slang array"
    TRUE
    test/test_array
)
add_compile_fail_test(
    "slang array fail 1"
    TRUE
    test/test_array_fail1
)
add_compile_fail_test(
    "slang array fail 2"
    TRUE
    test/test_array_fail2
)
add_compile_exec_fail_test(
    "slang array fail 3"
    TRUE
    test/test_array_fail3
)
add_compile_exec_test(
    "slang cast"
    TRUE
    test/test_cast
)
add_compile_exec_fail_test(
    "slang cast fail"
    TRUE
    test/test_cast_fail
)
add_compile_fail_test(
    "slang const assign fail"
    FALSE
    test/test_const_assign_fail
)
add_compile_exec_test(
    "slang const export"
    TRUE
    test/test_const_export
)
add_compile_exec_test(
    "slang const import"
    TRUE
    test/test_const_import
)
add_compile_exec_test(
    "slang conversions"
    TRUE
    test/test_conversions
)
add_compile_exec_test(
    "slang eval subexpr"
    TRUE
    test/test_eval_subexpr
)
add_compile_exec_fail_test(
    "slang invalid main signature 1"
    FALSE
    test/test_invalid_main_signature1
)
add_compile_exec_fail_test(
    "slang invalid main signature 2"
    FALSE
    test/test_invalid_main_signature2
)
add_compile_exec_test(
    "slang macros"
    TRUE
    test/test_macros
)
add_compile_exec_test(
    "slang macros recursive"
    TRUE
    test/test_macros_recursive
)
add_compile_fail_test(
    "slang macros fail 1"
    FALSE
    test/test_macros_fail1
)
add_compile_fail_test(
    "slang macros fail 2"
    FALSE
    test/test_macros_fail2
)
add_compile_fail_test(
    "slang macros fail 3"
    FALSE
    test/test_macros_fail3
)
add_compile_fail_test(
    "slang macros fail 4"
    FALSE
    test/test_macros_fail4
)
add_compile_fail_test(
    "slang macros fail 5"
    FALSE
    test/test_macros_fail5
)
add_compile_exec_test(
    "slang test_macro_import"
    TRUE
    test/test_macro_import --search-path test
)
set_tests_properties(
    "slang test_macro_import (default, compile)" 
    PROPERTIES FIXTURES_REQUIRED 
    test_macros
)
add_compile_exec_test(
    "slang test_macro_transitive_import"
    TRUE
    test/test_macro_transitive_import --search-path test
)
set_tests_properties(
    "slang test_macro_transitive_import (default, compile)" 
    PROPERTIES FIXTURES_REQUIRED 
    test_macro_import
)
add_compile_exec_test(
    "slang math"
    TRUE
    test/test_math
)
add_compile_exec_test(
    "slang operators"
    TRUE
    test/test_operators
)
add_compile_exec_test(
    "slang strings"
    TRUE
    test/test_strings
)
add_compile_exec_test(
    "slang structs"
    TRUE
    test/test_structs
)

#
# Tests with other package dependencies
#

add_compile_test(
    "slang example_package/hello"
    TRUE
    test/example_package/hello
)
set_tests_properties(
    "slang example_package/hello (default, compile)" 
    PROPERTIES FIXTURES_SETUP 
    example_package_hello
)

add_compile_exec_test(
    "slang test_import"
    TRUE
    test/test_import --search-path test
)
set_tests_properties(
    "slang test_import (default, compile)" 
    PROPERTIES FIXTURES_REQUIRED 
    example_package_hello
)
set_tests_properties(
    "slang test_import (--no-eval-const-subexpr, compile)" 
    PROPERTIES FIXTURES_REQUIRED 
    example_package_hello
)
set_tests_properties(
    "slang test_import (default, exec)" 
    PROPERTIES FIXTURES_REQUIRED 
    example_package_hello
)
set_tests_properties(
    "slang test_import (--no-eval-const-subexpr, exec)" 
    PROPERTIES FIXTURES_REQUIRED 
    example_package_hello
)

#
# Examples.
#

add_compile_exec_test(
    "slang examples/hello_world"
    TRUE
    examples/hello_world
)
add_compile_exec_test(
    "slang examples/array_loop"
    TRUE
    examples/array_loop
)
add_compile_exec_test(
    "slang examples/structs"
    TRUE
    examples/structs
)
add_compile_exec_test(
    "slang examples/linked_list"
    TRUE
    examples/linked_list
)
add_compile_exec_test(
    "slang examples/print_args"
    TRUE
    examples/print_args
)

add_my_test(
    "slang examples/print_args -- Hello World"
    ${CMAKE_BINARY_DIR}/slang exec examples/print_args -- Hello World
)
set_tests_properties(
    "slang examples/print_args -- Hello World"
    PROPERTIES 
    FIXTURES_REQUIRED needs_std
)

add_compile_exec_test(
    "slang examples/string_conversion"
    TRUE
    examples/string_conversion
)

add_my_test(
    "slang examples/string_conversion -- 3.141 a -1"
    ${CMAKE_BINARY_DIR}/slang exec examples/string_conversion -- 3.141 a -1
)
set_tests_properties(
    "slang examples/string_conversion -- 3.141 a -1"
    PROPERTIES 
    FIXTURES_REQUIRED needs_std
)

add_compile_exec_fail_test(
    "slang examples/assert"
    TRUE
    examples/assert
)

#
# Native integration example/test.
#

add_compile_test(
    "slang native_integration"
    FALSE
    examples/native_integration
)
set_tests_properties(
    "slang native_integration (default, compile)" 
    PROPERTIES FIXTURES_SETUP 
    native_integration
)

add_test(NAME "native_integration" 
    COMMAND ${CMAKE_BINARY_DIR}/examples/native_integration
)
set_tests_properties("native_integration" 
    PROPERTIES 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)
set_tests_properties(
    "native_integration" 
    PROPERTIES FIXTURES_REQUIRED 
    native_integration
)
    