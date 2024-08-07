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

add_executable(test_lexer
    test_lexer.cpp
    ../src/lexer.cpp
)
target_link_libraries(test_lexer fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_lexer PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_parser
    test_parser.cpp
    ../src/ast.cpp
    ../src/codegen.cpp
    ../src/lexer.cpp
    ../src/module.cpp
    ../src/parser.cpp
    ../src/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_parser fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_parser PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_codegen
    test_codegen.cpp
    ../src/ast.cpp
    ../src/codegen.cpp
    ../src/lexer.cpp
    ../src/module.cpp
    ../src/parser.cpp
    ../src/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_codegen fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_codegen PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_compile_ir
    test_compile_ir.cpp
    ../src/ast.cpp
    ../src/codegen.cpp
    ../src/lexer.cpp
    ../src/module.cpp
    ../src/parser.cpp
    ../src/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_compile_ir fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_compile_ir PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_type_system
    test_type_system.cpp
    ../src/archives/file.cpp
    ../src/ast.cpp
    ../src/codegen.cpp
    ../src/filemanager.cpp
    ../src/lexer.cpp
    ../src/module.cpp
    ../src/parser.cpp
    ../src/resolve.cpp
    ../src/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_type_system fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_type_system PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_resolve
    test_resolve.cpp
    ../src/archives/file.cpp
    ../src/ast.cpp
    ../src/codegen.cpp
    ../src/filemanager.cpp
    ../src/lexer.cpp
    ../src/module.cpp
    ../src/parser.cpp
    ../src/resolve.cpp
    ../src/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_resolve fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_resolve PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_output
    test_output.cpp
    ../src/archives/file.cpp
    ../src/ast.cpp
    ../src/codegen.cpp
    ../src/emitter.cpp
    ../src/filemanager.cpp
    ../src/lexer.cpp
    ../src/module.cpp
    ../src/opcodes.cpp
    ../src/parser.cpp
    ../src/resolve.cpp
    ../src/typing.cpp
    ../src/utils.cpp
)
target_link_libraries(test_output fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_output PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
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

add_executable(test_interpreter
    test_interpreter.cpp
    ../src/archives/file.cpp
    ../src/runtime/arrays.cpp
    ../src/runtime/strings.cpp
    ../src/runtime/utils.cpp
    ../src/filemanager.cpp
    ../src/gc.cpp
    ../src/interpreter.cpp
    ../src/opcodes.cpp
)
target_link_libraries(test_interpreter fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_interpreter PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)

add_executable(test_vector
    test_vector.cpp
)
target_link_libraries(test_vector fmt::fmt GTest::gtest GTest::gtest_main)
target_compile_options(test_vector PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
