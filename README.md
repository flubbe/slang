# Scripting Language

This repository is for learning how to build a scripting language, that is, a compiler
and an interpreter. It is far from complete and I am working on aspects of it every once
in a while.

Currently the only way to run anything is by writing a [test](test), as the command line interface
(in [src/main.cpp](src/main.cpp) and [src/commandline.cpp](src/commandline.cpp)) is incomplete.

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
    - [`boost`](https://www.boost.org/) (>=1.83.0, <2.0)
    - [`fmt`](https://github.com/fmtlib/fmt) (>=10.0.0, <11.0)
    - [GoogleTest](https://github.com/google/googletest) (>=1.14.0, <2.0)

    Install/download these, and set up your compiler to point to the corresponding `include` directories.
    Then use `CMake` to build the project.

### The scripting language

To see how the scripting language is intended to work, you can have a look at:
- [test/test_compile_ir.cpp](test/test_compile_ir.cpp)
- [test/test_output.cpp](test/test_output.cpp)
