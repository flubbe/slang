from pathlib import Path
import subprocess

_module_path = Path(__file__).parent

_test_dir = (
    _module_path / Path("..") / Path("build") / Path("Debug") / Path("test")
).absolute()
_tests = [
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

_script_tests = ["test_array", "test_conversions", "test_structs"]

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

    # compile test scripts.
    for test_name in _script_tests:
        print(f"Compiling test/{test_name} ...")
        if (
            subprocess.call(
                [
                    "./build/Debug/slang",
                    "compile",
                    f"test/{test_name}",
                ],
                cwd=_module_path.parent,
            )
            != 0
        ):
            print(f"Compiling test '{test_name}' failed")
            exit(1)

    # Run C++ tests.
    retcodes: list[int] = []
    for t in _tests:
        retcodes.append(subprocess.call(_test_dir / t, cwd=_module_path.parent))

    failed_tests = [(i, r) for i, r in enumerate(retcodes) if r != 0]
    if len(failed_tests) > 0:
        for i, r in failed_tests:
            if r != 0:
                print(f"Test {_tests[i]} failed")

    print(
        f"---------- {len(retcodes)} tests ran, {len(retcodes) - len(failed_tests)} passed, {len(failed_tests)} failed ----------"
    )

    # Run script tests.
    script_retcodes: list[int] = []
    for test_name in _script_tests:
        script_retcodes.append(
            subprocess.call(
                [
                    "./build/Debug/slang",
                    "exec",
                    f"test/{test_name}",
                ],
                cwd=_module_path.parent,
            )
        )

    script_failed_tests = [(i, r) for i, r in enumerate(script_retcodes) if r != 0]
    if len(script_failed_tests) > 0:
        for i, r in script_failed_tests:
            if r != 0:
                print(f"Test {_script_tests[i]} failed")

    print(
        f"---------- {len(script_retcodes)} tests ran, {len(script_retcodes) - len(script_failed_tests)} passed, {len(script_failed_tests)} failed ----------"
    )

    exit(len(failed_tests) + len(script_failed_tests) != 0)
