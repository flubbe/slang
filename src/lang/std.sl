/**
 * Print a string to stdout.
 *
 * @param s The string to print.
 */
#[native(lib="slang")]
fn print(s: str) -> void;

/**
 * Print a string to stdout and append a new-line character.
 *
 * @param s The string to print.
 */
#[native(lib="slang")]
fn println(s: str) -> void;
