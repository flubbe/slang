# Scripting Language

This repository is for learning how to build a scripting language, that is, a compiler
and an interpreter. It is far from complete and I am working on aspects of it every once
in a while.

The command line interface (in [src/main.cpp](src/main.cpp) and [src/commandline.cpp](src/commandline.cpp)) 
is incomplete, but you can try:
```bash
$ slang compile src/lang/std.sl -o lang/std.cmod
$ slang compile examples/hello_world.sl
$ slang exec examples/hello_world
```
The last command should result in
```
Hello, World!

Program exited with exit code 0.
```
To get an impression of the generated modules and bytecode, you can use
```bash
$ slang exec lang/std --disasm
$ slang exec examples/hello_world --disasm
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

1. Using _Conan_ and _CMake_:
    ```
    $ conan install . --build=missing
    $ cmake --preset conan-debug
    $ cmake --build build --preset conan-debug
    ```
2. Manually:

    The project depends on
    - [`cxxopts`](https://github.com/jarro2783/cxxopts) (==3.2.0)
    - [`fmt`](https://github.com/fmtlib/fmt) (>=10.0.0, <11.0)
    - [GoogleTest](https://github.com/google/googletest) (>=1.14.0, <2.0)

    Install/download these, and set up your compiler to point to the corresponding `include` directories.
    Then use `CMake` to build the project.

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

The language has data type support for `i32`, `f32`, `str`, and custom `struct`'s. Arrays are also supported,
though (currently) they have to be one-dimensional.

See [the documentation](docs/language.md) for more details.
