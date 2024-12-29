/**
 * slang - a simple scripting language.
 *
 * constant expression evaluation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <unordered_map>
#include <set>
#include <tuple>

#include "ast.h"
#include "codegen.h"
#include "module.h"
#include "typing.h"
#include "utils.h"

namespace slang::ast
{

/*
 * literal_expression.
 */

std::unique_ptr<cg::value> literal_expression::evaluate(cg::context&) const
{
    if(tok.type == token_type::int_literal)
    {
        return std::make_unique<cg::constant_int>(std::get<std::int32_t>(*tok.value));
    }
    else if(tok.type == token_type::fp_literal)
    {
        return std::make_unique<cg::constant_float>(std::get<float>(*tok.value));
    }
    else if(tok.type == token_type::str_literal)
    {
        return std::make_unique<cg::constant_str>(std::get<std::string>(*tok.value));
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
    else if(entry->type == module_::constant_type::f32)
    {
        return std::make_unique<cg::constant_float>(std::get<float>(entry->data));
    }
    else if(entry->type == module_::constant_type::str)
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
    std::function<std::int32_t(std::int32_t, std::int32_t)> func_i32;
    std::function<float(float, float)> func_f32;

    std::unique_ptr<cg::value> operator()(
      const std::unique_ptr<cg::value>& lhs,
      const std::unique_ptr<cg::value>& rhs) const
    {
        if(!lhs || !rhs)
        {
            throw cg::codegen_error("Null argument passed to binary operator evaluation.");
        }

        if(lhs->to_string() != rhs->to_string())
        {
            throw cg::codegen_error(
              fmt::format(
                "Operand types don't match for binary operator evaluation: '{}' != '{}'.",
                lhs->to_string(), rhs->to_string()));
        }

        if(lhs->to_string() == "i32")
        {
            const cg::constant_int* lhs_i32 = static_cast<const cg::constant_int*>(lhs.get());
            const cg::constant_int* rhs_i32 = static_cast<const cg::constant_int*>(rhs.get());
            return std::make_unique<cg::constant_int>(func_i32(lhs_i32->get_int(), rhs_i32->get_int()));
        }
        else if(lhs->to_string() == "f32")
        {
            const cg::constant_float* lhs_f32 = static_cast<const cg::constant_float*>(lhs.get());
            const cg::constant_float* rhs_f32 = static_cast<const cg::constant_float*>(rhs.get());
            return std::make_unique<cg::constant_float>(func_f32(lhs_f32->get_float(), rhs_f32->get_float()));
        }

        throw cg::codegen_error(
          fmt::format(
            "Invalid type '{}' for unary operator evaluation.",
            lhs->get_type().to_string()));
    }
};

bool binary_expression::is_const_eval(cg::context& ctx) const
{
    // operators that support constant expression evaluation.
    const std::array<std::string, 18> bin_ops = {
      "+", "-", "*", "/", "%",
      "<<", ">>",
      "<", "<=", ">", ">=", "==", "!=",
      "&", "^", "|",
      "&&", "||"};

    return std::find(bin_ops.begin(), bin_ops.end(), op.s) != bin_ops.end()
           && lhs->is_const_eval(ctx) && rhs->is_const_eval(ctx);
}

std::unique_ptr<cg::value> binary_expression::evaluate(cg::context& ctx) const
{
    if(!is_const_eval(ctx))
    {
        return {};
    }

    std::unordered_map<std::string, binary_operation_helper> eval_map = {
      {"+", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a + b;
             },
             [](float a, float b) -> float
             {
                 return a + b;
             }}},
      {"-", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a - b;
             },
             [](float a, float b) -> float
             {
                 return a - b;
             }}},
      {"*", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a * b;
             },
             [](float a, float b) -> float
             {
                 return a * b;
             }}},
      {"/", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
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
      {"%", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 if(b == 0)
                 {
                     throw cg::codegen_error("Division by zero detected while evaluating constant.");
                 }
                 return a % b;
             },
             [](float, float) -> float
             {
                 throw cg::codegen_error("Invalid type 'f32' for binary operator '%'.");
             }}},
      {"<<", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a << (b & 0x1f);
              },
              [](float, float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for binary operator '<<'.");
              }}},
      {">>", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a >> (b & 0x1f);
              },
              [](float, float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for binary operator '>>'.");
              }}},
      {"<", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a < b;
             },
             [](float a, float b) -> float
             {
                 return a < b;
             }}},
      {"<=", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a <= b;
              },
              [](float a, float b) -> float
              {
                  return a <= b;
              }}},
      {">", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a > b;
             },
             [](float a, float b) -> float
             {
                 return a > b;
             }}},
      {">=", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a >= b;
              },
              [](float a, float b) -> float
              {
                  return a >= b;
              }}},
      {"==", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a == b;
              },
              [](float a, float b) -> float
              {
                  return a == b;
              }}},
      {"!=", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a != b;
              },
              [](float a, float b) -> float
              {
                  return a != b;
              }}},
      {"&", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a & b;
             },
             [](float, float) -> float
             {
                 throw cg::codegen_error("Invalid type 'f32' for binary operator '&'.");
             }}},
      {"^", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a ^ b;
             },
             [](float, float) -> float
             {
                 throw cg::codegen_error("Invalid type 'f32' for binary operator '^'.");
             }}},
      {"|", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
             {
                 return a | b;
             },
             [](float, float) -> float
             {
                 throw cg::codegen_error("Invalid type 'f32' for binary operator '|'.");
             }}},
      {"&&", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a && b;
              },
              [](float, float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for binary operator '&&'.");
              }}},
      {"||", {[](std::uint32_t a, std::uint32_t b) -> std::uint32_t
              {
                  return a || b;
              },
              [](float, float) -> float
              {
                  throw cg::codegen_error("Invalid type 'f32' for binary operator '||'.");
              }}},
    };

    auto it = eval_map.find(op.s);
    if(it == eval_map.end())
    {
        return {nullptr};
    }

    return it->second(lhs->evaluate(ctx), rhs->evaluate(ctx));
}

/*
 * unary_expression.
 */

struct unary_operation_helper
{
    std::function<std::int32_t(std::int32_t)> func_i32;
    std::function<float(float)> func_f32;

    std::unique_ptr<cg::value> operator()(const std::unique_ptr<cg::value>& v) const
    {
        if(!v)
        {
            throw cg::codegen_error("Null argument passed to unary operator evaluation.");
        }

        if(v->to_string() == "i32")
        {
            const cg::constant_int* v_i32 = static_cast<const cg::constant_int*>(v.get());
            return std::make_unique<cg::constant_int>(func_i32(v_i32->get_int()));
        }
        else if(v->to_string() == "f32")
        {
            const cg::constant_float* v_f32 = static_cast<const cg::constant_float*>(v.get());
            return std::make_unique<cg::constant_float>(func_f32(v_f32->get_float()));
        }

        throw cg::codegen_error(fmt::format("Invalid type '{}' for unary operator evaluation.", v->get_type().to_string()));
    }
};

bool unary_expression::is_const_eval(cg::context& ctx) const
{
    if(op.s != "+" && op.s != "-" && op.s != "!" && op.s != "~")
    {
        return false;
    }

    return operand->is_const_eval(ctx);
}

std::unique_ptr<cg::value> unary_expression::evaluate(cg::context& ctx) const
{
    if(!is_const_eval(ctx))
    {
        return {};
    }

    std::unordered_map<std::string, unary_operation_helper> eval_map = {
      {"+", {
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

    return it->second(operand->evaluate(ctx));
}

}    // namespace slang::ast
