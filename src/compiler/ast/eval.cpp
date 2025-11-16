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
  [[maybe_unused]] ty::context& type_ctx,
  [[maybe_unused]] const_::env& env) const
{
    if(!tok.value.has_value())
    {
        throw cg::codegen_error(loc, "Literal expression has no value.");
    }

    // default to i32 for integer literals and f64 for floating-point literals.

    if(tok.type == token_type::int_literal)
    {
        return std::make_optional<const_::const_info>(
          {.origin_module_id = sema::symbol_info::current_module_id,
           .type = const_::constant_type::i32,
           .value = std::get<std::int64_t>(tok.value.value())});
    }

    if(tok.type == token_type::fp_literal)
    {
        return std::make_optional<const_::const_info>(
          {.origin_module_id = sema::symbol_info::current_module_id,
           .type = const_::constant_type::f64,
           .value = std::get<double>(tok.value.value())});
    }

    if(tok.type == token_type::str_literal)
    {
        return std::make_optional<const_::const_info>(
          {.origin_module_id = sema::symbol_info::current_module_id,
           .type = const_::constant_type::str,
           .value = std::get<std::string>(tok.value.value())});
    }

    return std::nullopt;
}

/*
 * type_cast_expression.
 */

std::optional<const_::const_info> type_cast_expression::evaluate(
  ty::context& type_ctx,
  const_::env& env) const
{
    if(!env.is_expression_evaluated(*expr))
    {
        // visit the nodes to get the expression values.
        visit_nodes(
          [&type_ctx, &env](const ast::expression& node)
          {
              env.set_expression_const_eval(node, false);

              std::optional<const_::const_info> result = node.evaluate(type_ctx, env);
              if(result.has_value())
              {
                  env.set_expression_const_eval(node, true);
                  env.set_expression_value(node, result.value());
              }
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );

        // check that we calculated the required value.
        if(!env.is_expression_evaluated(*expr))
        {
            return std::nullopt;
        }
    }

    // type cast.
    const auto& v = env.get_expression_value(*expr);
    if(v.type == const_::constant_type::i32)
    {
        if(target_type->get_type() == type_ctx.get_i8_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = std::get<std::int64_t>(v.value)
                       & 0xff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i16_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = std::get<std::int64_t>(v.value)
                       & 0xffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i32_type())
        {
            return v;
        }

        if(target_type->get_type() == type_ctx.get_i64_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i64,
              .value = std::get<std::int64_t>(v.value)};
        }

        if(target_type->get_type() == type_ctx.get_f32_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f32,
              .value = static_cast<float>(std::get<std::int64_t>(v.value))};
        }

        if(target_type->get_type() == type_ctx.get_f64_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f64,
              .value = static_cast<double>(std::get<std::int64_t>(v.value))};
        }

        throw ty::type_error(
          loc,
          std::format(
            "Invalid compile-time evaluated cast '{}' -> '{}'.",
            const_::to_string(v.type),
            type_ctx.to_string(target_type->get_type())));
    }

    if(v.type == const_::constant_type::i64)
    {
        if(target_type->get_type() == type_ctx.get_i8_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = std::get<std::int64_t>(v.value)
                       & 0xff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i16_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = std::get<std::int64_t>(v.value)
                       & 0xffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i32_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = std::get<std::int64_t>(v.value)
                       & 0xffffffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i64_type())
        {
            return v;
        }

        if(target_type->get_type() == type_ctx.get_f32_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f32,
              .value = static_cast<float>(std::get<std::int64_t>(v.value))};
        }

        if(target_type->get_type() == type_ctx.get_f64_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f64,
              .value = static_cast<double>(std::get<std::int64_t>(v.value))};
        }

        throw ty::type_error(
          loc,
          std::format(
            "Invalid compile-time evaluated cast '{}' -> '{}'.",
            const_::to_string(v.type),
            type_ctx.to_string(target_type->get_type())));
    }

    if(v.type == const_::constant_type::f32)
    {
        if(target_type->get_type() == type_ctx.get_i8_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = static_cast<std::int64_t>(
                         std::get<double>(v.value))
                       & 0xff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i16_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = static_cast<std::int64_t>(
                         std::get<double>(v.value))
                       & 0xffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i32_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = static_cast<std::int64_t>(
                         std::get<double>(v.value))
                       & 0xffffffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i64_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i64,
              .value = static_cast<std::int64_t>(
                std::get<double>(v.value))};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_f32_type())
        {
            return v;
        }

        if(target_type->get_type() == type_ctx.get_f64_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f64,
              .value = std::get<double>(v.value)};
        }

        throw ty::type_error(
          loc,
          std::format(
            "Invalid compile-time evaluated cast '{}' -> '{}'.",
            const_::to_string(v.type),
            type_ctx.to_string(target_type->get_type())));
    }

    if(v.type == const_::constant_type::f64)
    {
        if(target_type->get_type() == type_ctx.get_i8_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = static_cast<std::int64_t>(
                         std::get<double>(v.value))
                       & 0xff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i16_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = static_cast<std::int64_t>(
                         std::get<double>(v.value))
                       & 0xffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i32_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = static_cast<std::int64_t>(
                         std::get<double>(v.value))
                       & 0xffffffff};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_i64_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i64,
              .value = static_cast<std::int64_t>(
                std::get<double>(v.value))};    // NOLINT(readability-magic-numbers)
        }

        if(target_type->get_type() == type_ctx.get_f32_type())
        {
            return const_::const_info{
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f32,
              .value = static_cast<float>(std::get<double>(v.value))};
        }

        if(target_type->get_type() == type_ctx.get_f64_type())
        {
            return v;
        }

        throw ty::type_error(
          loc,
          std::format(
            "Invalid compile-time evaluated cast '{}' -> '{}'.",
            const_::to_string(v.type),
            type_ctx.to_string(target_type->get_type())));
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
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Reference '{}' has no symbol id.",
            name.s));
    }

    // check whether we're referencing a constant.
    return env.constant_registry.contains(symbol_id.value());
}

std::optional<const_::const_info> variable_reference_expression::evaluate(
  [[maybe_unused]] ty::context& type_ctx,
  const_::env& env) const
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Reference '{}' has no symbol id.",
            name.s));
    }

    return env.get_const_info(symbol_id.value());
}

/*
 * binary_expression.
 */

struct binary_operation_helper
{
    source_location loc;
    std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32;
    std::function<std::int64_t(std::int64_t, std::int64_t)> func_i64;
    std::function<float(float, float)> func_f32;
    std::function<double(double, double)> func_f64;

    binary_operation_helper(
      const binary_expression& expr,
      std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32,
      std::function<std::int32_t(std::int64_t, std::int64_t)> func_i64,
      std::function<float(float, float)> func_f32,
      std::function<double(double, double)> func_f64)
    : loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_i64{std::move(func_i64)}
    , func_f32{std::move(func_f32)}
    , func_f64{std::move(func_f64)}
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
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = func_i32(
                static_cast<std::int32_t>(std::get<std::int64_t>(lhs.value)),
                static_cast<std::int32_t>(std::get<std::int64_t>(rhs.value)))};
        }

        if(lhs.type == const_::constant_type::i64)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i64,
              .value = func_i64(
                std::get<std::int64_t>(lhs.value),
                std::get<std::int64_t>(rhs.value))};
        }

        if(lhs.type == const_::constant_type::f32)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f32,
              .value = func_f32(
                static_cast<float>(std::get<double>(lhs.value)),
                static_cast<float>(std::get<double>(rhs.value)))};
        }

        if(lhs.type == const_::constant_type::f64)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f64,
              .value = func_f64(
                std::get<double>(lhs.value),
                std::get<double>(rhs.value))};
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid type '{}' for binary operator evaluation.",
            const_::to_string(lhs.type),
            const_::to_string(rhs.type)));
    }
};

struct comparison_helper
{
    source_location loc;
    std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32;
    std::function<std::int32_t(std::int64_t, std::int64_t)> func_i64;
    std::function<std::int32_t(float, float)> func_f32;
    std::function<std::int32_t(double, double)> func_f64;

    comparison_helper(
      const binary_expression& expr,
      std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32,
      std::function<std::int32_t(std::int64_t, std::int64_t)> func_i64,
      std::function<std::int32_t(float, float)> func_f32,
      std::function<std::int32_t(double, double)> func_f64)
    : loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_i64{std::move(func_i64)}
    , func_f32{std::move(func_f32)}
    , func_f64{std::move(func_f64)}
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
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = func_i32(
                static_cast<std::int32_t>(std::get<std::int64_t>(lhs.value)),
                static_cast<std::int32_t>(std::get<std::int64_t>(rhs.value)))};
        }

        if(lhs.type == const_::constant_type::i64)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = func_i64(
                std::get<std::int64_t>(lhs.value),
                std::get<std::int64_t>(rhs.value))};
        }

        if(lhs.type == const_::constant_type::f32)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = func_f32(
                static_cast<float>(std::get<double>(lhs.value)),
                static_cast<float>(std::get<double>(rhs.value)))};
        }

        if(lhs.type == const_::constant_type::f64)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = func_f64(
                std::get<double>(lhs.value),
                std::get<double>(rhs.value))};
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
    // FIXME This should at least be static, but we supply *this for error messages.
    const std::unordered_map<std::string, binary_operation_helper> eval_map = {
      {"+", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a + b; }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             { return a + b; }, 
             [](float a, float b) -> float
             { return a + b; },
             [](double a, double b) -> double
             { return a + b; }}},
      {"-", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a - b; }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             { return a - b; }, 
             [](float a, float b) -> float
             { return a - b; },
             [](double a, double b) -> double
             { return a - b; }}},
      {"*", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a * b; }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             { return a * b; }, 
             [](float a, float b) -> float
             { return a * b; },
             [](double a, double b) -> double
             { return a * b; }}},
      {"/", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a / b; 
             }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
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
             },
             [](double a, double b) -> double
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a / b; 
             }}},
      {"%", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a % b; 
             }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a % b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '%'."); },
             [](double, double) -> double
             { throw cg::codegen_error("Invalid type 'f64' for binary operator '%'."); }}},
      {"&", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a & b; }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             { return a & b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '&'."); },
             [](double, double) -> double
             { throw cg::codegen_error("Invalid type 'f64' for binary operator '&'."); }}},
      {"^", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a ^ b; },
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             { return a ^ b; },
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '^'."); },
             [](double, double) -> double
             { throw cg::codegen_error("Invalid type 'f64' for binary operator '^'."); }}},
      {"|", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a | b; }, 
             [](std::int64_t a, std::int64_t b) -> std::int64_t
             { return a | b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '|'."); },
             [](double, double) -> double
             { throw cg::codegen_error("Invalid type 'f64' for binary operator '|'."); }}},
      {"&&", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a && b; }, 
              [](std::int64_t a, std::int64_t b) -> std::int64_t // FIXME type?
              { return a && b; }, 
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '&&'."); },
              [](double, double) -> double
              { throw cg::codegen_error("Invalid type 'f64' for binary operator '&&'."); }}},
      {"||", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a || b; }, 
              [](std::int64_t a, std::int64_t b) -> std::int64_t // FIXME type?
              { return a || b; }, 
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '||'."); },
              [](double, double) -> double
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '||'."); }}},
    };

    // FIXME This should at least be static, but we supply *this for error messages.
    const std::unordered_map<std::string, comparison_helper> comp_map = {
      {"<", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return static_cast<std::int32_t>(a < b); }, 
             [](std::int64_t a, std::int64_t b) -> std::int32_t
             { return static_cast<std::int32_t>(a < b); }, 
             [](float a, float b) -> std::int32_t
             { return static_cast<std::int32_t>(a < b); },
             [](double a, double b) -> std::int32_t
             { return static_cast<std::int32_t>(a < b); }}},
      {"<=", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a <= b); }, 
              [](std::int64_t a, std::int64_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a <= b); }, 
              [](float a, float b) -> std::int32_t
              { return static_cast<std::int32_t>(a <= b); },
              [](double a, double b) -> std::int32_t
              { return static_cast<std::int32_t>(a <= b); }}},
      {">", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return static_cast<std::int32_t>(a > b); }, 
             [](std::int64_t a, std::int64_t b) -> std::int32_t
             { return static_cast<std::int32_t>(a > b); }, 
             [](float a, float b) -> std::int32_t
             { return static_cast<std::int32_t>(a > b); },
             [](double a, double b) -> std::int32_t
             { return static_cast<std::int32_t>(a > b); }}},
      {">=", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a >= b); }, 
              [](std::int64_t a, std::int64_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a >= b); }, 
              [](float a, float b) -> std::int32_t
              { return static_cast<std::int32_t>(a >= b); },
              [](double a, double b) -> std::int32_t
              { return static_cast<std::int32_t>(a >= b); }}},
      {"==", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a == b); }, 
              [](std::int64_t a, std::int64_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a == b); }, 
              [](float a, float b) -> std::int32_t
              { return static_cast<std::int32_t>(a == b); },
              [](double a, double b) -> std::int32_t
              { return static_cast<std::int32_t>(a == b); }}},
      {"!=", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a != b); }, 
              [](std::int64_t a, std::int64_t b) -> std::int32_t
              { return static_cast<std::int32_t>(a != b); }, 
              [](float a, float b) -> std::int32_t
              { return static_cast<std::int32_t>(a != b); },
              [](double a, double b) -> std::int32_t
              { return static_cast<std::int32_t>(a != b); }}}
    };
    // clang-format on

    if(!env.is_expression_evaluated(*lhs) || !env.is_expression_evaluated(*rhs))
    {
        // visit the nodes to get the expression values.
        visit_nodes(
          [&type_ctx, &env](const ast::expression& node)
          {
              env.set_expression_const_eval(node, false);

              std::optional<const_::const_info> result = node.evaluate(type_ctx, env);
              if(result.has_value())
              {
                  env.set_expression_const_eval(node, true);
                  env.set_expression_value(node, result.value());
              }
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

    // special case: shift operators (l.h.s. and r.h.s. types don't have to match)
    if(op.s == "<<" || op.s == ">>")
    {
        const auto& lhs_value = env.get_expression_value(*lhs);
        const auto& rhs_value = env.get_expression_value(*rhs);

        if(lhs_value.type != const_::constant_type::i32
           && lhs_value.type != const_::constant_type::i64)
        {
            throw cg::codegen_error(
              lhs->get_location(),
              std::format(
                "Invalid type '{}' for shift operator '>>'. Expected 'i32' or 'i64'.",
                const_::to_string(lhs_value.type)));
        }

        if(rhs_value.type != const_::constant_type::i32)
        {
            throw cg::codegen_error(
              rhs->get_location(),
              std::format(
                "Expected r.h.s. of type 'i32', got '{}'.",
                const_::to_string(rhs_value.type)));
        }

        if(std::get<std::int64_t>(rhs_value.value) < 0)
        {
            throw cg::codegen_error("Negative shift counts are not allowed.");
        }

        std::uint32_t count_mask =
          (lhs_value.type == const_::constant_type::i32)
            ? 0x1f
            : 0x3f;

        std::uint64_t result_mask =
          (lhs_value.type == const_::constant_type::i32)
            ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(~0))
            : static_cast<std::uint64_t>(~0);

        std::int64_t result{0};
        std::uint32_t shift_count = static_cast<std::uint32_t>(std::get<std::int64_t>(rhs_value.value)) & count_mask;
        std::int64_t shift_value = std::get<std::int64_t>(lhs_value.value);

        if(op.s == "<<")
        {
            result = utils::numeric_cast<std::int64_t>(
              (shift_value << shift_count) & result_mask);
        }
        else
        {
            result = utils::numeric_cast<std::int64_t>(
              (shift_value >> shift_count) & result_mask);
        }

        return const_::const_info{
          .origin_module_id = sema::symbol_info::current_module_id,
          .type = lhs_value.type,
          .value = result};
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
    source_location loc;
    std::function<std::int32_t(std::int32_t)> func_i32;
    std::function<std::int64_t(std::int64_t)> func_i64;
    std::function<float(float)> func_f32;
    std::function<double(double)> func_f64;

    unary_operation_helper(
      const unary_expression& expr,
      std::function<std::int32_t(std::int32_t)> func_i32,
      std::function<std::int64_t(std::int64_t)> func_i64,
      std::function<float(float)> func_f32,
      std::function<double(double)> func_f64)
    : loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_i64{std::move(func_i64)}
    , func_f32{std::move(func_f32)}
    , func_f64{std::move(func_f64)}
    {
    }

    const_::const_info operator()(const const_::const_info& v) const
    {
        if(v.type == const_::constant_type::i32)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i32,
              .value = func_i32(static_cast<std::int32_t>(
                static_cast<std::int32_t>(std::get<std::int64_t>(v.value))))};
        }

        if(v.type == const_::constant_type::i64)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::i64,
              .value = func_i64(static_cast<std::int64_t>(
                std::get<std::int64_t>(v.value)))};
        }

        if(v.type == const_::constant_type::f32)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f32,
              .value = func_f32(
                static_cast<float>(std::get<double>(v.value)))};
        }

        if(v.type == const_::constant_type::f64)
        {
            return {
              .origin_module_id = sema::symbol_info::current_module_id,
              .type = const_::constant_type::f64,
              .value = func_f32(std::get<double>(v.value))};
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

    // clang-format off
    // FIXME This should at least be static, but we supply *this for error messages.
    const std::unordered_map<std::string, unary_operation_helper> eval_map = {
      {"+", {
              *this,
              [](std::int32_t a) -> std::int32_t
              { return a; },
              [](std::int64_t a) -> std::int64_t
              { return a; },
              [](float a) -> float
              { return a; },
              [](double a) -> double
              { return a; },
            }},
      {"-", {
              *this,
              [](std::int32_t a) -> std::int32_t
              { return -a; },
              [](std::int64_t a) -> std::int64_t
              { return -a; },
              [](float a) -> float
              { return -a; },
              [](double a) -> double
              { return -a; },
            }},
      {"!", {
              *this,
              [](std::int32_t a) -> std::int32_t
              {
                  return a == 0;    // matches the generated opcodes.
              },
              [](std::int64_t a) -> std::int64_t
              {
                  return a == 0;    // matches the generated opcodes.
              },
              [](float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for unary operator '!'."); },
              [](double) -> double
              { throw cg::codegen_error("Invalid type 'f64' for unary operator '!'."); },
            }},
      {"~", {
              *this,
              [](std::int32_t a) -> std::int32_t
              {
                  return static_cast<std::int32_t>(~0) ^ a;    // matches the generated opcodes.
              },
              [](std::int64_t a) -> std::int64_t
              {
                  return static_cast<std::int64_t>(~0) ^ a;    // matches the generated opcodes.
              },
              [](float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for unary operator '!'.");
              },
              [](double) -> double
              {
                  throw cg::codegen_error("Invalid type 'f64' for unary operator '!'.");
              },
            }}};
    // clang-format on

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
              env.set_expression_const_eval(node, false);

              std::optional<const_::const_info> result = node.evaluate(type_ctx, env);
              if(result.has_value())
              {
                  env.set_expression_const_eval(node, true);
                  env.set_expression_value(node, result.value());
              }
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
