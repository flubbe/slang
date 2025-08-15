/**
 * slang - a simple scripting language.
 *
 * constant expression info, used e.g. during constant evaluation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>

#include "ast/ast.h"
#include "constant.h"

namespace slang::const_
{

void env::set_const_info(
  sema::symbol_id id,
  const_info info)
{
    auto it = const_info_map.find(id);
    if(it != const_info_map.end())
    {
        if(it->second != info)
        {
            throw sema::semantic_error(
              std::format(
                "Constant info already exists for the symbol '{}' with a different value.",
                static_cast<int>(id.value)));
        }
    }
    else
    {
        const_info_map.insert({id, std::move(info)});
    }
}

std::optional<const_info> env::get_const_info(
  sema::symbol_id id) const
{
    auto it = const_info_map.find(id);
    if(it == const_info_map.end())
    {
        return std::nullopt;
    }

    return it->second;
}

void env::set_expression_const_eval(
  const ast::expression& expr,
  bool is_const_eval)
{
    const_eval_exprs[&expr] = is_const_eval;
}

void env::set_expression_value(
  const ast::expression& expr,
  const_info info)
{
    const_eval_expr_values[&expr] = std::move(info);
}

const const_info& env::get_expression_value(
  const ast::expression& expr) const
{
    auto it = const_eval_expr_values.find(&expr);
    if(it == const_eval_expr_values.end())
    {
        throw sema::semantic_error(
          std::format(
            "{}: No evaluation result for expression found.",
            ::slang::to_string(expr.get_location())));
    }

    return it->second;
}

}    // namespace slang::const_
