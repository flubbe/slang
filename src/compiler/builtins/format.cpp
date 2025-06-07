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

class format_macro_expander
{
    /** Descriptor for the `format!` macro. */
    const module_::macro_descriptor& desc;

    /** Location of the macro invokation. */
    token_location loc;

    /** Expressions the macro operates on. */
    const std::vector<std::unique_ptr<ast::expression>>& exprs;

    /** Format string. */
    const token& format_string;

    /** Format specifiers/placeholders. */
    std::vector<format_string_placeholder> placeholders;

    /**
     * Check and return the token holding the format string.
     *
     * @throws Throws a `codegen_error` if validation failed.
     * @returns Returns the token holding the format string.
     */
    const token& get_format_token()
    {
        if(exprs.empty())
        {
            throw codegen_error(
              loc,
              "Cannot evaluate macro 'format!' with no arguments. Consider removing it.");
        }

        if(!exprs[0]->is_literal())
        {
            throw codegen_error(
              loc,
              "Cannot evaluate macro 'format!': Expected <string-literal> as its first argument.");
        }

        auto* format_expr = exprs[0]->as_literal();
        const auto& format_token = format_expr->get_token();
        if(format_token.type != token_type::str_literal)
        {
            throw codegen_error(
              format_token.location,
              "Expected <string-literal>.");
        }

        return format_token;
    }

    /**
     * Validate macro definition and arguments.
     *
     * @throws Throws a `codegen_error` if validation fails.
     */
    void validate()
    {
        // FIXME This only needs to be done when loading the macro descriptor.
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
    }

    /**
     * Create the format string placeholders.
     *
     * @throws Throws a `codegen_error` when an invalid or unsupported
     *         format specifier was found.
     */
    void create_format_string_placeholders()
    {
        std::optional<format_string_placeholder> current_placeholder{std::nullopt};
        std::string current_format_specifier;

        for(std::size_t i = 0; i < format_string.s.length(); ++i)
        {
            if(format_string.s[i] == '{')
            {
                if(current_placeholder.has_value())
                {
                    throw codegen_error(
                      loc,
                      fmt::format(
                        "Invalid format string '{}'.",
                        format_string.s));
                }

                if(i + 1 < format_string.s.length() && format_string.s[i + 1] == '{')
                {
                    placeholders.emplace_back(
                      format_string_placeholder{i, i + 1, '{'});
                    i += 1;
                    continue;
                }

                current_placeholder = format_string_placeholder{i, 0, 0};
            }
            else if(format_string.s[i] == '}')
            {
                if(current_placeholder.has_value())
                {
                    current_placeholder->end = i + 1;

                    if(current_format_specifier.length() != 1
                       || (current_format_specifier[0] != 'd'
                           && current_format_specifier[0] != 'f'
                           && current_format_specifier[0] != 's'))
                    {
                        throw codegen_error(
                          loc,
                          fmt::format(
                            "Unsupported format specifier '{}'.",
                            current_format_specifier));
                    }
                    current_placeholder->type = current_format_specifier[0];
                    current_format_specifier.clear();

                    placeholders.emplace_back(*current_placeholder);
                    current_placeholder = std::nullopt;
                }
                else
                {
                    if(i + 1 < format_string.s.length() && format_string.s[i + 1] == '}')
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
                        format_string.s));
                }
            }
            else
            {
                if(current_placeholder.has_value())
                {
                    current_format_specifier += format_string.s[i];
                }
            }
        }

        if(current_placeholder.has_value())
        {
            throw codegen_error(
              loc,
              fmt::format(
                "Invalid format string '{}'.",
                format_string.s));
        }
    }

public:
    /** Deleted constructors. */
    format_macro_expander() = delete;
    format_macro_expander(const format_macro_expander&) = delete;
    format_macro_expander(format_macro_expander&&) = delete;

    /** Default destructor. */
    ~format_macro_expander() = default;

    /** Deleted assignments. */
    format_macro_expander& operator=(const format_macro_expander&) = delete;
    format_macro_expander& operator=(format_macro_expander&&) = delete;

    /**
     * Constructor.
     *
     * @param desc Descriptor for the `format!` macro.
     * @param loc Location of the macro invokation.
     * @param exprs Expressions the macro operates on.
     */
    format_macro_expander(
      const module_::macro_descriptor& desc,
      token_location loc,
      const std::vector<std::unique_ptr<ast::expression>>& exprs)
    : desc{desc}
    , loc{loc}
    , exprs{exprs}
    , format_string{get_format_token()}
    {
        validate();
        create_format_string_placeholders();
    }

    /** Return the token containing the format string. */
    [[nodiscard]]
    const token& get_format_string() const
    {
        return format_string;
    }

    /** Return the format specifiers/placeholders. */
    [[nodiscard]]
    const std::vector<format_string_placeholder>& get_placeholders() const
    {
        return placeholders;
    }
};

std::unique_ptr<ast::expression> expand_builtin_format(
  const module_::macro_descriptor& desc,
  token_location loc,
  const std::vector<std::unique_ptr<ast::expression>>& exprs)
{
    format_macro_expander expander{desc, loc, exprs};

    auto format_placeholders = expander.get_placeholders();
    if(format_placeholders.size() + 1 != exprs.size())
    {
        if(!format_placeholders.empty())
        {
            throw codegen_error(
              loc,
              "Unmatched format placeholders or syntax error in macro invokation.");
        }
    }

    auto format_string = expander.get_format_string();

    // String only.
    if(exprs.size() == 1)
    {
        return std::make_unique<ast::literal_expression>(format_string.location, format_string);
    }

    std::unique_ptr<ast::expression> lhs;

    // Go through the placeholder list and convert tokens as needed.
    std::size_t last_string_fragment_end = 1;    // skip '"'.
    for(std::size_t i = 0; i < format_placeholders.size(); ++i)
    {
        auto& fph = format_placeholders[i];

        std::size_t expr_index = i + 1;
        const auto& expr = exprs[expr_index];

        // get string fragment.
        std::string fragment = format_string.s.substr(
          last_string_fragment_end,
          fph.start - last_string_fragment_end);
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
            auto conversion_args = std::vector<std::unique_ptr<ast::expression>>{};
            conversion_args.emplace_back(expr->clone());

            conversion_ast = std::make_unique<ast::namespace_access_expression>(
              token{"std", loc},
              std::make_unique<ast::call_expression>(
                token{"i32_to_string", loc},
                std::move(conversion_args)));
        }
        else if(fph.type == 'f')
        {
            auto conversion_args = std::vector<std::unique_ptr<ast::expression>>{};
            conversion_args.emplace_back(expr->clone());

            conversion_ast = std::make_unique<ast::namespace_access_expression>(
              token{"std", loc},
              std::make_unique<ast::call_expression>(
                token{"f32_to_string", loc},
                std::move(conversion_args)));
        }
        else if(fph.type == 's')
        {
            // No conversion needed.
            conversion_ast = expr->clone();
        }
        else
        {
            throw codegen_error(
              expr->get_location(),
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
