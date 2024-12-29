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
#[native]
struct result 
{
    ok: i32,
    value: type
};

/** Wrapper around i32. */
#[native]
struct i32s
{
    value: i32
};

/** Wrapper around f32. */
#[native]
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
 * @return Returns a `result` containing a `i32s`.
 */
#[native(lib="slang")]
fn parse_i32(s: str) -> result;

/**
 * Parse a string and return an f32 float.
 *
 * @param s The string to parse.
 * @return Returns a `result` containing a `f32s`.
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

/*
 * Math.
 */

/** pi with 32 bit accuracy. */
const PI: f32 = 3.1415927;

/** sqrt(2) with 32 bit accuracy. */
const SQRT2: f32 = 1.4142135;

/**
 * Compute the absolute value.
 *
 * @param x A floating point value.
 * @return Returns `abs(x)`.
 */
#[native(lib="slang")]
fn abs(x: f32) -> f32;

/**
 * Compute the square root.
 *
 * @param x A floating point value.
 * @return Returns `sqrt(x)`.
 */
#[native(lib="slang")]
fn sqrt(x: f32) -> f32;

/**
 * Computes the least integer value not less than `x`.
 *
 * @param x A floating point value.
 * @return Returns `ceil(x)`.
 */
#[native(lib="slang")]
fn ceil(x: f32) -> f32;

/**
 * Computes the largest integer value not greater than `x`.
 *
 * @param x A floating point value.
 * @return Returns `ceil(x)`.
 */
#[native(lib="slang")]
fn floor(x: f32) -> f32;

/**
 * Computes the nearest integer not greater in magnitude than `x`.
 *
 * @param x A floating point value.
 * @return Returns `trunc(x)`.
 */
#[native(lib="slang")]
fn trunc(x: f32) -> f32;

/**
 * Computes the nearest integer value to `x` (in floating-point format),
 * rounding halfway cases away from zero, regardless of the current rounding mode
 *
 * @param x A floating point value.
 * @return Returns `round(x)`.
 */
#[native(lib="slang")]
fn round(x: f32) -> f32;

/**
 * Compute the sine.
 *
 * @param x Argument to the sine.
 * @return Returns `sin(x)`.
 */
#[native(lib="slang")]
fn sin(x: f32) -> f32;

/**
 * Compute the cosine.
 *
 * @param x Argument to the cosine.
 * @return Returns `cos(x)`.
 */
#[native(lib="slang")]
fn cos(x: f32) -> f32;

/**
 * Compute the tangent.
 *
 * @param x Argument to the tangent.
 * @return Returns `tan(x)`.
 */
#[native(lib="slang")]
fn tan(x: f32) -> f32;

/**
 * Compute the arc sine.
 *
 * @param x Argument to the arc sine.
 * @return Returns `asin(x)`.
 */
#[native(lib="slang")]
fn asin(x: f32) -> f32;

/**
 * Compute the arc cosine.
 *
 * @param x Argument to the arc cosine.
 * @return Returns `acos(x)`.
 */
#[native(lib="slang")]
fn acos(x: f32) -> f32;

/**
 * Compute the principal value of the arc tangent.
 *
 * @param x Argument to the arc tangent.
 * @return Returns `atan(x)`.
 */
#[native(lib="slang")]
fn atan(x: f32) -> f32;

/**
 * Computes the arc tangent of `y/x` using the signs of arguments to determine the correct quadrant.
 *
 * @param x `f32` value.
 * @param y `f32` value.
 * @return Returns `atan(x)`.
 */
#[native(lib="slang")]
fn atan2(x: f32, y: f32) -> f32;
