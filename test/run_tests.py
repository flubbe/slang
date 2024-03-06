from pathlib import Path
import subprocess

_module_path = Path(__file__).parent

_test_dir = (
    _module_path / Path("..") / Path("build") / Path("Debug") / Path("test")
).absolute()
_tests = [
    "test_package",
    "test_lexer",
    "test_parser",
    "test_codegen",
    "test_compile_ir",
    "test_type_system",
]

if __name__ == "__main__":
    retcodes: list[int] = []
    for t in _tests:
        retcodes.append(subprocess.call(_test_dir / t))

    for i, r in enumerate(retcodes):
        if r != 0:
            print(f"Test {_tests[i]} failed")
