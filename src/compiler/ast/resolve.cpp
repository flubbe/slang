/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "shared/module.h"
#include "compiler/codegen/codegen.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "ast.h"
#include "utils.h"

#include <print>

namespace slang::ast
{

/*
 * type_cast_expression.
 */

void type_cast_expression::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
}

/*
 * namespace_access_expression.
 */

void namespace_access_expression::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
}

/*
 * access_expression.
 */

void access_expression::resolve_names(rs::context& ctx)
{
    lhs->resolve_names(ctx);
}

/*
 * directive_expression.
 */

void directive_expression::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
}

/*
 * variable_reference_expression.
 */

void variable_reference_expression::resolve_names(rs::context& ctx)
{
    if(element_expr)
    {
        element_expr->resolve_names(ctx);
    }

    if(expansion)
    {
        expansion->resolve_names(ctx);
    }

    if(!scope_id.has_value())
    {
        throw cg::codegen_error(
          name.location,
          "No scope information available.");
    }

    if(name.type == token_type::macro_identifier)
    {
        symbol_id = ctx.resolve(
          name.s,
          sema::symbol_type::macro_argument,
          scope_id.value());
    }
    else
    {
        symbol_id = ctx.resolve(
          name.s,
          sema::symbol_type::variable,
          scope_id.value());
        if(symbol_id.has_value())
        {
            return;
        }

        symbol_id = ctx.resolve(
          name.s,
          sema::symbol_type::constant,
          scope_id.value());
        if(symbol_id.has_value())
        {
            return;
        }
    }

    if(!symbol_id.has_value())
    {
        throw cg::codegen_error(
          name.location,
          std::format(
            "Could not resolve symbol '{}'.",
            name.s));
    }
}

/*
 * variable_declaration_expression.
 */

void variable_declaration_expression::resolve_names(rs::context& ctx)
{
    if(expr)
    {
        expr->resolve_names(ctx);
    }
}

/*
 * constant_declaration_expression.
 */

void constant_declaration_expression::resolve_names(rs::context& ctx)
{
    if(expr)
    {
        expr->resolve_names(ctx);
    }
}

/*
 * array_initializer_expression.
 */

void array_initializer_expression::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      exprs,
      [&ctx](std::unique_ptr<expression>& e)
      {
          e->resolve_names(ctx);
      });
}

/*
 * struct_definition_expression.
 */

void struct_definition_expression::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      members,
      [&ctx](std::unique_ptr<variable_declaration_expression>& e)
      {
          e->resolve_names(ctx);
      });
}

/*
 * struct_anonymous_initializer_expression.
 */

void struct_anonymous_initializer_expression::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      initializers,
      [&ctx](std::unique_ptr<expression>& e)
      {
          e->resolve_names(ctx);
      });
}

/*
 * named_initializer.
 */

void named_initializer::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
}

/*
 * struct_named_initializer_expression.
 */

void struct_named_initializer_expression::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      initializers,
      [&ctx](std::unique_ptr<named_initializer>& e)
      {
          e->resolve_names(ctx);
      });
}

/*
 * binary_expression.
 */

void binary_expression::resolve_names(rs::context& ctx)
{
    lhs->resolve_names(ctx);
    rhs->resolve_names(ctx);
}

/*
 * unary_expression.
 */

void unary_expression::resolve_names(rs::context& ctx)
{
    operand->resolve_names(ctx);
}

/*
 * new_expression.
 */

void new_expression::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
}

/*
 * postfix_expression.
 */

void postfix_expression::resolve_names(rs::context& ctx)
{
    identifier->resolve_names(ctx);
}

/*
 * block.
 */

void block::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      exprs,
      [&ctx](std::unique_ptr<expression>& e)
      {
          e->resolve_names(ctx);
      });
}

/*
 * function_expression.
 */

void function_expression::resolve_names(rs::context& ctx)
{
    if(body)
    {
        body->resolve_names(ctx);
    }
}

/*
 * call_expression.
 */

void call_expression::resolve_names(rs::context& ctx)
{
    if(!scope_id.has_value())
    {
        throw cg::codegen_error(
          callee.location,
          "No scope information available.");
    }

    const auto name = get_qualified_callee_name();

    symbol_id = ctx.resolve(
      name,
      sema::symbol_type::function,
      scope_id.value());
    if(!symbol_id.has_value())
    {
        throw cg::codegen_error(
          callee.location,
          std::format(
            "Could not resolve function '{}'.",
            name));
    }

    std::ranges::for_each(
      args,
      [&ctx](std::unique_ptr<expression>& e)
      {
          e->resolve_names(ctx);
      });

    if(index_expr)
    {
        index_expr->resolve_names(ctx);
    }
}

/*
 * macro_invocation.
 */

void macro_invocation::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      exprs,
      [&ctx](std::unique_ptr<expression>& e)
      {
          e->resolve_names(ctx);
      });

    if(index_expr)
    {
        index_expr->resolve_names(ctx);
    }
}

/*
 * return_statement.
 */

void return_statement::resolve_names(rs::context& ctx)
{
    if(expr)
    {
        expr->resolve_names(ctx);
    }
}

/*
 * if_statement.
 */

void if_statement::resolve_names(rs::context& ctx)
{
    condition->resolve_names(ctx);
    if_block->resolve_names(ctx);
    if(else_block)
    {
        else_block->resolve_names(ctx);
    }
}

/*
 * while_statement.
 */

void while_statement::resolve_names(rs::context& ctx)
{
    condition->resolve_names(ctx);
    while_block->resolve_names(ctx);
}

/*
 * macro_branch.
 */

void macro_branch::resolve_names(rs::context& ctx)
{
    body->resolve_names(ctx);
}

/*
 * macro_expression_list:
 */

void macro_expression_list::resolve_names([[maybe_unused]] rs::context& ctx)
{
    throw cg::codegen_error(loc, "Non-expanded macro expression list.");
}

/*
 * macro_expression.
 */

void macro_expression::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      branches,
      [&ctx](std::unique_ptr<macro_branch>& b)
      {
          b->resolve_names(ctx);
      });
}

}    // namespace slang::ast
