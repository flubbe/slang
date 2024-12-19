/**
 * Standard Library.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

/*
 * Types.
 */

/** Generic type. */
#[allow_cast]
struct type {};

/** 
 * Result type, holding a generic value and an indicator whether the result holds an error. 
 */
struct result 
{
    ok: i32,
    value: type
};

/** Wrapper around i32. */
struct i32s
{
    value: i32
};

/** Wrapper around f32. */
struct f32s
{
    value: f32
};

/*
 * Output.
 */

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

/*
 * Arrays.
 */

/**
 * Copy an array. The array types have to match and `from.length <= to.length`.
 *
 * @param from The array to copy from.
 * @param to The array to copy into.
 */
#[native(lib="slang")]
fn array_copy(from: type, to: type) -> void;

/*
 * Strings.
 */

/**
 * Concatenate two strings.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @return Returns the concatentation s1 + s2.
 */
#[native(lib="slang")]
fn string_concat(s1: str, s2: str) -> str;

/**
 * Compare two strings for equality.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @return Returns 1 if the strings compare equal, and 0 otherwise.
 */
#[native(lib="slang")]
fn string_equals(s1: str, s2: str) -> i32;

/**
 * Convert an i32 integer to a string.
 *
 * @param i An i32 integer.
 * @return Returns the string representation of i.
 */
#[native(lib="slang")]
fn i32_to_string(i: i32) -> str;

/**
 * Convert an f32 float to a string.
 *
 * @param f An f32 float.
 * @return Returns the string representation of f.
 */
#[native(lib="slang")]
fn f32_to_string(f: f32) -> str;

/**
 * Parse a string and return an i32 integer. 
 *
 * @param s The string to parse.
 * @return Returns a `result` containing a `i32_s`.
 */
#[native(lib="slang")]
fn parse_i32(s: str) -> result;

/**
 * Parse a string and return an f32 float.
 *
 * @param s The string to parse.
 * @return Returns a `result` containing a `f32_s`.
 */
#[native(lib="slang")]
fn parse_f32(s: str) -> result;
  
/*
 * Debug.
 */

/**
 * Assert that a condition does not evaluate to 0.
 * 
 * @param condition The condition.
 * @param msg A message that is emitted when the condition is 0.
 */
#[native(lib="slang")]
fn assert(condition: i32, msg: str) -> void;
