from typing import cast
from pathlib import Path
import subprocess
from io import BytesIO

_module_path = Path(__file__).parent

_test_dir = (
    _module_path / Path("..") / Path("build") / Path("Debug") / Path("test")
).absolute()
_tests: list[str] = [
    "test_vector",
    "test_package",
    "test_lexer",
    "test_parser",
    "test_codegen",
    "test_compile_ir",
    "test_type_system",
    "test_filemanager",
    "test_serialization",
    "test_output",
    "test_resolve",
    "test_interpreter",
]
_expect_failure: list[str] = []

_compile_only_tests: list[str] = ["test_const_assign_fail"]
_compile_only_expect_failure: list[str] = ["test_const_assign_fail"]

_script_tests: list[str] = [
    "test_array",
    "test_cast",
    "test_cast_fail",
    "test_const_export",
    "test_const_import",
    "test_conversions",
    "test_eval_subexpr",
    "test_math",
    "test_operators",
    "test_strings",
    "test_structs",
]
_script_expect_failure: list[str] = ["test_cast_fail"]


_lang_path = (_module_path / Path("..") / Path("lang")).absolute()
_lang_std_module_path = (_lang_path / Path("std.cmod")).absolute()

if __name__ == "__main__":
    # generate std.cmod if necessary
    if not _lang_std_module_path.exists():
        print("Compiling lang/std ...")
        if (
            subprocess.call(
                [
                    "./build/Debug/slang",
                    "compile",
                    "src/lang/std.sl",
                    "-o",
                    "lang/std.cmod",
                ],
                cwd=_module_path.parent,
            )
            != 0
        ):
            print("Compiling lang/std failed. Exiting.")
            exit(1)

    # Run C++ tests.
    retcodes: list[int] = []
    for t in _tests:
        result = subprocess.call(_test_dir / t, cwd=_module_path.parent)
        if t not in _expect_failure:
            retcodes.append(result)
        else:
            retcodes.append(result == 0)

    failed_tests = [(i, r) for i, r in enumerate(retcodes) if r != 0]
    if len(failed_tests) > 0:
        for i, r in failed_tests:
            if r != 0:
                print(f"Test {_tests[i]} failed")

    print(
        f"---------- {len(retcodes)} tests ran, {len(retcodes) - len(failed_tests)} passed, {len(failed_tests)} failed ----------"
    )

    # compile-only tests.
    compile_info: list[tuple[int, str, str]] = []
    for test_name in _compile_only_tests:
        print(f"[......] Compiling test/{test_name} ...", end="")
        p = subprocess.Popen(
            [
                "./build/Debug/slang",
                "compile",
                f"test/{test_name}",
            ],
            cwd=_module_path.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        p.wait(timeout=5)
        compile_info.append(
            (
                p.returncode,
                cast(BytesIO, p.stdout).read().decode(),
                cast(BytesIO, p.stderr).read().decode(),
            )
        )

        result = (
            p.returncode
            if test_name not in _compile_only_expect_failure
            else int(p.returncode == 0)
        )
        if result == 0:
            print(f"\r[  OK  ]")
        else:
            print(f"\r[FAILED]")

    failed_compilation = [
        (idx, info)
        for idx, info in enumerate(compile_info)
        if info[0] != 0 and _compile_only_tests[idx] not in _compile_only_expect_failure
    ]
    if len(failed_compilation) > 0:
        for idx, info in failed_compilation:
            test_path = Path(f"test/{_script_tests[idx]}")
            print()
            print(f"Compilation of '{test_path.absolute()}' failed.")
            if len(info[1]) != 0:
                print(f"Captured stdout:\n{info[1]}")
            if len(info[2]) != 0:
                print(f"Captured stderr:\n{info[2]}")

    # compile test scripts.
    compile_info = []
    for test_name in _script_tests:
        print(f"[......] Compiling test/{test_name} ...", end="")
        p = subprocess.Popen(
            [
                "./build/Debug/slang",
                "compile",
                f"test/{test_name}",
            ],
            cwd=_module_path.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        p.wait(timeout=5)
        compile_info.append(
            (
                p.returncode,
                cast(BytesIO, p.stdout).read().decode(),
                cast(BytesIO, p.stderr).read().decode(),
            )
        )
        if p.returncode == 0:
            print(f"\r[  OK  ]")
        else:
            print(f"\r[FAILED]")

    failed_compilation = [
        (idx, info) for idx, info in enumerate(compile_info) if info[0] != 0
    ]
    if len(failed_compilation) > 0:
        for idx, info in failed_compilation:
            test_path = Path(f"test/{_script_tests[idx]}")
            print()
            print(f"Compilation of '{test_path.absolute()}' failed.")
            if len(info[1]) != 0:
                print(f"Captured stdout:\n{info[1]}")
            if len(info[2]) != 0:
                print(f"Captured stderr:\n{info[2]}")

        exit(1)

    # Run script tests.
    script_info: list[tuple[int, str, str]] = []
    for test_name in _script_tests:
        test_path = Path(f"test/{test_name}")
        print(f"[......] Running {test_path.absolute()}", end="")
        p = subprocess.Popen(
            [
                "./build/Debug/slang",
                "exec",
                f"test/{test_name}",
            ],
            cwd=_module_path.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        p.wait(timeout=5)

        result = (
            p.returncode
            if test_name not in _script_expect_failure
            else int(p.returncode == 0)
        )
        script_info.append(
            (
                result,
                cast(BytesIO, p.stdout).read().decode(),
                cast(BytesIO, p.stderr).read().decode(),
            )
        )
        if result == 0:
            print(f"\r[  OK  ]")
        else:
            print(f"\r[FAILED]")

    script_failed_tests = [
        (idx, info) for idx, info in enumerate(script_info) if info[0] != 0
    ]
    if len(script_failed_tests) > 0:
        for idx, info in script_failed_tests:
            test_path = Path(f"test/{_script_tests[idx]}")
            print()
            print(f"Test '{test_path.absolute()}' failed.")
            if len(info[1]) != 0:
                print(f"Captured stdout:\n{info[1]}")
            if len(info[2]) != 0:
                print(f"Captured stderr:\n{info[2]}")

    print(
        f"---------- {len(script_info)} tests ran, {len(script_info) - len(script_failed_tests)} passed, {len(script_failed_tests)} failed ----------"
    )

    exit(len(failed_tests) + len(script_failed_tests) != 0)
