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

std::string to_string(constant_type c)
{
    switch(c)
    {
    case constant_type::i8: return "i8";
    case constant_type::i16: return "i16";
    case constant_type::i32: return "i32";
    case constant_type::i64: return "i64";
    case constant_type::f32: return "f32";
    case constant_type::f64: return "f64";
    case constant_type::str: return "str";
    default: return "<unknown>";
    }
}

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
                id.value));
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

void env::register_constant(sema::symbol_id id)
{
    if(constant_registry.contains(id))
    {
        throw sema::semantic_error(
          std::format(
            "Constant already registered for the symbol '{}'.",
            id.value));
    }

    constant_registry.insert(id);
}

constant_id env::intern(std::int32_t i)
{
    auto it = std::ranges::find_if(
      const_literal_map,
      [i](const auto& p) -> bool
      {
          return p.second.type == constant_type::i32
                 && std::get<std::int32_t>(p.second.value) == i;
      });
    if(it != const_literal_map.end())
    {
        return it->first;
    }

    if(const_literal_map.size() == std::numeric_limits<std::size_t>::max())
    {
        // should never happen.
        throw std::runtime_error(
          std::format(
            "Too many constants in constant environment (>= std::numeric_limits<std::size_t>::max() = {})",
            std::numeric_limits<std::size_t>::max()));
    }

    constant_id id = const_literal_map.size();
    const_literal_map.insert(
      {id,
       const_info{
         .origin_module_id = sema::symbol_info::current_module_id,
         .type = constant_type::i32,
         .value = i}});

    return id;
}

constant_id env::intern(float f)
{
    auto it = std::ranges::find_if(
      const_literal_map,
      [f](const auto& p) -> bool
      {
          return p.second.type == constant_type::f32
                 && std::get<float>(p.second.value) == f;
      });
    if(it != const_literal_map.end())
    {
        return it->first;
    }

    if(const_literal_map.size() == std::numeric_limits<std::size_t>::max())
    {
        // should never happen.
        throw std::runtime_error(
          std::format(
            "Too many constants in constant environment (>= std::numeric_limits<std::size_t>::max() = {})",
            std::numeric_limits<std::size_t>::max()));
    }

    constant_id id = const_literal_map.size();
    const_literal_map.insert(
      {id,
       const_info{
         .origin_module_id = sema::symbol_info::current_module_id,
         .type = constant_type::f32,
         .value = f}});

    return id;
}

constant_id env::intern(std::string s)
{
    auto it = std::ranges::find_if(
      const_literal_map,
      [&s](const auto& p) -> bool
      {
          return p.second.type == constant_type::str
                 && std::get<std::string>(p.second.value) == s;
      });
    if(it != const_literal_map.end())
    {
        return it->first;
    }

    if(const_literal_map.size() == std::numeric_limits<std::size_t>::max())
    {
        // should never happen.
        throw std::runtime_error(
          std::format(
            "Too many constants in constant environment (>= std::numeric_limits<std::size_t>::max() = {})",
            std::numeric_limits<std::size_t>::max()));
    }

    constant_id id = const_literal_map.size();
    const_literal_map.insert(
      {id,
       const_info{
         .origin_module_id = sema::symbol_info::current_module_id,
         .type = constant_type::str,
         .value = std::move(s)}});

    return id;
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
