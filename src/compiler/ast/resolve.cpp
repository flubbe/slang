/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "shared/module.h"
#include "compiler/codegen/codegen.h"
#include "compiler/diagnostics.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "ast.h"
#include "utils.h"

namespace diag = slang::compiler::diagnostics;

namespace slang::ast
{

/*
 * expression_statement.
 */

void expression_statement::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
}

/*
 * type_expression.
 */

void type_expression::resolve_names(rs::context& ctx)
{
    // FIXME The type could be a built-in type, which is not visible
    //       by the resolver context.
    ctx.resolve(
      get_qualified_name(),
      sema::symbol_type::type,
      sema::scope::invalid_id,
      type_name.context);
}

/*
 * type_cast_expression.
 */

void type_cast_expression::resolve_names(rs::context& ctx)
{
    target_type->resolve_names(ctx);
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

void variable_reference_expression::resolve_names(
  rs::context& ctx)
{
    if(!scope_id.has_value())
    {
        throw internal_error{
          loc,
          "No scope information available for variable reference."};
    }

    if(expansion)
    {
        expansion->resolve_names(ctx);
    }

    if(name.type == token_type::macro_identifier)
    {
        symbol_id = ctx.resolve(
          get_qualified_name(),
          sema::symbol_type::macro_argument,
          scope_id.value(),
          name.context);
    }
    else
    {
        symbol_id = ctx.resolve(
          get_qualified_name(),
          sema::symbol_type::variable,
          scope_id.value(),
          name.context);
        if(symbol_id.has_value())
        {
            return;
        }

        symbol_id = ctx.resolve(
          get_qualified_name(),
          sema::symbol_type::constant,
          scope_id.value(),
          name.context);
        if(symbol_id.has_value())
        {
            return;
        }
    }

    if(!symbol_id.has_value())
    {
        auto& d = ctx.get_diagnostics().report_error(
          ctx.get_filename(),
          diag::source_span{name},
          std::format(
            "unknown variable `{}`",
            get_qualified_name()));
        d.with_label("unknown variable");

        auto candidates = ctx.get_visible_symbol_names(scope_id.value(), sema::symbol_type::variable);
        auto const_candidates = ctx.get_visible_symbol_names(scope_id.value(), sema::symbol_type::constant);
        candidates.insert(candidates.end(), const_candidates.begin(), const_candidates.end());

        if(auto hint = diag::find_closest_match(get_qualified_name(), candidates))
        {
            d.with_help(
              std::format(
                "did you mean `{}`?",
                *hint));
        }
    }
}

/*
 * array_subscript_expression.
 */

void array_subscript_expression::resolve_names(rs::context& ctx)
{
    lhs->resolve_names(ctx);
    subscript_expr->resolve_names(ctx);
}

/*
 * variable_declaration_expression.
 */

void variable_declaration_expression::resolve_names(rs::context& ctx)
{
    type->resolve_names(ctx);

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
    if(!scope_id.has_value())
    {
        throw internal_error{
          loc,
          "No scope information available for struct initializer."};
    }

    const std::string qualified_name = get_qualified_name();

    symbol_id = ctx.resolve(
      qualified_name,
      sema::symbol_type::type,
      scope_id.value(),
      name.context);
    if(!symbol_id.has_value())
    {
        auto& d = ctx.get_diagnostics().report_error(
          ctx.get_filename(),
          diag::source_span{name},
          std::format(
            "unknown type `{}`",
            qualified_name));
        d.with_label("unknown type");

        auto candidates = ctx.get_visible_symbol_names(scope_id.value(), sema::symbol_type::type);
        if(auto hint = diag::find_closest_match(qualified_name, candidates))
        {
            d.with_help(
              std::format(
                "did you mean `{}`?",
                *hint));
        }
    }

    std::ranges::for_each(
      initializers,
      [&ctx](std::unique_ptr<named_initializer>& e)
      {
          e->resolve_names(ctx);
      });
}

/*
 * assignment_expression.
 */

void assignment_expression::resolve_names(rs::context& ctx)
{
    lhs->resolve_names(ctx);
    rhs->resolve_names(ctx);
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
    type_expr->resolve_names(ctx);
    array_length_expr->resolve_names(ctx);
}

/*
 * postfix_expression.
 */

void postfix_expression::resolve_names(rs::context& ctx)
{
    expr->resolve_names(ctx);
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
        throw internal_error{
          loc,
          "No scope information available for function call."};
    }

    const auto name = get_qualified_callee_name();

    symbol_id = ctx.resolve(
      name,
      sema::symbol_type::function,
      scope_id.value(),
      callee.context);
    if(!symbol_id.has_value())
    {
        auto& d = ctx.get_diagnostics().report_error(
          ctx.get_filename(),
          diag::source_span{callee},
          std::format(
            "unknown function `{}`",
            name));
        d.with_label("unknown function");

        auto candidates = ctx.get_visible_symbol_names(scope_id.value(), sema::symbol_type::function);
        if(auto hint = diag::find_closest_match(name, candidates))
        {
            d.with_help(
              std::format(
                "did you mean `{}`?",
                *hint));
        }
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
    if(!scope_id.has_value())
    {
        throw internal_error{
          loc,
          "No scope information available for macro invocation."};
    }

    const auto qualified_name = get_qualified_callee_name();

    symbol_id = ctx.resolve(
      qualified_name,
      sema::symbol_type::macro,
      scope_id.value(),
      name.context);
    if(!symbol_id.has_value())
    {
        auto& d = ctx.get_diagnostics().report_error(
          ctx.get_filename(),
          diag::source_span{name},
          std::format(
            "unknown macro `{}`",
            qualified_name));
        d.with_label("unknown macro");

        auto candidates = ctx.get_visible_symbol_names(scope_id.value(), sema::symbol_type::macro);
        if(auto hint = diag::find_closest_match(qualified_name, candidates))
        {
            d.with_help(
              std::format(
                "did you mean `{}`?",
                *hint));
        }
    }

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
 * macro_rule.
 */

void macro_rule::resolve_names(rs::context& ctx)
{
    if(validation_body)
    {
        validation_body->resolve_names(ctx);
    }
}

/*
 * macro_expression_list:
 */

void macro_expression_list::resolve_names([[maybe_unused]] rs::context& ctx)
{
    // no-op.
}

/*
 * macro_expression.
 */

void macro_expression::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      branches,
      [&ctx](std::unique_ptr<macro_rule>& branch)
      {
          branch->resolve_names(ctx);
      });
}

/*
 * translation_unit.
 */

void translation_unit::resolve_names(rs::context& ctx)
{
    std::ranges::for_each(
      decls,
      [&ctx](std::unique_ptr<expression>& d)
      {
          d->resolve_names(ctx);
      });
}

}    // namespace slang::ast
