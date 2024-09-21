# Scripting Language

This repository is for learning how to build a scripting language, that is, a compiler
and an interpreter. It is far from complete and I am working on aspects of it every once
in a while.

The command line interface (in [src/main.cpp](src/main.cpp) and [src/commandline.cpp](src/commandline.cpp)) 
is incomplete, but you can try:
```bash
$ slang compile lang/std/std.sl
$ slang compile examples/hello_world.sl
$ slang exec examples/hello_world
```
The last command should result in
```
Hello, World!

Program exited with exit code 0.
```
Also, have a look at the [test](test) folder for more examples.

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
    - [`fmt`](https://github.com/fmtlib/fmt) (>=10.0.0, <11.0)
    - [GoogleTest](https://github.com/google/googletest) (>=1.14.0, <2.0)

    Install/download these, and set up your compiler to point to the corresponding `include` directories.
    Then use `CMake` to build the project.

### The scripting language

A simple _Hello, World_ application looks like
```
import std;

fn main(s: str) -> i32
{
    std::println("Hello, World!");
    return 0;
}
```

The language has data type support for `i32`, `f32`, `str`, and custom `struct`'s. Arrays are also supported,
though (currently) they have to be one-dimensional.

To see how the scripting language is intended to work and for further examples, you can have a look at:
- [test/test_compile_ir.cpp](test/test_compile_ir.cpp)
- [test/test_output.cpp](test/test_output.cpp)

