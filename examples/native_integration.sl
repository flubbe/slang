/*
 * Declare functions that are implemented natively.
 */

/**
 * Print a string to stdout.
 *
 * @param s The string to print.
 */
#[native(lib="slang")]
fn print(s: str) -> void;

/*
 * Declare struct which also has a native implementation.
 */

#[native]
struct S
{
    s: str,
    i: i32
};

/**
 * Test function, called from native code.
 */
fn test(s: S, f: f32) -> S
{
    print("Received string: ");
    print(s.s);
    print("\n");

    return S{"Hello from script!", f as i32};
}
