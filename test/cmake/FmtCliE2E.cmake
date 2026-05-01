cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SLANG_BIN)
    message(FATAL_ERROR "SLANG_BIN not set")
endif()
if(NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "TEST_ROOT not set")
endif()

set(TMP_DIR "${TEST_ROOT}/build/fmt_cli_e2e")
file(MAKE_DIRECTORY "${TMP_DIR}")

set(INPUT_FILE "${TMP_DIR}/sample.sl")
set(INPUT_TEXT "fn main()->void{let value:i32=very_long_identifier_name+another_long_identifier_name+third_long_identifier_name;}")

# 1) --check should fail on unformatted input.
file(WRITE "${INPUT_FILE}" "${INPUT_TEXT}")
execute_process(
    COMMAND "${SLANG_BIN}" fmt --check --line-length 60 "${INPUT_FILE}"
    RESULT_VARIABLE CHECK_FAIL_CODE
)
if(CHECK_FAIL_CODE EQUAL 0)
    message(FATAL_ERROR "Expected 'slang fmt --check' to fail for unformatted input.")
endif()

# 2) fmt should rewrite file.
execute_process(
    COMMAND "${SLANG_BIN}" fmt --line-length 60 "${INPUT_FILE}"
    RESULT_VARIABLE FMT_CODE
)
if(NOT FMT_CODE EQUAL 0)
    message(FATAL_ERROR "'slang fmt' failed with exit code ${FMT_CODE}.")
endif()

file(READ "${INPUT_FILE}" FORMATTED_TEXT)
set(EXPECTED_TEXT [[fn main() -> void {
    let value : i32 = very_long_identifier_name + another_long_identifier_name +
    third_long_identifier_name;
}
]])
if(NOT FORMATTED_TEXT STREQUAL EXPECTED_TEXT)
    message(FATAL_ERROR "Formatted output mismatch.\nExpected:\n${EXPECTED_TEXT}\nGot:\n${FORMATTED_TEXT}")
endif()

# 3) --check should pass after formatting.
execute_process(
    COMMAND "${SLANG_BIN}" fmt --check --line-length 60 "${INPUT_FILE}"
    RESULT_VARIABLE CHECK_PASS_CODE
)
if(NOT CHECK_PASS_CODE EQUAL 0)
    message(FATAL_ERROR "Expected 'slang fmt --check' to pass after formatting.")
endif()
