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
#include "compiler/codegen.h"
#include "compiler/typing.h"
#include "ast.h"
#include "utils.h"

namespace slang::ast
{

/*
 * type_cast_expression.
 */

void type_cast_expression::resolve_names()
{
    expr->resolve_names();
}

/*
 * namespace_access_expression.
 */

void namespace_access_expression::resolve_names()
{
    expr->resolve_names();
}

/*
 * access_expression.
 */

void access_expression::resolve_names()
{
    lhs->resolve_names();
    rhs->resolve_names();
}

/*
 * directive_expression.
 */

void directive_expression::resolve_names()
{
    expr->resolve_names();
}

/*
 * variable_reference_expression.
 */

void variable_reference_expression::resolve_names()
{
    if(element_expr)
    {
        element_expr->resolve_names();
    }

    if(expansion)
    {
        expansion->resolve_names();
    }
}

/*
 * variable_declaration_expression.
 */

void variable_declaration_expression::resolve_names()
{
    if(expr)
    {
        expr->resolve_names();
    }
}

/*
 * array_initializer_expression.
 */

void array_initializer_expression::resolve_names()
{
    std::ranges::for_each(
      exprs,
      [](std::unique_ptr<expression>& e)
      {
          e->resolve_names();
      });
}

/*
 * struct_definition_expression.
 */

void struct_definition_expression::resolve_names()
{
    std::ranges::for_each(
      members,
      [](std::unique_ptr<variable_declaration_expression>& e)
      {
          e->resolve_names();
      });
}

/*
 * struct_anonymous_initializer_expression.
 */

void struct_anonymous_initializer_expression::resolve_names()
{
    std::ranges::for_each(
      initializers,
      [](std::unique_ptr<expression>& e)
      {
          e->resolve_names();
      });
}

/*
 * named_initializer.
 */

void named_initializer::resolve_names()
{
    expr->resolve_names();
}

/*
 * struct_named_initializer_expression.
 */

void struct_named_initializer_expression::resolve_names()
{
    std::ranges::for_each(
      initializers,
      [](std::unique_ptr<named_initializer>& e)
      {
          e->resolve_names();
      });
}

/*
 * binary_expression.
 */

void binary_expression::resolve_names()
{
    lhs->resolve_names();
    rhs->resolve_names();
}

/*
 * unary_expression.
 */

void unary_expression::resolve_names()
{
    operand->resolve_names();
}

/*
 * new_expression.
 */

void new_expression::resolve_names()
{
    expr->resolve_names();
}

/*
 * postfix_expression.
 */

void postfix_expression::resolve_names()
{
    identifier->resolve_names();
}

/*
 * block.
 */

void block::resolve_names()
{
    std::ranges::for_each(
      exprs,
      [](std::unique_ptr<expression>& e)
      {
          e->resolve_names();
      });
}

/*
 * function_expression.
 */

void function_expression::resolve_names()
{
    if(body)
    {
        body->resolve_names();
    }
}

/*
 * call_expression.
 */

void call_expression::resolve_names()
{
    std::ranges::for_each(
      args,
      [](std::unique_ptr<expression>& e)
      {
          e->resolve_names();
      });

    if(index_expr)
    {
        index_expr->resolve_names();
    }
}

/*
 * macro_invocation.
 */

void macro_invocation::resolve_names()
{
    std::ranges::for_each(
      exprs,
      [](std::unique_ptr<expression>& e)
      {
          e->resolve_names();
      });

    if(index_expr)
    {
        index_expr->resolve_names();
    }
}

/*
 * return_statement.
 */

void return_statement::resolve_names()
{
    if(expr)
    {
        expr->resolve_names();
    }
}

/*
 * if_statement.
 */

void if_statement::resolve_names()
{
    condition->resolve_names();
    if_block->resolve_names();
    if(else_block)
    {
        else_block->resolve_names();
    }
}

/*
 * while_statement.
 */

void while_statement::resolve_names()
{
    condition->resolve_names();
    while_block->resolve_names();
}

/*
 * macro_branch.
 */

void macro_branch::resolve_names()
{
    body->resolve_names();
}

/*
 * macro_expression_list:
 */

void macro_expression_list::resolve_names()
{
    throw cg::codegen_error(loc, "Non-expanded macro expression list.");
}

/*
 * macro_expression.
 */

void macro_expression::resolve_names()
{
    std::ranges::for_each(
      branches,
      [](std::unique_ptr<macro_branch>& b)
      {
          b->resolve_names();
      });
}

}    // namespace slang::ast
