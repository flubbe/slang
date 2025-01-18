# The Scripting Language

`slang` is a compiled and interpreted language with a garbage collector.
It is meant to not be too complicated, and many choices made in the design
reflect this. For example, the syntax is chosen so that the parser can be
made simpler, the garbage collector is using a synchronous mark-and-sweep
algorithm, and the language is single-threaded.

## A `Hello, World!` programm

```
// Import the standard library.
import std;

fn main(args: [str]) -> i32
{
    std::println("Hello, World!");
    return 0;
}
```

## The syntax

A program consists of:
```
<import-statements>
<constants>
<types>
<functions>
```

1. Import statements are of the form `import path::to::module`. The path
   is resolved by the file manager by search its search paths. It picks
   the first module found in that list.

   The imports are accessible by prepending the import name, e.g. `std::print("Hello");`
   or `std::i32s{value: 12} as std::type`.
2. Constants are declared as
    ```
    const <name> : <type> = <constant>;
    ```
    They are exported by the module and can be accessed from another module.
3. Types are declared as
    ```
    struct <name> {
        <member-name1> : <member-type1>,
        <member-name2> : <member-type2>,
        ...
        <member-nameN> : <member-typeN>
    };
    ```
    They are exported by the module and can be accessed from another module.
4. Functions are exported by the module and can be accessed from another module.
    They are defined via
    ```
    fn <name>(<arg-name1> : <arg-type1>, ..., <arg-nameN> : <arg-typeN>) -> <return-type>
    {
        <statement-or-expression1>
        ...
        <statement-or-expressionM>
    }
    ```
    Statements are:
    1. Empty statement `;`.
    2. Variable declarations: `let <name> : <type> [=<expression>];`.
    3. `if` statements:
        ```
        if(<condition>)
        <if-block>
        [else <else-block>]
        ```
    4. `while` statement:
        ```
        while(<condition>)
        <while-block>
        ```
    5. `break` and `continue` statements:
        Break or continue a `while` loop.
    6. `return [<expression>]`: Return from a function.

    TODO expressions, type casts    

5. TODO Directives.

## Modules

## The standard library

After building the module, the standard library can be imported using
```
import std;
```
Currently, there are only a hand full of functions and struct's implemented.
These are:

### Output
- `fn print(s: str) -> void`: Print a string to stdout.
- `fn println(s: str) -> void`: Print a string to stdout and append a new-line character.

### Arrays and strings
- `fn array_copy(from: type, to: type) -> void`: Copy an array. The array 
    types have to match and `from.length <= to.length`.
- `fn string_concat(s1: str, s2: str) -> str`: Concatenate two strings.
- `fn string_equals(s1: str, s2: str) -> i32`: Compare two strings for equality. 
    Returns `1` if the strings compare equal, and `0` otherwise.
- `fn i32_to_string(i: i32) -> str`: Convert an `i32` value to a string.
- `fn f32_to_string(f: f32) -> str`: Convert a `f32` value to a string.
- `fn parse_i32(s: str) -> result`: Parse a string and return an `i32` integer
    wrapped in a `i32s` inside a `result`.
- `fn parse_f32(s: str) -> result`: Parse a string and return an `f32` float
    wrapped in a `f32s` inside a `result`.

### Debugging
- `fn assert(condition: i32, msg: str) -> void`: 
    Assert that `condition` does not evaluate to `0`. If `condition` evaluates
    to `0`, `msg` is emitted to stdout and the program exits with exit code `1`.

### Maths

A list of implemented constants and functions:
- `const PI: f32`
- `const SQRT2: f32`
- `fn abs(x: f32) -> f32`
- `fn sqrt(x: f32) -> f32`
- `fn ceil(x: f32) -> f32`
- `fn floor(x: f32) -> f32`
- `fn trunc(x: f32) -> f32`
- `fn round(x: f32) -> f32`
- `fn sin(x: f32) -> f32`
- `fn cos(x: f32) -> f32`
- `fn tan(x: f32) -> f32`
- `fn asin(x: f32) -> f32`
- `fn acos(x: f32) -> f32`
- `fn atan(x: f32) -> f32`
- `fn atan2(x: f32, y: f32) -> f32`

    **Note:** The behavior here for `+0` and `-0` is different from its underlying
    C++ implementation, which will be adjusted in a future iteration.

## Integration with native code

See the example consisting of
- [examples/native_integration.cpp](examples/native_integration.cpp), 
- [examples/native_integration.sl](examples/native_integration.sl)
- [examples/CMakeLists.txt](examples/CMakeLists.txt).

TODO 