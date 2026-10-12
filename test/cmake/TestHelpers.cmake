# Helper for generic executable/command tests
function(add_my_test test_name)
    add_test(NAME "${test_name}" COMMAND ${ARGN})
    set_tests_properties("${test_name}"
        PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
endfunction()

# Internal helper to handle conditional fixture binding
function(_slang_apply_std_fixture test_name uses_std)
    if(uses_std)
        set_tests_properties("${test_name}"
            PROPERTIES
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_test test_name uses_std)
    set(target_default "${test_name} (default, compile)")
    set(target_no_eval "${test_name} (--no-eval-const-subexpr, compile)")

    add_my_test("${target_default}" slang compile ${ARGN})
    add_my_test("${target_no_eval}" slang compile --no-eval-const-subexpr ${ARGN})

    _slang_apply_std_fixture("${target_default}" "${uses_std}")
    _slang_apply_std_fixture("${target_no_eval}" "${uses_std}")
endfunction()

function(add_compile_fail_test test_name uses_std)
    set(target_default "${test_name} (default, compile fail)")
    set(target_no_eval "${test_name} (--no-eval-const-subexpr, compile fail)")

    add_my_test("${target_default}" slang compile ${ARGN})
    add_my_test("${target_no_eval}" slang compile --no-eval-const-subexpr ${ARGN})

    set_tests_properties("${target_default}" "${target_no_eval}"
        PROPERTIES WILL_FAIL TRUE
    )

    _slang_apply_std_fixture("${target_default}" "${uses_std}")
    _slang_apply_std_fixture("${target_no_eval}" "${uses_std}")
endfunction()

function(add_compile_exec_test test_name uses_std)
    set(default_compile "${test_name} (default, compile)")
    set(default_run "${test_name} (default, run)")
    set(noeval_compile "${test_name} (--no-eval-const-subexpr, compile)")
    set(noeval_run "${test_name} (--no-eval-const-subexpr, run)")

    add_my_test("${default_compile}" slang compile ${ARGN})
    add_my_test("${default_run}" slang run ${ARGN})
    set_tests_properties("${default_compile}" PROPERTIES FIXTURES_SETUP "${default_compile}")
    set_tests_properties("${default_run}" PROPERTIES FIXTURES_REQUIRED "${default_compile}")

    add_my_test("${noeval_compile}" slang compile --no-eval-const-subexpr ${ARGN})
    add_my_test("${noeval_run}" slang run ${ARGN})
    set_tests_properties("${noeval_compile}" PROPERTIES FIXTURES_SETUP "${noeval_compile}")
    set_tests_properties("${noeval_run}" PROPERTIES FIXTURES_REQUIRED "${noeval_compile}")

    if(uses_std)
        set_tests_properties(
            "${default_compile}" "${default_run}"
            "${noeval_compile}" "${noeval_run}"
            PROPERTIES FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_exec_fail_test test_name uses_std)
    set(default_compile "${test_name} (default, compile)")
    set(default_run_fail "${test_name} (default, run fail)")
    set(noeval_compile "${test_name} (--no-eval-const-subexpr, compile)")
    set(noeval_run_fail "${test_name} (--no-eval-const-subexpr, run fail)")

    add_my_test("${default_compile}" slang compile ${ARGN})
    add_my_test("${default_run_fail}" slang run ${ARGN})
    set_tests_properties("${default_run_fail}" PROPERTIES WILL_FAIL TRUE)

    add_my_test("${noeval_compile}" slang compile --no-eval-const-subexpr ${ARGN})
    add_my_test("${noeval_run_fail}" slang run ${ARGN})
    set_tests_properties("${noeval_run_fail}" PROPERTIES WILL_FAIL TRUE)

    if(uses_std)
        set_tests_properties(
            "${default_compile}" "${default_run_fail}"
            "${noeval_compile}" "${noeval_run_fail}"
            PROPERTIES FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

# JIT Helpers with centralized target platform check
macro(slang_is_jit_supported OUT_VAR)
    if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(${OUT_VAR} TRUE)
    else()
        set(${OUT_VAR} FALSE)
    endif()
endmacro()

function(add_my_jit_test test_name)
    slang_is_jit_supported(JIT_SUPPORTED)
    if(NOT JIT_SUPPORTED)
        return()
    endif()

    add_my_test("${test_name}" ${ARGN})
endfunction()

function(add_jit_compile_test test_name uses_std)
    slang_is_jit_supported(JIT_SUPPORTED)
    if(NOT JIT_SUPPORTED)
        return()
    endif()

    set(jit_test_name "${test_name} (JIT)")
    add_my_test("${jit_test_name}" slang run --jit ${ARGN})

    set_tests_properties("${jit_test_name}"
        PROPERTIES FIXTURES_REQUIRED "${test_name} (default, compile)"
    )

    if(uses_std)
        set_tests_properties("${jit_test_name}"
            PROPERTIES FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

# Helper function to safely set required fixtures on JIT tests only if JIT is enabled on this platform
function(set_jit_test_properties test_name)
    slang_is_jit_supported(JIT_SUPPORTED)
    if(NOT JIT_SUPPORTED)
        return()
    endif()

    set_tests_properties("${test_name} (JIT)" ${ARGN})
endfunction()
