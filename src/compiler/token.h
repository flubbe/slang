/**
 * slang - a simple scripting language.
 *
 * token helpers.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

#include "archives/archive.h"
#include "location.h"

namespace slang
{

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
    end_of_input,     /** parser-owned end-of-input marker */

    last = end_of_input /** Last element. */
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

/** Numeric suffix type. */
enum class suffix_type
{
    integer,        /** integers literals */
    floating_point, /** floating-point literals */
};

/**
 * Convert a suffix type into a readable string.
 *
 * @param ty The suffix type.
 * @returns Returns a readable string.
 */
std::string to_string(suffix_type ty);

/** A numeric suffix. */
struct numeric_suffix
{
    /** Suffix type. */
    suffix_type ty;

    /** Bit width. */
    std::uint32_t width;
};

/** An evaluated constant. */
using const_value = std::variant<std::int64_t, double, std::string>;

/** Comment trivia attached to a token. */
struct comment_trivia
{
    /** The raw comment text including delimiters. */
    std::string text;

    /** Whether this is a block comment. */
    bool is_block{false};

    /** Whether there was at least one blank line before this comment. */
    bool has_blank_line_before{false};
};

/** `comment_trivia` serializer. */
archive& operator&(archive& ar, comment_trivia& trivia);

/** Syntax context id for hygiene tracking. */
using syntax_context_id = std::uint32_t;

/** Root syntax context (source code tokens outside macros). */
inline constexpr syntax_context_id root_syntax_context{0};

/** An evaluated token. */
struct token
{
    /** The token. */
    std::string s;

    /** Token location. */
    source_location location;

    /** Token type. */
    token_type type;

    /**
     * Literal suffix, for `token_type::int_literal`,
     * `token_type::fp_literal` and `token_type::str_literal.
     */
    std::optional<numeric_suffix> suffix;

    /**
     * Evaluated token, for `token_type::int_literal`,
     * `token_type::fp_literal` and `token_type::str_literal.`
     */
    std::optional<const_value> value;

    /** Whether there was at least one blank line before this token. */
    bool has_blank_line_before{false};

    /** Leading comments attached to this token. */
    std::vector<comment_trivia> leading_comments;

    /** Trailing comments attached to this token. */
    std::vector<comment_trivia> trailing_comments;

    /** Syntax context for hygiene tracking. */
    syntax_context_id context{root_syntax_context};

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
     * @param type The token's type. Defaults to `token_type::unknown`.
     * @param suffix Optional suffix for numeric tokens.
     * @param value The token's value. Defaults to `std::nullopt`.
     * @param leading_comments Leading comments. Defaults to empty.
     * @param trailing_comments Trailing comments. Defaults to empty.
     * @param context Syntax context for hygiene tracking. Defaults to `root_syntax_context`.
     */
    token(
      std::string s,
      source_location location,
      token_type type = token_type::unknown,
      std::optional<numeric_suffix> suffix = std::nullopt,
      std::optional<const_value> value = std::nullopt,
      bool has_blank_line_before = false,
      std::vector<comment_trivia> leading_comments = {},
      std::vector<comment_trivia> trailing_comments = {},
      syntax_context_id context = root_syntax_context)
    : s{std::move(s)}
    , location{std::move(location)}
    , type{type}
    , suffix{suffix}
    , value{std::move(value)}
    , has_blank_line_before{has_blank_line_before}
    , leading_comments{std::move(leading_comments)}
    , trailing_comments{std::move(trailing_comments)}
    , context{context}
    {
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
