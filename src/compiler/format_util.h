/**
 * slang - a simple scripting language.
 *
 * format! macro utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "shared/token.h"

namespace slang::typing
{
class context;
}    // namespace slang::typing

namespace slang::format_util
{

/** A format string placeholder with its resolved type. */
struct placeholder
{
    /** Starting offset into the format string. */
    std::size_t start{0};

    /** One past the ending offset into the format string. */
    std::size_t end{0};

    /** Specifier: 'd' (int), 'f' (float), 's' (str), or nullopt (auto). */
    std::optional<char> specifier{std::nullopt};

    /** Resolved argument type. */
    std::optional<int> arg_type{-1};
};

/**
 * Parse a format string for placeholders.
 *
 * Handles {{ and }} escaping (produces literal { or }).
 * Supports specifiers: d (int), f (float), s (str).
 *
 * @param fmt The format string (without surrounding quotes).
 * @param loc Source location for error reporting.
 * @returns The list of placeholders.
 * @throws std::runtime_error on invalid format string.
 */
std::vector<placeholder> parse_format_string(
  const std::string& fmt,
  source_location loc);

/**
 * Validate that an argument type satisfies a format specifier.
 *
 * @param arg_type The resolved argument type.
 * @param specifier The placeholder specifier (d/f/s/nullopt).
 * @param ctx The type context for type comparison.
 * @param loc Source location for error reporting.
 * @throws std::runtime_error if the specifier is unsupported or the type does
 *     not satisfy it.
 */
void validate_arg_type(
  typing::type_id arg_type,
  const std::optional<char>& specifier,
  const typing::context& ctx,
  source_location loc);

}    // namespace slang::format_util
