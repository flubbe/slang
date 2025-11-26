# Scripting Language

This repository is for learning how to build a scripting language, that is, a compiler
and an interpreter. It is far from complete and I am working on aspects of it every once
in a while.

The command line interface (in [src/main.cpp](src/main.cpp) and [src/commandline](src/commandline)) 
provides these commands:
- `slang compile`: Compiles a source file into bytecode.
- `slang run`: Run the main function from a compiled file.
- `slang disasm`: Disassemble a module and print the bytecode.

For example:
```bash
$ slang compile src/lang/std.sl -o lang/std.cmod
$ slang compile examples/hello_world.sl
$ slang run examples/hello_world
```
The last command should result in
```
Hello, World!

Program exited with exit code 0.
```
To get an impression of the generated modules and bytecode, you can use
```bash
$ slang disasm lang/std
$ slang disasm examples/hello_world
```
You can also have a look at the [instruction set](docs/instructions.md).

To see how the language works, have a look at the [examples](examples) and also at 
[test/test_compile_ir.cpp](test/test_compile_ir.cpp) and [test/test_output.cpp](test/test_output.cpp).

For an example showing how the interpreter can be integrated into C++, have a look at
[examples/native_integration.cpp](examples/native_integration.cpp), [examples/native_integration.sl](examples/native_integration.sl)
and the necessary setup in [examples/CMakeLists.txt](examples/CMakeLists.txt).

A preliminary (incomplete) documentation of the scripting language can be found [here](docs/language.md).

## Getting started

### Building the project

1. Using _Conan_ and _CMake_ (debug build):
    ```
    $ conan install . --build=missing -s compiler.cppstd=23 -s build_type=Debug
    $ cmake --preset conan-debug
    $ cmake --build build --preset conan-debug
    ```
2. Manually:

    The project depends on
    - [`cxxopts`](https://github.com/jarro2783/cxxopts) (==3.2.0)
    - [`GSL`](https://github.com/microsoft/GSL) (==4.1.0)
    - [GoogleTest](https://github.com/google/googletest) (>=1.14.0, <2.0)

    Install/download these, and set up your compiler to point to the corresponding `include` directories.
    Then use `CMake` to build the project.

### Running the tests

Run the tests using `ctest`. Assuming a debug build, run:
```
$ ctest --test-dir build/Debug
```

### The scripting language

A simple _Hello, World_ application looks like
```
import std;

fn main(args: [str]) -> i32
{
    std::println("Hello, World!");
    return 0;
}
```

The language has data type support for `i8`, `i16`, `i32`, `i64`, `f32`, `f64`, `str`, and 
custom `struct`'s. Arrays are also supported, though (currently) they have to be one-dimensional.

See [the documentation](docs/language.md) for more details.
