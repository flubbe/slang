/**
 * slang - a simple scripting language.
 *
 * format! macro utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "compiler/typing.h"
#include "format_util.h"

namespace slang::format_util
{

/*
 * parse_format_string.
 */

std::vector<placeholder> parse_format_string(
  std::string_view fmt,
  source_location loc)
{
    std::vector<placeholder> placeholders;
    std::optional<placeholder> current{std::nullopt};
    std::string current_specifier;
    bool has_specifier_delimiter = false;

    for(std::size_t i = 0; i < fmt.length(); ++i)
    {
        if(fmt[i] == '{')
        {
            if(current.has_value())
            {
                throw format_error{
                  loc.advance(fmt, i),
                  "Invalid format string: nested placeholder."};
            }

            if(i + 1 < fmt.length() && fmt[i + 1] == '{')
            {
                placeholders.emplace_back(placeholder{
                  .start = i, .end = i + 1, .specifier = '{'});
                ++i;
                continue;
            }

            current = placeholder{
              .start = i, .end = 0, .specifier = std::nullopt, .arg_type = std::nullopt};
        }
        else if(fmt[i] == '}')
        {
            if(current.has_value())
            {
                if(has_specifier_delimiter
                   && current_specifier.empty())
                {
                    throw format_error{
                      loc.advance(fmt, i),
                      "Invalid format string: empty format specifier."};
                }

                current->end = i + 1;

                if(current_specifier.length() > 1
                   || (current_specifier.length() == 1
                       && current_specifier[0] != 'd'
                       && current_specifier[0] != 'f'
                       && current_specifier[0] != 's'))
                {
                    throw format_error{
                      loc.advance(fmt, i),
                      std::format(
                        "Unsupported format specifier: '{}'.",
                        current_specifier)};
                }

                current->specifier = current_specifier.empty()
                                       ? std::nullopt
                                       : std::make_optional(
                                           current_specifier[0]);
                current_specifier.clear();
                has_specifier_delimiter = false;

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

                throw format_error{
                  loc.advance(fmt, i),
                  "Invalid format string: unmatched '}'."};
            }
        }
        else
        {
            if(current.has_value())
            {
                // Skip ':' delimiter before the specifier.
                if(fmt[i] == ':')
                {
                    if(has_specifier_delimiter)
                    {
                        throw format_error{
                          loc.advance(fmt, i),
                          "Invalid format string: repeated specifier delimiter."};
                    }

                    has_specifier_delimiter = true;
                    continue;
                }

                current_specifier += fmt[i];
            }
        }
    }

    if(current.has_value())
    {
        throw format_error{
          loc.advance(fmt, current->start),
          "Invalid format string: unmatched '{'."};
    }

    return placeholders;
}

/*
 * validate_arg_type.
 */

void validate_arg_type(
  typing::type_id arg_type,
  const std::optional<char>& specifier,
  const typing::context& ctx,
  source_location loc)
{
    if(!specifier.has_value())
    {
        if(arg_type == ctx.get_i8_type()
           || arg_type == ctx.get_i16_type()
           || arg_type == ctx.get_i32_type()
           || arg_type == ctx.get_i64_type()
           || arg_type == ctx.get_f32_type()
           || arg_type == ctx.get_f64_type()
           || arg_type == ctx.get_str_type())
            return;

        throw format_error{
          loc,
          "format!: argument is not convertible to a string."}
          .with_help(
            "without a specifier, the argument must be an integer "
            "(i8/i16/i32/i64), a float (f32/f64), or a 'str'");
    }

    switch(specifier.value())
    {
    case 'd':
        if(arg_type != ctx.get_i8_type()
           && arg_type != ctx.get_i16_type()
           && arg_type != ctx.get_i32_type()
           && arg_type != ctx.get_i64_type())
        {
            throw format_error{
              loc,
              "format!: argument has wrong type for 'd' specifier."}
              .with_label("type mismatch")
              .with_note(
                std::format(
                  "the 'd' specifier expects an integer (i8/i16/i32/i64); got '{}'",
                  ctx.to_string(arg_type)));
        }
        break;
    case 'f':
        if(arg_type != ctx.get_f32_type()
           && arg_type != ctx.get_f64_type())
        {
            throw format_error{
              loc,
              "format!: argument has wrong type for 'f' specifier."}
              .with_label("type mismatch")
              .with_note(
                std::format(
                  "the 'f' specifier expects a float (f32/f64); got '{}'",
                  ctx.to_string(arg_type)));
        }
        break;
    case 's':
        if(arg_type != ctx.get_str_type())
        {
            throw format_error{
              loc,
              "format!: argument has wrong type for 's' specifier."}
              .with_label("type mismatch")
              .with_note(
                std::format(
                  "the 's' specifier expects a 'str'; got '{}'",
                  ctx.to_string(arg_type)));
        }
        break;
    default:
        throw format_error{
          loc,
          std::format(
            "format!: unknown specifier '{}'.",
            specifier.value())}
          .with_label("invalid format specifier")
          .with_help(
            "valid specifiers are 'd' (integer), 'f' (float), and 's' (string)");
    }
}

}    // namespace slang::format_util
