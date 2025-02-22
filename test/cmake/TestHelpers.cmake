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
    add_test(NAME "${test_name} (compile)" COMMAND ${CMAKE_BINARY_DIR}/slang compile ${ARGN})
    set_tests_properties("${test_name} (compile)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_fail_test test_name uses_std)
    add_test(NAME "${test_name} (compile fail)" COMMAND ${CMAKE_BINARY_DIR}/slang compile ${ARGN})
    set_tests_properties("${test_name} (compile fail)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        WILL_FAIL TRUE
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (compile fail)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_exec_test test_name uses_std)
    add_my_test("${test_name} (compile)" ${CMAKE_BINARY_DIR}/slang compile ${ARGN})
    add_my_test("${test_name} (exec)" ${CMAKE_BINARY_DIR}/slang exec ${ARGN})

    if(${uses_std})
        set_tests_properties(
            "${test_name} (compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (exec)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()

function(add_compile_exec_fail_test test_name uses_std)
    add_my_test("${test_name} (compile)" ${CMAKE_BINARY_DIR}/slang compile ${ARGN})
    add_test(NAME "${test_name} (exec fail)" COMMAND ${CMAKE_BINARY_DIR}/slang exec ${ARGN})
    set_tests_properties("${test_name} (exec fail)" 
        PROPERTIES 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        WILL_FAIL TRUE
    )

    if(${uses_std})
        set_tests_properties(
            "${test_name} (compile)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
        set_tests_properties(
            "${test_name} (exec fail)"
            PROPERTIES 
            FIXTURES_REQUIRED needs_std
        )
    endif()
endfunction()
