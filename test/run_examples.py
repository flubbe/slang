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

    for ex in _example_files:
        subprocess.call(
            ["./build/Debug/slang", "exec", str(Path("examples") / ex)],
            cwd=_module_path.parent,
        )
