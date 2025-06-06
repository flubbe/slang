/**
 * slang - a simple scripting language.
 *
 * token helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <optional>
#include <variant>

#include "archives/archive.h"

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
 * `token_location` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param loc The token location to serialize.
 */
archive& operator&(archive& ar, token_location& loc);

/**
 * Convert a token location to a string.
 *
 * @param loc The token location.
 * @return A string of the form "(line, col)".
 */
std::string to_string(const token_location& loc);

/** Token type. */
enum class token_type
{
    unknown,          /** Unknown token type. */
    delimiter,        /** A delimiter, e.g. + - * / % ! & | ^ . :: < > ( ) { } [ ] ; */
    identifier,       /** starts with A-Z, a-z or _ and continues with A-Z, a-z, _, 0-9 */
    macro_identifier, /** Same as `identifier`, but starting with $ */
    macro_name,       /** Same as `identifier`, but ending with ! */
    int_literal,      /** integer literal */
    fp_literal,       /** floating-point literal */
    str_literal,      /** a quoted string (including the quotes) */

    last = str_literal /** Last element. */
};

/**
 * Convert a `token_type` to a string.
 *
 * @param t The token type.
 * @return Returns s astring representation of the token type.
 */
std::string to_string(token_type ty);

/**
 * `token_type` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param ty The token type to serialize.
 */
archive& operator&(archive& ar, token_type& ty);

/** An evaluated token. */
struct token
{
    /** The token. */
    std::string s;

    /** Token location. */
    token_location location;

    /** Token type. */
    token_type type;

    /**
     * Evaluated token, for `token_type::int_literal`,
     * `token_type::fp_literal` and `token_type::string_literal.`
     */
    std::optional<std::variant<int, float, std::string>> value;

    /** Default constructors. */
    token() = default;
    token(const token&) = default;
    token(token&&) = default;

    /** Default assignments. */
    token& operator=(const token&) = default;
    token& operator=(token&&) = default;

    /**
     * Initializing constructor for a token.
     *
     * @param s The token's string.
     * @param location The token's location.
     * @param type The token's type. Defaults to token_type::unknown.
     * @param value The token's value. Defaults to std::nullopt.
     */
    token(
      std::string s,
      token_location location,
      token_type type = token_type::unknown,
      std::optional<std::variant<int, float, std::string>> value = std::nullopt)
    : s{std::move(s)}
    , location{std::move(location)}
    , type{type}
    , value{std::move(value)}
    {
    }

    /** Comparison operator. */
    bool operator<(const token& other) const
    {
        if(location.line < other.location.line)
        {
            return true;
        }
        else if(location.line == other.location.line)
        {
            if(location.col < other.location.col)
            {
                return true;
            }
            else if(location.col == other.location.col)
            {
                return s < other.s;
            }
        }

        return false;
    }
};

/**
 * `token` serializer.
 *
 * @param ar The archive to use for serialization.
 * @param cls The type class to serialize.
 * @returns The input archive.
 */
archive& operator&(archive& ar, token& tok);

}    // namespace slang
