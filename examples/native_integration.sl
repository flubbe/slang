/*
 * Declare functions that are implemented natively.
 */

/**
 * Print a string to stdout.
 *
 * @param s The string to print.
 */
#[native(lib = "slang")]
fn print(s: str) -> void;

/*
 * Declare struct which also has a native implementation.
 */
#[native]
struct S {
    a: i8, b: i8, c: i16, s: str, i: i32
};

/**
 * Test function, called from native code.
 */
fn test(s: S, f: f32) -> S {
    print("Received string: ");
    print(s.s);
    print("\n");

    return S {
        79i8, s.b + 1i8, s.c - 2i16, "Hello from script!", f as i32
    };
}
