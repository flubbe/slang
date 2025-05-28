#
# Helper function for test setup.
#

function(add_my_test test_name)
    add_test(NAME "${test_name}" COMMAND ${ARGN})
    set_tests_properties("${test_name}" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
endfunction()

function(add_compile_test test_name uses_std)
    add_test(
        NAME "${test_name} (default, compile)" 
        COMMAND ${CMAKE_BINARY_DIR}/slang compile ${ARGN}
    )
    set_tests_properties("${test_name} (default, compile)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    add_test(
        NAME "${test_name} (--no-eval-const-subexpr, compile)" 
        COMMAND ${CMAKE_BINARY_DIR}/slang compile --no-eval-const-subexpr ${ARGN}
    )
    set_tests_properties("${test_name} (--no-eval-const-subexpr, compile)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (default, compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (--no-eval-const-subexpr, compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_fail_test test_name uses_std)
    add_test(
        NAME "${test_name} (default, compile fail)" 
        COMMAND ${CMAKE_BINARY_DIR}/slang compile ${ARGN}
    )
    set_tests_properties("${test_name} (default, compile fail)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        WILL_FAIL TRUE
    )

    add_test(
        NAME "${test_name} (--no-eval-const-subexpr, compile fail)" 
        COMMAND ${CMAKE_BINARY_DIR}/slang compile --no-eval-const-subexpr ${ARGN}
    )
    set_tests_properties("${test_name} (--no-eval-const-subexpr, compile fail)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        WILL_FAIL TRUE
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (default, compile fail)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (--no-eval-const-subexpr, compile fail)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_exec_test test_name uses_std)
    add_my_test("${test_name} (default, compile)" ${CMAKE_BINARY_DIR}/slang compile ${ARGN})
    set_tests_properties(
        "${test_name} (default, compile)"
        PROPERTIES
        FIXTURES_SETUP "${test_name} (default, compile)"
    )
    add_my_test("${test_name} (default, run)" ${CMAKE_BINARY_DIR}/slang run ${ARGN})
    set_tests_properties(
        "${test_name} (default, run)"
        PROPERTIES
        FIXTURES_REQUIRED "${test_name} (default, compile)"
    )

    add_my_test("${test_name} (--no-eval-const-subexpr, compile)" ${CMAKE_BINARY_DIR}/slang compile --no-eval-const-subexpr ${ARGN})
    set_tests_properties(
        "${test_name} (--no-eval-const-subexpr, compile)"
        PROPERTIES
        FIXTURES_SETUP "${test_name} (--no-eval-const-subexpr, compile)"
    )
    add_my_test("${test_name} (--no-eval-const-subexpr, run)" ${CMAKE_BINARY_DIR}/slang run ${ARGN})
    set_tests_properties(
        "${test_name} (--no-eval-const-subexpr, run)"
        PROPERTIES
        FIXTURES_REQUIRED "${test_name} (--no-eval-const-subexpr, compile)"
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (default, compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (default, run)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (--no-eval-const-subexpr, compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (--no-eval-const-subexpr, run)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_exec_fail_test test_name uses_std)
    add_my_test("${test_name} (default, compile)" ${CMAKE_BINARY_DIR}/slang compile ${ARGN})
    add_test(NAME "${test_name} (default, run fail)" COMMAND ${CMAKE_BINARY_DIR}/slang run ${ARGN})
    set_tests_properties("${test_name} (default, run fail)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        WILL_FAIL TRUE
    )

    add_my_test("${test_name} (--no-eval-const-subexpr, compile)" ${CMAKE_BINARY_DIR}/slang compile --no-eval-const-subexpr ${ARGN})
    add_test(NAME "${test_name} (--no-eval-const-subexpr, run fail)" COMMAND ${CMAKE_BINARY_DIR}/slang run ${ARGN})
    set_tests_properties("${test_name} (--no-eval-const-subexpr, run fail)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        WILL_FAIL TRUE
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (default, compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (default, run fail)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (--no-eval-const-subexpr, compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (--no-eval-const-subexpr, run fail)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()
