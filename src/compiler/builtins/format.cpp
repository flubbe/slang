/**
 * slang - a simple scripting language.
 *
 * built-in 'format!' macro.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "macros.h"

namespace slang::codegen::macros
{

/** Format string placeholder. */
struct format_string_placeholder
{
    /** Starting offset into the string. */
    std::size_t start{0};

    /** One past the ending offset into the string. */
    std::size_t end{0};

    /** Type: `{`, `}`, `d` (i32), `f` (f32) or `s` (str). Required for now. */
    std::uint8_t type{0};
};

/**
 * Return format specifiers in a string.
 *
 * @param loc Location of the format string.
 * @param s The format string.
 * @returns Returns a (possibly empty) vector of format placeholders.
 */
static std::vector<format_string_placeholder> get_format_string_placeholders(
  token_location loc,
  const std::string& s)
{
    std::vector<format_string_placeholder> placeholders;
    std::optional<format_string_placeholder> current_placeholder{std::nullopt};
    std::string format_string;

    for(std::size_t i = 0; i < s.length(); ++i)
    {
        if(s[i] == '{')
        {
            if(current_placeholder.has_value())
            {
                throw codegen_error(
                  loc,
                  fmt::format(
                    "Invalid format string '{}'.",
                    s));
            }

            if(i + 1 < s.length() && s[i + 1] == '{')
            {
                placeholders.emplace_back(
                  format_string_placeholder{i, i + 1, '{'});
                i += 1;
                continue;
            }

            current_placeholder = format_string_placeholder{i, 0, 0};
        }
        else if(s[i] == '}')
        {
            if(current_placeholder.has_value())
            {
                current_placeholder->end = i + 1;

                if(format_string.length() != 1
                   || (format_string[0] != 'd'
                       && format_string[0] != 'f'
                       && format_string[0] != 's'))
                {
                    throw codegen_error(
                      loc,
                      fmt::format(
                        "Unsupported format string '{}'.",
                        format_string));
                }
                current_placeholder->type = format_string[0];
                format_string.clear();

                placeholders.emplace_back(*current_placeholder);
                current_placeholder = std::nullopt;
            }
            else
            {
                if(i + 1 < s.length() && s[i + 1] == '}')
                {
                    placeholders.emplace_back(
                      format_string_placeholder{i, i + 1, '}'});
                    i += 1;
                    continue;
                }

                throw codegen_error(
                  loc,
                  fmt::format(
                    "Invalid format string '{}'.",
                    s));
            }
        }
        else
        {
            if(current_placeholder.has_value())
            {
                format_string += s[i];
            }
        }
    }

    if(current_placeholder.has_value())
    {
        throw codegen_error(
          loc,
          fmt::format(
            "Invalid format string '{}'.",
            s));
    }

    return placeholders;
}

std::unique_ptr<ast::expression> expand_builtin_format(
  const module_::macro_descriptor& desc,
  token_location loc,
  const std::vector<token>& tokens)
{
    /*
     * Validate macro definition.
     * FIXME This only needs to be done when loading the macro descriptor.
     */
    if(desc.directives.size() != 1)
    {
        throw codegen_error(
          loc,
          fmt::format(
            "Expected 1 directive for 'format!', got {}.",
            desc.directives.size()));
    }

    if(desc.directives[0].first != "builtin"
       || !desc.directives[0].second.args.empty())
    {
        throw codegen_error(
          loc,
          fmt::format(
            "Expected 'builtin' directive for 'format' with 0 arguments, got '{}' with {} arguments.",
            desc.directives[0].first, desc.directives[0].second.args.size()));
    }

    if(tokens.empty())
    {
        throw codegen_error(loc, "Cannot evaluate macro 'format!' with no arguments. Consider removing it.");
    }

    if(tokens[0].type != token_type::str_literal)
    {
        throw codegen_error(tokens[0].location, "Expected <string-literal>.");
    }

    // Get format placeholders.
    auto format_placeholders = get_format_string_placeholders(tokens[0].location, tokens[0].s);

    if(2 * format_placeholders.size() + 1 != tokens.size())
    {
        if(!format_placeholders.empty())
        {
            throw codegen_error(loc, "Unmatched format placeholders or syntax error in macro invokation.");
        }
    }

    // String only.
    if(tokens.size() == 1)
    {
        return std::make_unique<ast::literal_expression>(tokens[0].location, tokens[0]);
    }

    std::unique_ptr<ast::expression> lhs;

    // Go through the placeholder list and convert tokens as needed.
    std::size_t last_string_fragment_end = 1;    // skip '"'.
    for(std::size_t i = 0; i < format_placeholders.size(); ++i)
    {
        auto& fph = format_placeholders[i];

        std::size_t token_index = 2 * (i + 1);
        const auto& tok = tokens[token_index];

        if(tokens[token_index - 1].s != ",")
        {
            throw codegen_error(
              tok.location,
              fmt::format(
                "Expected ';', got '{}'.",
                tokens[token_index - 1].s));
        }

        // get string fragment.
        std::string fragment = tokens[0].s.substr(last_string_fragment_end, fph.start - last_string_fragment_end);
        last_string_fragment_end = fph.end;

        if(!fragment.empty())
        {
            if(lhs)
            {
                auto concat_args = std::vector<std::unique_ptr<ast::expression>>{};
                concat_args.emplace_back(std::move(lhs));
                concat_args.emplace_back(
                  std::make_unique<ast::literal_expression>(
                    loc,
                    token{fragment, loc, token_type::str_literal, fragment}));

                auto concat_expr = std::make_unique<ast::namespace_access_expression>(
                  token{"std", loc},
                  std::make_unique<ast::call_expression>(
                    token{"string_concat", loc},
                    std::move(concat_args)));

                lhs = std::move(concat_expr);
            }
            else
            {
                lhs = std::make_unique<ast::literal_expression>(
                  loc, token{fragment, loc, token_type::str_literal, fragment});
            }
        }

        std::unique_ptr<ast::expression> conversion_ast;
        if(fph.type == 'd')
        {
            // The token is an integer literal or identifier.
            if(tok.type == token_type::identifier)
            {
                auto conversion_args = std::vector<std::unique_ptr<ast::expression>>{};
                conversion_args.emplace_back(
                  std::make_unique<ast::variable_reference_expression>(
                    tok));

                conversion_ast = std::make_unique<ast::namespace_access_expression>(
                  token{"std", loc},
                  std::make_unique<ast::call_expression>(
                    token{"i32_to_string", loc},
                    std::move(conversion_args)));
            }
            else if(tok.type == token_type::int_literal)
            {
                conversion_ast = std::make_unique<ast::literal_expression>(
                  tok.location,
                  token{tok.s, tok.location, token_type::str_literal, tok.s});
            }
            else
            {
                throw codegen_error(
                  loc,
                  fmt::format(
                    "Expected <literal> or <identifier>, got '{}'.",
                    tok.s));
            }
        }
        else if(fph.type == 'f')
        {
            // The token is an floating-point literal or identifier.
            if(tok.type == token_type::identifier)
            {
                auto conversion_args = std::vector<std::unique_ptr<ast::expression>>{};
                conversion_args.emplace_back(
                  std::make_unique<ast::variable_reference_expression>(
                    tok));

                conversion_ast = std::make_unique<ast::namespace_access_expression>(
                  token{"std", loc},
                  std::make_unique<ast::call_expression>(
                    token{"f32_to_string", loc},
                    std::move(conversion_args)));
            }
            else if(tok.type == token_type::fp_literal)
            {
                conversion_ast = std::make_unique<ast::literal_expression>(
                  tok.location,
                  token{tok.s, tok.location, token_type::str_literal, tok.s});
            }
            else
            {
                throw codegen_error(
                  loc,
                  fmt::format(
                    "Expected <literal> or <identifier>, got '{}'.",
                    tok.s));
            }
        }
        else if(fph.type == 's')
        {
            // No conversion needed.
            conversion_ast = std::make_unique<ast::literal_expression>(
              tok.location, tok);
        }
        else
        {
            throw codegen_error(
              tok.location,
              fmt::format(
                "Unknown format specified '{}'.",
                fph.type));
        }

        if(lhs)
        {
            auto concat_args = std::vector<std::unique_ptr<ast::expression>>{};
            concat_args.emplace_back(std::move(lhs));
            concat_args.emplace_back(std::move(conversion_ast));

            auto concat_expr = std::make_unique<ast::namespace_access_expression>(
              token{"std", loc},
              std::make_unique<ast::call_expression>(
                token{"string_concat", loc},
                std::move(concat_args)));

            lhs = std::move(concat_expr);
        }
        else
        {
            lhs = std::move(conversion_ast);
        }
    }

    if(!lhs)
    {
        throw codegen_error(loc, "Empty macro expansion.");
    }

    return lhs;
}

}    // namespace slang::codegen::macros