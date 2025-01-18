from pathlib import Path
import subprocess

_module_path = Path(__file__).parent

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

    failures: int = 0

    # compile and run the examples
    _example_files: list[str] = [
        "hello_world",
        "array_loop",
        "structs",
        "linked_list",
        "print_args",
        "string_conversion",
    ]
    for ex in list(_example_files):
        if (
            subprocess.call(
                ["./build/Debug/slang", "compile", str(Path("examples") / ex)],
                cwd=_module_path.parent,
            )
            != 0
        ):
            print(f"Compilation failed for {ex}")
            _example_files.remove(ex)
            failures += 1

    for ex in _example_files:
        if (
            subprocess.call(
                ["./build/Debug/slang", "exec", str(Path("examples") / ex)],
                cwd=_module_path.parent,
            )
        ) != 0:
            print(f"Error during execution of {ex}")
            failures += 1

    # Assertion example.
    if (
        subprocess.call(
            ["./build/Debug/slang", "compile", "examples/assert"],
            cwd=_module_path.parent,
        )
        != 0
    ):
        print("Compilation failed for assert")
        failures += 1
    else:
        if (
            subprocess.call(
                ["./build/Debug/slang", "exec", "examples/assert"],
                cwd=_module_path.parent,
            )
            == 0
        ):
            print("Assertion not triggered.")
            failures += 1

    # Compile integration example.
    if (
        subprocess.call(
            [
                "./build/Debug/slang",
                "compile",
                str(Path("examples/native_integration")),
            ],
            cwd=_module_path.parent,
        )
        != 0
    ):
        print("Compilation failed for native_integration")
        failures += 1

    # Run integration example.
    if (
        subprocess.call(
            [
                "./build/Debug/examples/native_integration",
            ],
            cwd=_module_path.parent,
        )
        != 0
    ):
        print("native_integration example failed.")
        failures += 1

    print(f"---- {failures} failures in examples ----")
    exit(0 if failures == 0 else 1)
