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

_lang_path = (_module_path / Path("..") / Path("lang")).absolute()
_lang_std_module_path = (_lang_path / Path("std.cmod")).absolute()

if __name__ == "__main__":
    # generate std.cmod if necessary
    if not _lang_std_module_path.exists():
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

    retcodes: list[int] = []
    for t in _tests:
        retcodes.append(subprocess.call(_test_dir / t, cwd=_module_path.parent))

    for i, r in enumerate(retcodes):
        if r != 0:
            print(f"Test {_tests[i]} failed")
