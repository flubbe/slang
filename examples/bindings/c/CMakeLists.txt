set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(native_integration_c
    native_integration.c
)

target_include_directories(native_integration_c
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/bindings/c/include
)

target_link_libraries(native_integration_c
    PRIVATE
    slang_c
)
