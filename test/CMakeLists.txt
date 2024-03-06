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
    ../src/lexer.cpp
    ../src/parser.cpp
    ../src/codegen.cpp
    ../src/utils.cpp
)
target_link_libraries(test_parser fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_codegen
    test_codegen.cpp
    ../src/ast.cpp
    ../src/lexer.cpp
    ../src/parser.cpp
    ../src/codegen.cpp
    ../src/utils.cpp
)
target_link_libraries(test_codegen fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_compile_ir
    test_compile_ir.cpp
    ../src/ast.cpp
    ../src/lexer.cpp
    ../src/parser.cpp
    ../src/codegen.cpp
    ../src/utils.cpp
)
target_link_libraries(test_compile_ir fmt::fmt GTest::gtest GTest::gtest_main)

add_executable(test_type_system
    test_type_system.cpp
    ../src/ast.cpp
    ../src/lexer.cpp
    ../src/parser.cpp
    ../src/codegen.cpp
    ../src/utils.cpp
)
target_link_libraries(test_type_system fmt::fmt GTest::gtest GTest::gtest_main)
