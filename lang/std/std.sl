/**
 * Standard Library.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

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
fn array_copy(from: [], to: []) -> void;

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

