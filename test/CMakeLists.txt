project(tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt REQUIRED)
find_package(GTest REQUIRED CONFIG)

include_directories(
    ../src
)

add_executable(test_package
    test_package.cpp
    ../src/package.cpp
    ../src/utils.cpp
)
target_link_libraries(test_package fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_lexer
    test_lexer.cpp
    ../src/lexer.cpp
)
target_link_libraries(test_lexer fmt::fmt GTest::gtest GTest::gtest_main)

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

add_executable(test_serialization
    test_serialization.cpp
    ../src/archives/file.cpp
)
target_link_libraries(test_serialization fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_filemanager
    test_filemanager.cpp
    ../src/archives/file.cpp
    ../src/filemanager.cpp
)
target_link_libraries(test_filemanager fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_interpreter
    test_interpreter.cpp
    ../src/archives/file.cpp
    ../src/filemanager.cpp
    ../src/gc.cpp
    ../src/interpreter.cpp
    ../src/opcodes.cpp
)
target_link_libraries(test_interpreter fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_vector
    test_vector.cpp
)
target_link_libraries(test_vector fmt::fmt GTest::gtest GTest::gtest_main)
