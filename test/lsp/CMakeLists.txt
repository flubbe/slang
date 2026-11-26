# LSP test targets live in a subdirectory under test/ but are part
# of the parent "tests" CMake project (no `project()` call here).
#
# The parent's project root is `test/`, so ${CMAKE_SOURCE_DIR} points
# to the project root.  Build artifacts live under build/Debug/test/.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED CONFIG)

#
# Helper for creating LSP GTest executables.
#
function(add_lsp_gtest_executable target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/../../src
        ../../src
    )
    target_link_libraries(${target_name}
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        slang_warning_flags
        simdjson::simdjson
        PUBLIC
        slang_test_frontend_core
    )
    add_test(NAME "${target_name}" COMMAND ${target_name})
endfunction()

# -- Phase 1: Transport ---------------------------------------------------
add_lsp_gtest_executable(test_lsp_transport
    ../../src/lsp/lsp_transport.cpp
    ../../src/lsp/json_rpc.cpp
    test_lsp_transport.cpp
)

# -- Phase 2: JSON-RPC ----------------------------------------------------
add_lsp_gtest_executable(test_lsp_json_rpc
    ../../src/lsp/json_rpc.cpp
    test_json_rpc.cpp
)

# -- Phase 4: Document Store ---------------------------------------------
add_lsp_gtest_executable(test_lsp_document_store
    ../../src/lsp/json_rpc.cpp
    ../../src/lsp/document_store.cpp
    test_document_store.cpp
)

# -- Phase 5: Diagnostics -------------------------------------------------
add_lsp_gtest_executable(test_lsp_diagnostics
    ../../src/lsp/lsp_transport.cpp
    ../../src/lsp/json_rpc.cpp
    ../../src/lsp/diagnostics.cpp
    ../../src/lsp/document_store.cpp
    test_diagnostics.cpp
)

# -- Phase 6.1: Navigation ------------------------------------------------
add_lsp_gtest_executable(test_lsp_navigation
    ../../src/lsp/lsp_transport.cpp
    ../../src/lsp/json_rpc.cpp
    ../../src/lsp/diagnostics.cpp
    ../../src/lsp/navigation.cpp
    test_navigation.cpp
)

# -- Phase 6.2: Editor assistance: hover ----------------------------------
add_lsp_gtest_executable(test_lsp_hover
    ../../src/lsp/lsp_transport.cpp
    ../../src/lsp/json_rpc.cpp
    ../../src/lsp/diagnostics.cpp
    ../../src/lsp/navigation.cpp
    ../../src/lsp/hover.cpp
    test_hover.cpp
)

# -- Phase 6.2: Editor assistance: completion --------------------------------
add_lsp_gtest_executable(test_lsp_completion
    ../../src/lsp/lsp_transport.cpp
    ../../src/lsp/json_rpc.cpp
    ../../src/lsp/diagnostics.cpp
    ../../src/lsp/navigation.cpp
    ../../src/lsp/completion.cpp
    test_completion.cpp
)

# -- Phase 6.2: Editor assistance: signature help -------------------------
add_lsp_gtest_executable(test_lsp_signature_help
    ../../src/lsp/lsp_transport.cpp
    ../../src/lsp/json_rpc.cpp
    ../../src/lsp/diagnostics.cpp
    ../../src/lsp/navigation.cpp
    ../../src/lsp/signature_help.cpp
    test_signature_help.cpp
)
