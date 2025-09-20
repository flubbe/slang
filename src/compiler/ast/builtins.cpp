/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (built-ins).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/codegen/codegen.h"
#include "compiler/typing.h"
#include "builtins.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/*
 * format_macro_expander.
 */

const token& format_macro_expander::get_format_token()
{
    if(exprs.empty())
    {
        throw cg::codegen_error(
          loc,
          "Cannot evaluate macro 'format!' with no arguments. Consider removing it.");
    }

    if(!exprs[0]->is_literal())
    {
        throw cg::codegen_error(
          loc,
          "Cannot evaluate macro 'format!': Expected <string-literal> as its first argument.");
    }

    auto* format_expr = exprs[0]->as_literal();
    const auto& format_token = format_expr->get_token();
    if(format_token.type != token_type::str_literal)
    {
        throw cg::codegen_error(
          format_token.location,
          "Expected <string-literal>.");
    }

    return format_token;
}

void format_macro_expander::create_format_string_placeholders()
{
    std::optional<format_string_placeholder> current_placeholder{std::nullopt};
    std::string current_format_specifier;

    for(std::size_t i = 0; i < format_string.s.length(); ++i)
    {
        if(format_string.s[i] == '{')
        {
            if(current_placeholder.has_value())
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Invalid format string '{}'.",
                    format_string.s));
            }

            if(i + 1 < format_string.s.length() && format_string.s[i + 1] == '{')
            {
                placeholders.emplace_back(
                  format_string_placeholder{
                    .start = i,
                    .end = i + 1,
                    .type = '{'});
                i += 1;
                continue;
            }

            current_placeholder = format_string_placeholder{.start = i, .end = 0, .type = 0};
        }
        else if(format_string.s[i] == '}')
        {
            if(current_placeholder.has_value())
            {
                current_placeholder->end = i + 1;

                if(current_format_specifier.length() > 1
                   || (current_format_specifier.length() == 1
                       && (current_format_specifier[0] != 'd'
                           && current_format_specifier[0] != 'f'
                           && current_format_specifier[0] != 's')))
                {
                    throw cg::codegen_error(
                      loc,
                      std::format(
                        "Unsupported format specifier '{}'.",
                        current_format_specifier));
                }
                current_placeholder->type = !current_format_specifier.empty()
                                              ? std::make_optional(current_format_specifier[0])
                                              : std::nullopt;
                current_format_specifier.clear();

                placeholders.emplace_back(*current_placeholder);
                current_placeholder = std::nullopt;
            }
            else
            {
                if(i + 1 < format_string.s.length() && format_string.s[i + 1] == '}')
                {
                    placeholders.emplace_back(
                      format_string_placeholder{
                        .start = i,
                        .end = i + 1,
                        .type = '}'});
                    i += 1;
                    continue;
                }

                throw cg::codegen_error(
                  loc,
                  std::format(
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
        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid format string '{}'.",
            format_string.s));
    }
}

/*
 * format_macro_expression.
 */

std::unique_ptr<cg::value> format_macro_expression::generate_code(
  cg::context& ctx,
  memory_context mc) const
{
    if(placeholders.size() + 1 != exprs.size())
    {
        throw cg::codegen_error(
          loc,
          "Unmatched format placeholders or syntax error in macro invocation.");
    }

    const auto& format_string = exprs[0]->as_literal()->get_token();

    // String only.
    if(exprs.size() == 1)
    {
        return ast::literal_expression{
          format_string.location,
          format_string}
          .generate_code(ctx, mc);
    }

    std::unique_ptr<ast::expression> lhs;

    // Go through the placeholder list and convert tokens as needed.
    std::size_t last_string_fragment_end = 1;    // skip '"'.
    for(std::size_t i = 0; i < placeholders.size(); ++i)
    {
        const auto& fph = placeholders[i];

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
            throw cg::codegen_error(
              expr->get_location(),
              std::format(
                "Unknown format specified '{}'.",
                fph.type.has_value()
                  ? std::format("{}", fph.type.value())
                  : std::string("<unspecified>")));
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
        throw cg::codegen_error(loc, "Empty macro expansion.");
    }

    // trailing format string fragments.
    if(last_string_fragment_end != format_string.s.length() - 1)    // skip trailing quote.
    {
        std::string fragment = format_string.s.substr(
          last_string_fragment_end,
          format_string.s.length() - last_string_fragment_end - 1);

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

    return lhs->generate_code(ctx, mc);
}

std::optional<ty::type_id> format_macro_expression::type_check(ty::context& ctx, sema::env& env)
{
    // TODO
    throw std::runtime_error("format_macro_expression::type_check");

    // if(exprs.empty())
    // {
    //     throw ty::type_error(
    //       loc,
    //       std::format(
    //         "Macro 'format!': No format string found."));
    // }

    // format_macro_expander expander{loc, exprs};
    // if(expander.get_placeholders().size() != exprs.size() - 1)
    // {
    //     throw ty::type_error(
    //       loc,
    //       std::format(
    //         "Macro 'format!': Argument count does not match placeholder count: {} != {}.",
    //         exprs.size() - 1,
    //         expander.get_placeholders().size()));
    // }

    // // check that all expressions and specifiers match, or that the inferred type is supported.
    // for(std::size_t i = 1; i < exprs.size(); ++i)
    // {
    //     const auto& e = exprs[i];
    //     const auto& p = expander.get_placeholders()[i - 1];

    //     std::optional<ty::type_id> t = e->type_check(ctx);
    //     if(!t.has_value())
    //     {
    //         throw ty::type_error(
    //           e->get_location(),
    //           std::format(
    //             "Macro 'format!': Argument at position '{}' has no type.",
    //             i));
    //     }

    //     // FIXME Only i32, f32 and str is supported.
    //     auto type_str = t.value().to_string();
    //     if(p.type.has_value())
    //     {
    //         if((type_str == "i32" && p.type.value() == 'd')
    //            || (type_str == "f32" && p.type.value() == 'f')
    //            || (type_str == "str" && p.type.value() == 's'))
    //         {
    //             placeholders.emplace_back(p);
    //         }
    //         else
    //         {
    //             throw ty::type_error(
    //               e->get_location(),
    //               std::format(
    //                 "Macro 'format!': Argument at position {} has wrong type.",
    //                 i));
    //         }
    //     }
    //     else
    //     {
    //         // store type in placeholders.
    //         if(type_str == "i32")
    //         {
    //             placeholders.emplace_back(format_string_placeholder{
    //               .start = p.start,
    //               .end = p.end,
    //               .type = 'd'});
    //         }
    //         else if(type_str == "f32")
    //         {
    //             placeholders.emplace_back(format_string_placeholder{
    //               .start = p.start,
    //               .end = p.end,
    //               .type = 'f'});
    //         }
    //         else if(type_str == "str")
    //         {
    //             placeholders.emplace_back(format_string_placeholder{
    //               .start = p.start,
    //               .end = p.end,
    //               .type = 's'});
    //         }
    //         else
    //         {
    //             throw ty::type_error(
    //               e->get_location(),
    //               std::format(
    //                 "Macro 'format!': Argument at position '{}' is not convertible to a string.",
    //                 i));
    //         }
    //     }
    // }

    // return ctx.get_type("str", false);
}

std::string format_macro_expression::to_string() const
{
    std::string ret = std::format("FormatMacroExpression(exprs=(");
    if(!exprs.empty())
    {
        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            ret += std::format("{}, ", exprs[i]->to_string());
        }
        ret += exprs.back()->to_string();
    }
    return ret;
}

}    // namespace slang::ast
