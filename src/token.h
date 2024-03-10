/**
 * slang - a simple scripting language.
 *
 * token helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <optional>
#include <variant>

#include "fmt/core.h"

namespace slang
{

/**
 * Token location in the source.
 */
struct token_location
{
    std::size_t line{1}; /** Line. */
    std::size_t col{1};  /** Column. */

    /** Default constructors. */
    token_location() = default;
    token_location(const token_location&) = default;
    token_location(token_location&&) = default;

    /** Assignment. */
    token_location& operator=(const token_location&) = default;
    token_location& operator=(token_location&&) = default;

    /**
     * Initialize line and col.
     *
     * @param l The line.
     * @param c The column.
     */
    token_location(std::size_t l, std::size_t c)
    : line{l}
    , col{c}
    {
    }

    /**
     * Comparison.
     *
     * @param other The token_location to compare with.
     */
    bool operator==(const token_location& other) const
    {
        return line == other.line && col == other.col;
    }
};

/**
 * Convert a token location to a string.
 *
 * @param loc The token location.
 * @return A string of the form "(line, col)".
 */
inline std::string to_string(const token_location& loc)
{
    return fmt::format("{}:{}", loc.line, loc.col);
}

/**
 * Token type.
 */
enum class token_type
{
    unknown,     /** Unknown token type. */
    delimiter,   /** A delimiter, e.g. + - * / % ! & | ^ . :: < > ( ) { } [ ] ; */
    identifier,  /** starts with A-Z, a-z or _ and continues with A-Z, a-z, _, 0-9 */
    int_literal, /** integer literal */
    fp_literal,  /** floating-point literal */
    str_literal  /** a quoted string (including the quotes) */
};

/**
 * An evaluated token.
 */
struct token
{
    /** The token. */
    std::string s;

    /** Token location. */
    token_location location;

    /** Token type. */
    token_type type;

    /** Evaluated token, for token_type::int_literal, token_type::fp_literal and token_type::string_literal. */
    std::optional<std::variant<int, float, std::string>> value;
};

}    // namespace slang
