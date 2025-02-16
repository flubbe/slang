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

add_executable(native_integration
    native_integration.cpp
    ../src/archives/file.cpp
    ../src/runtime/arrays.cpp
    ../src/runtime/debug.cpp
    ../src/runtime/math.cpp
    ../src/runtime/strings.cpp
    ../src/runtime/types.cpp
    ../src/runtime/utils.cpp
    ../src/filemanager.cpp
    ../src/interpreter/gc.cpp
    ../src/interpreter/interpreter.cpp
    ../src/interpreter/module_loader.cpp
    ../src/shared/module_resolver.cpp
    ../src/shared/module.cpp
    ../src/shared/opcodes.cpp
)
target_link_libraries(native_integration fmt::fmt)
target_compile_options(native_integration PRIVATE
    ${COMPILE_OPTIONS_MSVC}
    ${COMPILE_OPTIONS_NOT_MSVC}
)
