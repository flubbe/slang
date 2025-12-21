# The Scripting Language

`slang` is a compiled and interpreted language with a garbage collector.
It is meant to not be too complicated, and many choices made in the design
reflect this. For example, the syntax is chosen so that the parser can be
made simpler, the garbage collector is using a synchronous mark-and-sweep
algorithm, and the language is single-threaded.

## A `Hello, World!` program

```
// Import the standard library.
import std;

fn main(args: str[]) -> i32
{
    std::println("Hello, World!");
    return 0;
}
```

## The syntax

Single-line comments are introduced by `//`. Multi-line comments are of the form `/* comment */`.

Ignoring comments, a program consists of:
```
<import-statements>
<constants>
<types>
<functions>
```

1. Import statements are of the form `import path::to::module`. The path
   is resolved by the file manager by going through its search paths. It picks
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
    Any `{ ... }` block introduces a new scope.
    1. Statements:
        1. Empty statement `;`.
        2. Variable declarations: `let <name> : <type> [=<expression>];`. 
        
            To initialize a custom type, use
            ```
            let <name> : <type> = <type> <initializer-list>;
            ```
            where `<initializer-list>` is either listing the initial values in declaration order
            of the type,
            ```
            {<value1>, ..., <valueN>}
            ```
            or providing named initializers:
            ```
            {<member-name1> : <value1>, ..., <member-nameN> : <valueN>}
            ```
            In the latter case, <code>&lt;member-name<i>i</i>&gt; : &lt;value<i>i</i>&gt;</code> can appear in any order.
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
    2. Expressions:
        1. Literals:

            Integer, floating-point, and string literals.

            Integers and floating-point literals can have suffixes `i8, i16, i32, i64, f32, f64` to type them.
            A literal with no suffix has type `i32` for integers and `f64` for floating-point values.
        2. Variable and member access:

            A variable is referenced by its name.
            Members of structs or arrays are accessed using the `.` and `[]` operators:
            ```
            <struct-name>.<member-name>
            <array-name>[<index-expression>]
            ```
        3. Function calls:

            ```
            <function-name>(<arg-expr1>, ..., <arg-exprN>);
            ```
            The argument expressions are evaluated left to right before the call.
        4. Binary operators for `i32`, `i64`, `f32`, `f64`:

            Standard arithmetic (`+`, `-`, `*`, `/`, `%`),
            left/right shifts (`<<`, `>>`), comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`),
            logical (`&&`, `||`, `!`) and bitwise (`&`, `|`, `^`) operators are supported (shifts, logical and bitwise operators only for `i32`).

            Operator precedence follows conventional rules, with parentheses `(<expr>)`
            used for explicit grouping.

        5. Unary operators for `i32`, `i64`, `f32`, `f64`:

            Unary `+` (identity), `-` (negation) and `!` (logical not, only for `i32`) apply to a single operand.

        6. Assignments:
            ```
            <target> = <expression>;
            ```
            Assigns the result of the right-hand expression to the target variable or member.

    3. Type casts: 
    
        An expression type can be cast to another type
        when adhering to type cast rules. Type casts are indicated
        by `as` used after the to-be cast expression:
        ```
        <expression> as <type>
        ```
        Casting is allowed between `i8`, `i16`, `i32`, `i64`, `f32` and `f64`, and between any
        struct and types marked with `#[allow_cast]` (see below).

        The cast binds to the token on its left. For example, the parenthesis in `(-123) as i8` is mandatory.

        **Note:** Arithmetic works with `i32`, `i64`, `f32` and `f64` types, and others have to be explicitly
        casted.
5. Macros are exported by the module and can be accessed from another module.
    They are defined via
    ```
    macro <name!> {
        <macro-branch1>
        <macro-branch2>
        ...
        <macro-branchN>
    }
    ```
    where <code>&lt;macro-branch<i>i</i>&gt;</code> is of the form
    ```
    (<arg1> : <arg-type1>, ..., <argM> : <arg-typeM>) => {
        <statement-or-expression1>
        ...
        <statement-or-expressionM>
    };
    ```
    The argument type <code>&lt;arg-type<i>i</i>&gt;</code> is `expr` to match an expression, or `expr...` to match
    an expression list. The latter is only valid for the last argument type.
    
    The argument list can be empty.

    _Built-in macros:_
    
    Currently the only built-in macro is `std::format!`.

Statements and expressions can be decorated with _directives_:
```
#[directive(arg-name1=arg1,...,arg_nameN=argN)]
...
```

Currently, the following directives are used:
```
// Allow casting to and from this type. The type has to be empty.
#[allow_cast]
struct <name> {};

// Indicate that there is a struct definition in C++ code that
// is registered and checked in the interpreter.
#[native]
struct <name> {...}

// Indicate that this function has a native implementation in "<lib-name>".
#[native(lib="<lib-name>")]
fn <name>(<arg-name1> : <arg-type1>, ..., <arg-nameN> : <arg-typeN>) -> <return-type>;

fn test()
{
    // Disable compile-time constant evaluation for the next expression.
    #[disable(const_eval)]
    let t: i32 = 1*2 + 3;
    ...
}
```
Unknown directives are ignored.

## Built-in types

Built-in types are
- `void`: Indicates `<no-type>`.
- `i8`, `i16`, `i32`, `i64`: Integers with bit widths of 8, 16, 32, and 64.
- `f32`, `f64`: 32 and 64 bit wide floating-point numbers.
- `str`: A string.
- Arrays are declared as `let <name>: [<type>];`. They are of fixed length, and the length
    can be accessed with `<name>.length`. A new array can be defined as
    ```
    let <name>: [<type>] = new <type>[<size>];
    ```
    **Note:** Arrays are one-dimensional.

**Note:** The internal formats of numbers are not fixed right now. The implementation
uses whatever the underlying C++ implementation provides, but e.g. `f32` is only tested with
`IEEE754` format.

## Modules

A _module_ is a single (source or compiled) file. It is identified by a path of the form:
```
path::to::module
```
Modules are resolved by traversing the search path list.

A _package_ is a collection of modules in a directory.

## The standard library

After building the module, the standard library can be imported using
```
import std;
```
Currently, there are only a hand full of functions and struct's implemented.
These are:

### Types
- `#[allow_cast] struct type {}`: A generic type to cast to and from.
- `#[native] struct result`: A result type.

    _Members_:
    - `ok: i32`: Indicates whether the operation returning the type was successful.
    - `value: type`: If the operation was successful, this holds the result.

        **Note:** Needs to be cast to the expected type.
- `#[native] struct i32s`: Wrapper around an `i32`. 

    _Members_:
    - `value: i32`: An integer.
- `#[native] struct f32s`: Wrapper around an `f32`.

    _Members_:
    - `value: f32`: A 32-bit floating point value.

### Output
- `fn print(s: str) -> void`: Print a string to stdout.
- `fn println(s: str) -> void`: Print a string to stdout and append a new-line character.

### Arrays and strings
- `fn array_copy(from: type, to: type) -> void`: Copy an array. The array 
    types have to match and `from.length <= to.length`.
- `fn string_length(s: str) -> i32`: Get the length of a string.
- `fn string_concat(s1: str, s2: str) -> str`: Concatenate two strings.
- `fn string_equals(s1: str, s2: str) -> i32`: Compare two strings for equality. 
    Returns `1` if the strings compare equal, and `0` otherwise.
- `fn i32_to_string(i: i32) -> str`: Convert an `i32` value to a string.
- `fn i64_to_string(i: i64) -> str`: Convert an `i64` value to a string.
- `fn f32_to_string(f: f32) -> str`: Convert a `f32` value to a string.
- `fn f64_to_string(f: f64) -> str`: Convert a `f64` value to a string.
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
- [examples/native_integration.cpp](../examples/native_integration.cpp), 
- [examples/native_integration.sl](../examples/native_integration.sl)
- [examples/CMakeLists.txt](../examples/CMakeLists.txt).
