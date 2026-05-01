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
    let value: i32 = very_long_identifier_name + another_long_identifier_name +
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

# 4) glob: **/* defaults to .sl files recursively.
set(GLOB_DIR "${TMP_DIR}/glob")
file(MAKE_DIRECTORY "${GLOB_DIR}/sub")
file(WRITE "${GLOB_DIR}/a.sl" "fn main()->void{let x:i32=1;}")
file(WRITE "${GLOB_DIR}/sub/b.sl" "fn main()->void{let y:i32=2;}")
file(WRITE "${GLOB_DIR}/sub/c.txt" "fn main()->void{let z:i32=3;}")

execute_process(
    COMMAND "${SLANG_BIN}" fmt "${GLOB_DIR}/**/*"
    RESULT_VARIABLE GLOB_FMT_CODE
)
if(NOT GLOB_FMT_CODE EQUAL 0)
    message(FATAL_ERROR "'slang fmt ${GLOB_DIR}/**/*' failed with exit code ${GLOB_FMT_CODE}.")
endif()

file(READ "${GLOB_DIR}/a.sl" A_SL)
file(READ "${GLOB_DIR}/sub/b.sl" B_SL)
file(READ "${GLOB_DIR}/sub/c.txt" C_TXT)

set(EXPECTED_A [[fn main() -> void {
    let x: i32 = 1;
}
]])
set(EXPECTED_B [[fn main() -> void {
    let y: i32 = 2;
}
]])
if(NOT A_SL STREQUAL EXPECTED_A)
    message(FATAL_ERROR "Glob **/* did not format a.sl as expected.")
endif()
if(NOT B_SL STREQUAL EXPECTED_B)
    message(FATAL_ERROR "Glob **/* did not format sub/b.sl as expected.")
endif()
if(NOT C_TXT STREQUAL "fn main()->void{let z:i32=3;}")
    message(FATAL_ERROR "Glob **/* unexpectedly formatted non-.sl file.")
endif()

# 5) glob: **/*.ext filters extension.
set(EXT_DIR "${TMP_DIR}/glob_ext")
file(MAKE_DIRECTORY "${EXT_DIR}/sub")
file(WRITE "${EXT_DIR}/x.foo" "fn main()->void{let x:i32=4;}")
file(WRITE "${EXT_DIR}/sub/y.foo" "fn main()->void{let y:i32=5;}")
file(WRITE "${EXT_DIR}/sub/z.sl" "fn main()->void{let z:i32=6;}")

execute_process(
    COMMAND "${SLANG_BIN}" fmt "${EXT_DIR}/**/*.foo"
    RESULT_VARIABLE GLOB_EXT_FMT_CODE
)
if(NOT GLOB_EXT_FMT_CODE EQUAL 0)
    message(FATAL_ERROR "'slang fmt ${EXT_DIR}/**/*.foo' failed with exit code ${GLOB_EXT_FMT_CODE}.")
endif()

file(READ "${EXT_DIR}/x.foo" X_FOO)
file(READ "${EXT_DIR}/sub/y.foo" Y_FOO)
file(READ "${EXT_DIR}/sub/z.sl" Z_SL)

set(EXPECTED_X [[fn main() -> void {
    let x: i32 = 4;
}
]])
set(EXPECTED_Y [[fn main() -> void {
    let y: i32 = 5;
}
]])
if(NOT X_FOO STREQUAL EXPECTED_X)
    message(FATAL_ERROR "Glob **/*.foo did not format x.foo as expected.")
endif()
if(NOT Y_FOO STREQUAL EXPECTED_Y)
    message(FATAL_ERROR "Glob **/*.foo did not format sub/y.foo as expected.")
endif()
if(NOT Z_SL STREQUAL "fn main()->void{let z:i32=6;}")
    message(FATAL_ERROR "Glob **/*.foo unexpectedly formatted .sl file.")
endif()

# 6) glob: /** also behaves as recursive .sl discovery and ignores non-.sl files.
set(STARSTAR_DIR "${TMP_DIR}/glob_starstar")
file(MAKE_DIRECTORY "${STARSTAR_DIR}/sub")
file(WRITE "${STARSTAR_DIR}/main.sl" "fn main()->void{let x:i32=7;}")
file(WRITE "${STARSTAR_DIR}/sub/next.sl" "fn main()->void{let y:i32=8;}")
file(WRITE "${STARSTAR_DIR}/sub/skip.cmod" "binary-ish")
file(WRITE "${STARSTAR_DIR}/sub/skip.cpp" "int main() {}")

execute_process(
    COMMAND "${SLANG_BIN}" fmt "${STARSTAR_DIR}/**"
    RESULT_VARIABLE STARSTAR_CODE
)
if(NOT STARSTAR_CODE EQUAL 0)
    message(FATAL_ERROR "'slang fmt ${STARSTAR_DIR}/**' failed with exit code ${STARSTAR_CODE}.")
endif()

file(READ "${STARSTAR_DIR}/main.sl" STAR_MAIN)
file(READ "${STARSTAR_DIR}/sub/next.sl" STAR_NEXT)
file(READ "${STARSTAR_DIR}/sub/skip.cmod" STAR_SKIP_CMOD)
file(READ "${STARSTAR_DIR}/sub/skip.cpp" STAR_SKIP_CPP)

set(EXPECTED_STAR_MAIN [[fn main() -> void {
    let x: i32 = 7;
}
]])
set(EXPECTED_STAR_NEXT [[fn main() -> void {
    let y: i32 = 8;
}
]])
if(NOT STAR_MAIN STREQUAL EXPECTED_STAR_MAIN)
    message(FATAL_ERROR "Glob /** did not format main.sl as expected.")
endif()
if(NOT STAR_NEXT STREQUAL EXPECTED_STAR_NEXT)
    message(FATAL_ERROR "Glob /** did not format sub/next.sl as expected.")
endif()
if(NOT STAR_SKIP_CMOD STREQUAL "binary-ish")
    message(FATAL_ERROR "Glob /** unexpectedly touched .cmod file.")
endif()
if(NOT STAR_SKIP_CPP STREQUAL "int main() {}")
    message(FATAL_ERROR "Glob /** unexpectedly touched .cpp file.")
endif()
