/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (built-ins).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/codegen/codegen.h"
#include "compiler/format_util.h"
#include "compiler/macro_format.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "builtins.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/*
 * format_macro_expression.
 */

void format_macro_expression::expand_late_macros(
  ty::context& ctx,
  sema::env& env)
{
    if(expansion)
    {
        throw cg::codegen_error(
          loc,
          "Double 'format!' macro expansion.");
    }

    std::vector<ty::type_id> arg_types;
    arg_types.reserve(exprs.size() > 1 ? exprs.size() - 1 : 0);
    for(std::size_t i = 1; i < exprs.size(); ++i)
    {
        std::optional<ty::type_id> arg_type = exprs[i]->type_check(ctx, env);
        if(!arg_type.has_value())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "No type for argument at position {}.",
                i));
        }
        arg_types.push_back(arg_type.value());
    }

    token_stream expansion_tokens;
    try
    {
        expansion_tokens = macro::expand_format_builtin(
          arg_tokens,
          arg_types,
          loc,
          ctx);
    }
    catch(const macro::macro_error& e)
    {
        throw cg::codegen_error(loc, e.what());
    }

    expansion = parser::parse_expression(std::move(expansion_tokens));
}

std::unique_ptr<cg::rvalue> format_macro_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    if(!expansion)
    {
        throw cg::codegen_error(
          loc,
          "Empty 'format!' macro expansion.");
    }

    return expansion->emit_rvalue(ctx, result_used);
}

void format_macro_expression::collect_names(co::context& ctx)
{
    if(!expansion)
    {
        // early expansion.
        super::collect_names(ctx);
    }
    else
    {
        // late expansion.
        expansion->collect_names(ctx);
    }
}

void format_macro_expression::resolve_names(rs::context& ctx)
{
    if(expansion)
    {
        expansion->resolve_names(ctx);
    }
}

std::optional<ty::type_id> format_macro_expression::type_check(ty::context& ctx, sema::env& env)
{
    if(expansion)
    {
        return expansion->type_check(ctx, env);
    }

    if(exprs.empty())
    {
        throw ty::type_error(
          loc,
          "Macro 'format!': No format string found.");
    }

    if(!exprs[0]->is_literal())
    {
        throw ty::type_error(
          exprs[0]->get_location(),
          "Expected <string-literal>.");
    }

    auto* format_expr = exprs[0]->as_literal();
    const auto& format_token = format_expr->get_token();
    if(format_token.type != token_type::str_literal)
    {
        throw ty::type_error(
          format_token.location,
          "Expected <string-literal>.");
    }

    if(exprs[0]->type_check(ctx, env) != ctx.get_str_type())
    {
        throw ty::type_error(
          exprs[0]->get_location(),
          "Expected <string-literal>.");
    }

    if(format_token.s.length() < 2)
    {
        throw ty::type_error(
          format_token.location,
          "Macro 'format!': Malformed format string.");
    }

    std::string fmt = format_token.s.substr(1, format_token.s.length() - 2);

    std::vector<format_util::placeholder> placeholders;
    try
    {
        placeholders = format_util::parse_format_string(fmt, format_token.location);
    }
    catch(const std::exception& e)
    {
        throw ty::type_error(loc, e.what());
    }

    if(placeholders.size() != exprs.size() - 1)
    {
        throw ty::type_error(
          loc,
          std::format(
            "Macro 'format!': Argument count does not match placeholder count: {} != {}.",
            exprs.size() - 1,
            placeholders.size()));
    }

    for(std::size_t i = 1; i < exprs.size(); ++i)
    {
        const auto& e = exprs[i];
        const auto& p = placeholders[i - 1];

        std::optional<ty::type_id> t = e->type_check(ctx, env);
        if(!t.has_value())
        {
            throw ty::type_error(
              e->get_location(),
              std::format(
                "Macro 'format!': Argument at position '{}' has no type.",
                i));
        }

        try
        {
            format_util::validate_arg_type(
              t.value(),
              p.specifier,
              ctx,
              e->get_location());
        }
        catch(const std::exception& err)
        {
            throw ty::type_error(e->get_location(), err.what());
        }
    }

    return ctx.get_str_type();
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
