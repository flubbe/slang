set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(slang_c SHARED
    src/slang.cpp
)

target_include_directories(slang_c
    PUBLIC
    include
)

target_link_libraries(slang_c
    PRIVATE
    slang_interpreter
)
