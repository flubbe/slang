/**
 * slang - a simple scripting language.
 *
 * constant expression evaluation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "shared/module.h"
#include "compiler/codegen/codegen.h"
#include "compiler/typing.h"
#include "ast.h"
#include "utils.h"

namespace slang::ast
{

/*
 * literal_expression.
 */

std::optional<const_::const_info> literal_expression::evaluate(
  ty::context& type_ctx,
  const_::env& env) const
{
    if(!tok.value.has_value())
    {
        throw cg::codegen_error(loc, "Literal expression has no value.");
    }

    if(tok.type == token_type::int_literal)
    {
        return std::make_optional<const_::const_info>(
          {.type = const_::constant_type::i32,
           .value = std::get<std::int32_t>(tok.value.value())});
    }

    if(tok.type == token_type::fp_literal)
    {
        return std::make_optional<const_::const_info>(
          {.type = const_::constant_type::f32,
           .value = std::get<float>(tok.value.value())});
    }

    if(tok.type == token_type::str_literal)
    {
        return std::make_optional<const_::const_info>(
          {.type = const_::constant_type::str,
           .value = std::get<std::string>(tok.value.value())});
    }

    return std::nullopt;
}

/*
 * namespace_access_expression.
 */

std::optional<const_::const_info> namespace_access_expression::evaluate(
  ty::context& type_ctx,
  const_::env& env) const
{
    return expr->evaluate(type_ctx, env);
}

/*
 * variable_reference_expression.
 */

bool variable_reference_expression::is_const_eval(const_::env& env) const
{
    // check whether we're referencing a constant.
    return env.get_const_info(symbol_id.value()) != std::nullopt;
}

std::optional<const_::const_info> variable_reference_expression::evaluate(
  [[maybe_unused]] ty::context& type_ctx,
  const_::env& env) const
{
    return env.get_const_info(symbol_id.value());
}

/*
 * binary_expression.
 */

struct binary_operation_helper
{
    ty::context& ctx;

    source_location loc;
    std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32;
    std::function<float(float, float)> func_f32;

    binary_operation_helper(
      ty::context& ctx,
      const binary_expression& expr,
      std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32,
      std::function<float(float, float)> func_f32)
    : ctx{ctx}
    , loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_f32{std::move(func_f32)}
    {
    }

    const_::const_info operator()(
      const const_::const_info& lhs,
      const const_::const_info& rhs) const
    {
        if(lhs.type != rhs.type)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Operand types don't match for binary operator evaluation: '{}' != '{}'.",
                const_::to_string(lhs.type),
                const_::to_string(rhs.type)));
        }

        if(lhs.type == const_::constant_type::i32)
        {
            return {
              .type = const_::constant_type::i32,
              .value = func_i32(
                std::get<std::int32_t>(lhs.value),
                std::get<std::int32_t>(rhs.value))};
        }

        if(lhs.type == const_::constant_type::f32)
        {
            return {
              .type = const_::constant_type::f32,
              .value = func_f32(
                std::get<float>(lhs.value),
                std::get<float>(rhs.value))};
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid type '{}' for binary operator evaluation.",
            const_::to_string(lhs.type),
            const_::to_string(rhs.type)));
    }
};

bool binary_expression::is_const_eval(const_::env& env) const
{
    // operators that support constant expression evaluation.
    static const std::array<std::string, 18> bin_ops = {
      "+", "-", "*", "/", "%",
      "<<", ">>",
      "<", "<=", ">", ">=", "==", "!=",
      "&", "^", "|",
      "&&", "||"};

    if(std::ranges::find(std::as_const(bin_ops), op.s) == bin_ops.cend())
    {
        return false;
    }

    if(!env.is_expression_const_eval(*lhs).has_value()
       || !env.is_expression_const_eval(*rhs).has_value())
    {
        // visit the nodes to get whether this expression is a compile time constant.
        visit_nodes(
          [&env](const ast::expression& expr)
          {
              env.set_expression_const_eval(expr, expr.is_const_eval(env));
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    auto lhs_const_eval = env.is_expression_const_eval(*lhs);
    auto rhs_const_eval = env.is_expression_const_eval(*rhs);

    return lhs_const_eval.value_or(false)
           && rhs_const_eval.value_or(false);
}

std::optional<const_::const_info> binary_expression::evaluate(
  ty::context& type_ctx,
  const_::env& env) const
{
    if(!is_const_eval(env))
    {
        return {};
    }

    // clang-format off
    static const std::unordered_map<std::string, binary_operation_helper> eval_map = {
      {"+", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a + b; }, 
             [](float a, float b) -> float
             { return a + b; }}},
      {"-", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a - b; }, 
             [](float a, float b) -> float
             { return a - b; }}},
      {"*", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a * b; }, 
             [](float a, float b) -> float
             { return a * b; }}},
      {"/", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a / b; 
             }, 
             [](float a, float b) -> float
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a / b; 
             }}},
      {"%", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a % b; }, [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '%'."); }}},
      {"<<", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a << (b & 0x1f); }, // NOLINT(readability-magic-numbers)
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '<<'."); }}},
      {">>", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a >> (b & 0x1f); }, // NOLINT(readability-magic-numbers)
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '>>'."); }}},
      {"&", {type_ctx, 
              *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a & b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '&'."); }}},
      {"^", {type_ctx, 
              *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a ^ b; },
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '^'."); }}},
      {"|", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a | b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '|'."); }}},
      {"&&", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a && b; }, 
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '&&'."); }}},
      {"||", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a || b; }, 
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '||'."); }}},
    };

    static const std::unordered_map<std::string, binary_operation_helper> comp_map = {
      {"<", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a < b; }, 
             [](float a, float b) -> std::int32_t
             { return a < b; }}},
      {"<=", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a <= b; }, 
              [](float a, float b) -> std::int32_t
              { return a <= b; }}},
      {">", {type_ctx, 
             *this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a > b; }, 
             [](float a, float b) -> std::int32_t
             { return a > b; }}},
      {">=", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a >= b; }, 
              [](float a, float b) -> std::int32_t
              { return a >= b; }}},
      {"==", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a == b; }, 
              [](float a, float b) -> std::int32_t
              { return a == b; }}},
      {"!=", {type_ctx, 
              *this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a != b; }, 
              [](float a, float b) -> std::int32_t
              { return a != b; }}}
    };
    // clang-format on

    if(!env.is_expression_evaluated(*lhs) || !env.is_expression_evaluated(*rhs))
    {
        // visit the nodes to get the expression values.
        visit_nodes(
          [&type_ctx, &env](const ast::expression& node)
          {
              // NOTE Whether evaluation is possible is checked by `is_const_eval`.
              env.set_expression_value(node, node.evaluate(type_ctx, env).value());
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );

        // check that we calculated the required values.
        if(!env.is_expression_evaluated(*lhs) || !env.is_expression_evaluated(*rhs))
        {
            return std::nullopt;
        }
    }

    auto eval_it = eval_map.find(op.s);
    if(eval_it != eval_map.end())
    {
        return eval_it->second(
          env.get_expression_value(*lhs),
          env.get_expression_value(*rhs));
    }

    auto comp_it = comp_map.find(op.s);
    if(comp_it != comp_map.end())
    {
        return comp_it->second(
          env.get_expression_value(*lhs),
          env.get_expression_value(*rhs));
    }

    return std::nullopt;
}

/*
 * unary_expression.
 */

struct unary_operation_helper
{
    ty::context& ctx;

    source_location loc;
    std::function<std::int32_t(std::int32_t)> func_i32;
    std::function<float(float)> func_f32;

    unary_operation_helper(
      ty::context& ctx,
      const unary_expression& expr,
      std::function<std::int32_t(std::int32_t)> func_i32,
      std::function<float(float)> func_f32)
    : ctx{ctx}
    , loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_f32{std::move(func_f32)}
    {
    }

    const_::const_info operator()(const const_::const_info& v) const
    {
        if(v.type == const_::constant_type::i32)
        {
            return {
              .type = const_::constant_type::i32,
              .value = func_i32(std::get<std::int32_t>(v.value))};
        }

        if(v.type == const_::constant_type::f32)
        {
            return {
              .type = const_::constant_type::f32,
              .value = func_f32(std::get<float>(v.value))};
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid type '{}' for unary operator evaluation.",
            const_::to_string(v.type)));
    }
};

bool unary_expression::is_const_eval(const_::env& env) const
{
    if(op.s != "+" && op.s != "-" && op.s != "!" && op.s != "~")
    {
        return false;
    }

    if(!env.is_expression_const_eval(*operand).has_value())
    {
        // visit the nodes to get whether this expression is a compile time constant.
        visit_nodes(
          [&env](const ast::expression& node)
          {
              env.set_expression_const_eval(node, node.is_const_eval(env));
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    return operand->is_const_eval(env);
}

std::optional<const_::const_info> unary_expression::evaluate(
  ty::context& type_ctx,
  const_::env& env) const
{
    if(!is_const_eval(env))
    {
        return {};
    }

    static const std::unordered_map<std::string, unary_operation_helper> eval_map = {
      {"+", {
              type_ctx,
              *this,
              [](std::int32_t a) -> std::int32_t
              {
                  return a;
              },
              [](float a) -> float
              {
                  return a;
              },
            }},
      {"-", {
              type_ctx,
              *this,
              [](std::int32_t a) -> std::int32_t
              {
                  return -a;
              },
              [](float a) -> float
              {
                  return -a;
              },
            }},
      {"!", {
              type_ctx,
              *this,
              [](std::int32_t a) -> std::int32_t
              {
                  return a == 0;    // matches the generated opcodes.
              },
              [](float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for unary operator '!'.");
              },
            }},
      {"~", {
              type_ctx,
              *this,
              [](std::int32_t a) -> std::int32_t
              {
                  return static_cast<std::int32_t>(~0) ^ a;    // matches the generated opcodes.
              },
              [](float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for unary operator '!'.");
              },
            }}};

    auto it = eval_map.find(op.s);
    if(it == eval_map.end())
    {
        return std::nullopt;
    }

    if(!env.is_expression_evaluated(*operand))
    {
        // visit the nodes to get the expression values.
        visit_nodes(
          [&type_ctx, &env](const ast::expression& node)
          {
              // NOTE Whether evaluation is possible is checked by `is_const_eval`.
              env.set_expression_value(node, node.evaluate(type_ctx, env).value());
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );

        // check that we calculated the required value.
        if(!env.is_expression_evaluated(*operand))
        {
            return std::nullopt;
        }
    }

    return it->second(env.get_expression_value(*operand));
}

}    // namespace slang::ast
