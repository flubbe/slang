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
#include "compiler/codegen.h"
#include "compiler/typing.h"
#include "ast.h"
#include "utils.h"

namespace slang::ast
{

/*
 * literal_expression.
 */

std::unique_ptr<cg::value> literal_expression::evaluate([[maybe_unused]] cg::context& ctx) const
{
    if(!tok.value.has_value())
    {
        throw cg::codegen_error(loc, "Literal expression has no value.");
    }

    if(tok.type == token_type::int_literal)
    {
        return std::make_unique<cg::constant_int>(std::get<std::int32_t>(tok.value.value()));
    }

    if(tok.type == token_type::fp_literal)
    {
        return std::make_unique<cg::constant_float>(std::get<float>(tok.value.value()));
    }

    if(tok.type == token_type::str_literal)
    {
        return std::make_unique<cg::constant_str>(std::get<std::string>(tok.value.value()));
    }

    return {nullptr};
}

/*
 * namespace_access_expression.
 */

std::unique_ptr<cg::value> namespace_access_expression::evaluate(cg::context& ctx) const
{
    return expr->evaluate(ctx);
}

/*
 * variable_reference_expression.
 */

bool variable_reference_expression::is_const_eval(cg::context& ctx) const
{
    // check whether we're referencing a constant.
    return ctx.get_constant(name.s, get_namespace_path()).has_value();
}

std::unique_ptr<cg::value> variable_reference_expression::evaluate(cg::context& ctx) const
{
    std::optional<cg::constant_table_entry> entry = ctx.get_constant(name.s, get_namespace_path());
    if(!entry.has_value())
    {
        return {};
    }

    if(entry->type == module_::constant_type::i32)
    {
        return std::make_unique<cg::constant_int>(std::get<std::int32_t>(entry->data));
    }

    if(entry->type == module_::constant_type::f32)
    {
        return std::make_unique<cg::constant_float>(std::get<float>(entry->data));
    }

    if(entry->type == module_::constant_type::str)
    {
        return std::make_unique<cg::constant_str>(std::get<std::string>(entry->data));
    }

    return {};
}

/*
 * binary_expression.
 */

struct binary_operation_helper
{
    token_location loc;
    std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32;
    std::function<float(float, float)> func_f32;

    binary_operation_helper(
      const binary_expression& expr,
      std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32,
      std::function<float(float, float)> func_f32)
    : loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_f32{std::move(func_f32)}
    {
    }

    std::unique_ptr<cg::value>
      operator()(
        const cg::value& lhs,
        const cg::value& rhs) const
    {
        if(lhs.to_string() != rhs.to_string())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Operand types don't match for binary operator evaluation: '{}' != '{}'.",
                lhs.to_string(), rhs.to_string()));
        }

        if(lhs.to_string() == "i32")
        {
            const auto* lhs_i32 = static_cast<const cg::constant_int*>(&lhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const auto* rhs_i32 = static_cast<const cg::constant_int*>(&rhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            return std::make_unique<cg::constant_int>(func_i32(lhs_i32->get_int(), rhs_i32->get_int()));
        }

        if(lhs.to_string() == "f32")
        {
            const auto* lhs_f32 = static_cast<const cg::constant_float*>(&lhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const auto* rhs_f32 = static_cast<const cg::constant_float*>(&rhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            return std::make_unique<cg::constant_float>(func_f32(lhs_f32->get_float(), rhs_f32->get_float()));
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid type '{}' for binary operator evaluation.",
            lhs.get_type().to_string()));
    }
};

struct binary_comparison_helper
{
    token_location loc;
    std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32;
    std::function<std::int32_t(float, float)> func_f32;

    binary_comparison_helper(
      const binary_expression& expr,
      std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32,
      std::function<std::int32_t(float, float)> func_f32)
    : loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_f32{std::move(func_f32)}
    {
    }

    std::unique_ptr<cg::value>
      operator()(
        const cg::value& lhs,
        const cg::value& rhs) const
    {
        if(lhs.to_string() != rhs.to_string())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Operand types don't match for binary operator evaluation: '{}' != '{}'.",
                lhs.to_string(), rhs.to_string()));
        }

        if(lhs.to_string() == "i32")
        {
            const auto* lhs_i32 = static_cast<const cg::constant_int*>(&lhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const auto* rhs_i32 = static_cast<const cg::constant_int*>(&rhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            return std::make_unique<cg::constant_int>(func_i32(lhs_i32->get_int(), rhs_i32->get_int()));
        }

        if(lhs.to_string() == "f32")
        {
            const auto* lhs_f32 = static_cast<const cg::constant_float*>(&lhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            const auto* rhs_f32 = static_cast<const cg::constant_float*>(&rhs);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            return std::make_unique<cg::constant_int>(func_f32(lhs_f32->get_float(), rhs_f32->get_float()));
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Invalid type '{}' for comparison evaluation.",
            lhs.get_type().to_string()));
    }
};

bool binary_expression::is_const_eval(cg::context& ctx) const
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

    if(!ctx.has_expression_constant(*lhs) || !ctx.has_expression_constant(*rhs))
    {
        // visit the nodes to get whether this expression is a compile time constant.
        visit_nodes(
          [&ctx](const ast::expression& node)
          {
              ctx.set_expression_constant(node, node.is_const_eval(ctx));
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    return ctx.get_expression_constant(*lhs) && ctx.get_expression_constant(*rhs);
}

std::unique_ptr<cg::value> binary_expression::evaluate(cg::context& ctx) const
{
    if(!is_const_eval(ctx))
    {
        return {};
    }

    // clang-format off
    static const std::unordered_map<std::string, binary_operation_helper> eval_map = {
      {"+", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a + b; }, 
             [](float a, float b) -> float
             { return a + b; }}},
      {"-", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a - b; }, 
             [](float a, float b) -> float
             { return a - b; }}},
      {"*", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a * b; }, 
             [](float a, float b) -> float
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
             [](float a, float b) -> float
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
                 return a % b; }, [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '%'."); }}},
      {"<<", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a << (b & 0x1f); }, // NOLINT(readability-magic-numbers)
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '<<'."); }}},
      {">>", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a >> (b & 0x1f); }, // NOLINT(readability-magic-numbers)
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '>>'."); }}},
      {"&", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a & b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '&'."); }}},
      {"^", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a ^ b; },
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '^'."); }}},
      {"|", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a | b; }, 
             [](float, float) -> float
             { throw cg::codegen_error("Invalid type 'f32' for binary operator '|'."); }}},
      {"&&", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a && b; }, 
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '&&'."); }}},
      {"||", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a || b; }, 
              [](float, float) -> float
              { throw cg::codegen_error("Invalid type 'f32' for binary operator '||'."); }}},
    };

    static const std::unordered_map<std::string, binary_comparison_helper> comp_map = {
      {"<", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a < b; }, 
             [](float a, float b) -> std::int32_t
             { return a < b; }}},
      {"<=", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a <= b; }, 
              [](float a, float b) -> std::int32_t
              { return a <= b; }}},
      {">", {*this, 
             [](std::int32_t a, std::int32_t b) -> std::int32_t
             { return a > b; }, 
             [](float a, float b) -> std::int32_t
             { return a > b; }}},
      {">=", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a >= b; }, 
              [](float a, float b) -> std::int32_t
              { return a >= b; }}},
      {"==", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a == b; }, 
              [](float a, float b) -> std::int32_t
              { return a == b; }}},
      {"!=", {*this, 
              [](std::int32_t a, std::int32_t b) -> std::int32_t
              { return a != b; }, 
              [](float a, float b) -> std::int32_t
              { return a != b; }}}
    };
    // clang-format on

    if(!ctx.has_expression_value(*lhs) || !ctx.has_expression_value(*rhs))
    {
        // visit the nodes to get the expression values.
        visit_nodes(
          [&ctx](const ast::expression& node)
          {
              ctx.set_expression_value(node, node.evaluate(ctx));
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );

        // check that we calculated the required values.
        if(!ctx.has_expression_value(*lhs) || !ctx.has_expression_value(*rhs))
        {
            return {nullptr};
        }
    }

    auto eval_it = eval_map.find(op.s);
    if(eval_it != eval_map.end())
    {
        return eval_it->second(
          ctx.get_expression_value(*lhs),
          ctx.get_expression_value(*rhs));
    }

    auto comp_it = comp_map.find(op.s);
    if(comp_it != comp_map.end())
    {
        return comp_it->second(
          ctx.get_expression_value(*lhs),
          ctx.get_expression_value(*rhs));
    }

    return {nullptr};
}

/*
 * unary_expression.
 */

struct unary_operation_helper
{
    token_location loc;
    std::function<std::int32_t(std::int32_t)> func_i32;
    std::function<float(float)> func_f32;

    unary_operation_helper(
      const unary_expression& expr,
      std::function<std::int32_t(std::int32_t)> func_i32,
      std::function<float(float)> func_f32)
    : loc{expr.get_location()}
    , func_i32{std::move(func_i32)}
    , func_f32{std::move(func_f32)}
    {
    }

    std::unique_ptr<cg::value>
      operator()(const cg::value& v) const
    {
        if(v.to_string() == "i32")
        {
            const auto* v_i32 = static_cast<const cg::constant_int*>(&v);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            return std::make_unique<cg::constant_int>(func_i32(v_i32->get_int()));
        }

        if(v.to_string() == "f32")
        {
            const auto* v_f32 = static_cast<const cg::constant_float*>(&v);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            return std::make_unique<cg::constant_float>(func_f32(v_f32->get_float()));
        }

        throw cg::codegen_error(
          loc,
          std::format("Invalid type '{}' for unary operator evaluation.",
                      v.get_type().to_string()));
    }
};

bool unary_expression::is_const_eval(cg::context& ctx) const
{
    if(op.s != "+" && op.s != "-" && op.s != "!" && op.s != "~")
    {
        return false;
    }

    if(!ctx.has_expression_constant(*operand))
    {
        // visit the nodes to get whether this expression is a compile time constant.
        visit_nodes(
          [&ctx](const ast::expression& node)
          {
              ctx.set_expression_constant(node, node.is_const_eval(ctx));
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    return operand->is_const_eval(ctx);
}

std::unique_ptr<cg::value> unary_expression::evaluate(cg::context& ctx) const
{
    if(!is_const_eval(ctx))
    {
        return {};
    }

    static const std::unordered_map<std::string, unary_operation_helper> eval_map = {
      {"+", {
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
        return {nullptr};
    }

    if(!ctx.has_expression_value(*operand))
    {
        // visit the nodes to get the expression values.
        visit_nodes(
          [&ctx](const ast::expression& node)
          {
              ctx.set_expression_value(node, node.evaluate(ctx));
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );

        // check that we calculated the required value.
        if(!ctx.has_expression_value(*operand))
        {
            return {nullptr};
        }
    }

    return it->second(ctx.get_expression_value(*operand));
}

}    // namespace slang::ast
