/**
 * slang - a simple scripting language.
 *
 * format! macro utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "format_util.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "compiler/typing.h"

namespace slang::format_util
{

namespace
{

/** Extract the string value from a string literal token. */
std::string extract_string_literal(const token& tok)
{
    if(tok.type != token_type::str_literal)
        throw std::runtime_error("Expected string literal.");
    const auto& s = tok.s;
    if(s.length() >= 2)
        return s.substr(1, s.length() - 2);
    return {};
}

}    // namespace

/*
 * parse_format_string.
 */

std::vector<placeholder> parse_format_string(
    const std::string& fmt,
    [[maybe_unused]] source_location loc)
{
    std::vector<placeholder> placeholders;
    std::optional<placeholder> current{std::nullopt};
    std::string current_specifier;

    for(std::size_t i = 0; i < fmt.length(); ++i)
    {
        if(fmt[i] == '{')
        {
            if(current.has_value())
                throw std::runtime_error(
                    "Invalid format string: nested placeholder.");

            if(i + 1 < fmt.length() && fmt[i + 1] == '{')
            {
                placeholders.emplace_back(placeholder{
                    .start = i, .end = i + 1, .specifier = '{'});
                ++i;
                continue;
            }

            current = placeholder{
                .start = i, .end = 0,
                .specifier = std::nullopt, .arg_type = std::nullopt};
        }
        else if(fmt[i] == '}')
        {
            if(current.has_value())
            {
                current->end = i + 1;

                if(current_specifier.length() > 1
                   || (current_specifier.length() == 1
                       && current_specifier[0] != 'd'
                       && current_specifier[0] != 'f'
                       && current_specifier[0] != 's'))
                {
                    throw std::runtime_error(
                        std::string("Unsupported format specifier: '")
                        + current_specifier + "'.");
                }

                current->specifier = current_specifier.empty()
                                         ? std::nullopt
                                         : std::make_optional(
                                               current_specifier[0]);
                current_specifier.clear();

                placeholders.push_back(*current);
                current = std::nullopt;
            }
            else
            {
                if(i + 1 < fmt.length() && fmt[i + 1] == '}')
                {
                    placeholders.emplace_back(placeholder{
                        .start = i, .end = i + 1, .specifier = '}'});
                    ++i;
                    continue;
                }

                throw std::runtime_error(
                    "Invalid format string: unmatched '}'.");
            }
        }
        else
        {
            if(current.has_value())
            {
                // Skip ':' delimiter before the specifier.
                if(fmt[i] == ':')
                    continue;
                current_specifier += fmt[i];
            }
        }
    }

    if(current.has_value())
        throw std::runtime_error(
            "Invalid format string: unmatched '{'.");

    return placeholders;
}

/*
 * validate_arg_type.
 */

void validate_arg_type(
    int arg_type,
    const std::optional<char>& specifier,
    const typing::context& ctx,
    [[maybe_unused]] source_location loc)
{
    if(!specifier.has_value())
    {
        if(arg_type == static_cast<int>(ctx.get_i32_type())
           || arg_type == static_cast<int>(ctx.get_i64_type())
           || arg_type == static_cast<int>(ctx.get_f32_type())
           || arg_type == static_cast<int>(ctx.get_f64_type())
           || arg_type == static_cast<int>(ctx.get_str_type()))
            return;

        throw std::runtime_error(
            "format!: argument is not convertible to a string.");
    }

    switch(specifier.value())
    {
        case 'd':
            if(arg_type != static_cast<int>(ctx.get_i32_type())
               && arg_type != static_cast<int>(ctx.get_i64_type()))
                throw std::runtime_error(
                    "format!: argument has wrong type for 'd' specifier.");
            break;
        case 'f':
            if(arg_type != static_cast<int>(ctx.get_f32_type())
               && arg_type != static_cast<int>(ctx.get_f64_type()))
                throw std::runtime_error(
                    "format!: argument has wrong type for 'f' specifier.");
            break;
        case 's':
            if(arg_type != static_cast<int>(ctx.get_str_type()))
                throw std::runtime_error(
                    "format!: argument has wrong type for 's' specifier.");
            break;
        default:
            throw std::runtime_error(
                std::string("format!: unknown specifier '")
                + specifier.value() + "'.");
    }
}

}    // namespace slang::format_util
