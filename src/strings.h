/**
 * slang - a simple scripting language.
 *
 * string helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

namespace slang
{

/** Returns if the given character is an alphabetic character. */
inline bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/** Returns if the given character is one of the 10 decimal digits: `0123456789`. */
inline bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/** Returns if the given character is an alphanumeric character. */
inline bool is_alnum(char c)
{
    return is_alpha(c) || is_digit(c);
}

}    // namespace slang