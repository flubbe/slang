/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cmath>
#include <print>
#include <ranges>
#include <set>
#include <stack>
#include <utility>

#include <gsl/gsl>

#include "compiler/codegen/codegen.h"
#include "compiler/attribute.h"
#include "compiler/macro.h"
#include "compiler/typing.h"
#include "shared/module.h"
#include "shared/type_utils.h"
#include "ast.h"
#include "node_registry.h"
#include "utils.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace slang::ast
{

/*
 * expression.
 */

std::unique_ptr<expression> expression::clone() const
{
    return std::make_unique<expression>(*this);
}

void expression::serialize(archive& ar)
{
    ar & loc;
    ar & namespace_stack;
    ar & expr_type;
}

call_expression* expression::as_call_expression()
{
    throw std::runtime_error("Expression is not a call expression.");
}

macro_expression* expression::as_macro_expression()
{
    throw std::runtime_error("Expression is not a macro.");
}

macro_branch* expression::as_macro_branch()
{
    throw std::runtime_error("Expression is not a macro branch.");
}

const macro_branch* expression::as_macro_branch() const
{
    throw std::runtime_error("Expression is not a macro branch.");
}

macro_expression_list* expression::as_macro_expression_list()
{
    throw std::runtime_error("Expression is not a macro expression list.");
}

const macro_expression_list* expression::as_macro_expression_list() const
{
    throw std::runtime_error("Expression is not a macro expression list.");
}

variable_declaration_expression* expression::as_variable_declaration()
{
    throw std::runtime_error("Expression is not a variable declaration.");
}

constant_declaration_expression* expression::as_constant_declaration()
{
    throw std::runtime_error("Expression is not a constant declaration.");
}

const constant_declaration_expression* expression::as_constant_declaration() const
{
    throw std::runtime_error("Expression is not a constant declaration.");
}

variable_reference_expression* expression::as_variable_reference()
{
    throw std::runtime_error("Expression is not a variable reference.");
}

macro_invocation* expression::as_macro_invocation()
{
    throw std::runtime_error("Expression is not a macro invocation.");
}

literal_expression* expression::as_literal()
{
    throw std::runtime_error("Expression is not a literal.");
}

access_expression* expression::as_access_expression()
{
    throw std::runtime_error("Expression is not an access expression.");
}

const access_expression* expression::as_access_expression() const
{
    throw std::runtime_error("Expression is not an access expression.");
}

named_expression* expression::as_named_expression()
{
    throw std::runtime_error("Expression is not a named expression.");
}

const named_expression* expression::as_named_expression() const
{
    throw std::runtime_error("Expression is not a named expression.");
}

std::unique_ptr<cg::rvalue> expression::try_emit_const_eval_result(
  cg::context& ctx) const
{
    // Evaluate constant subexpressions.
    auto const_eval_it = ctx.get_const_env().const_eval_expr_values.find(this);
    if(ctx.has_flag(cg::codegen_flags::enable_const_eval)
       && const_eval_it != ctx.get_const_env().const_eval_expr_values.cend())
    {
        auto info = const_eval_it->second;

        if(info.type == const_::constant_type::i32)
        {
            const auto back_end_type = cg::type_kind::i32;

            ctx.generate_const(
              {back_end_type},
              std::get<std::int64_t>(info.value));

            return std::make_unique<cg::rvalue>(back_end_type);
        }

        if(info.type == const_::constant_type::i64)
        {
            const auto back_end_type = cg::type_kind::i64;

            ctx.generate_const(
              {back_end_type},
              std::get<std::int64_t>(info.value));

            return std::make_unique<cg::rvalue>(back_end_type);
        }

        if(info.type == const_::constant_type::f32)
        {
            const auto back_end_type = cg::type_kind::f32;

            ctx.generate_const(
              {back_end_type},
              std::get<double>(info.value));

            return std::make_unique<cg::rvalue>(back_end_type);
        }

        if(info.type == const_::constant_type::f64)
        {
            const auto back_end_type = cg::type_kind::f64;

            ctx.generate_const(
              {back_end_type},
              std::get<double>(info.value));

            return std::make_unique<cg::rvalue>(back_end_type);
        }

        std::println(
          "{}: Warning: Attempted constant expression computation failed.",
          ::slang::to_string(loc));
    }

    return {};
}

std::optional<const_::const_info> expression::evaluate(
  [[maybe_unused]] ty::context& ctx,
  [[maybe_unused]] const_::env& env) const
{
    return std::nullopt;
}

void expression::generate_code(
  [[maybe_unused]] cg::context& ctx) const
{
    // no-op.

    // TODO move to statement base class.
}

std::unique_ptr<cg::lvalue> expression::emit_lvalue(
  [[maybe_unused]] cg::context& ctx) const
{
    throw cg::codegen_error(
      loc,
      "Expression cannot be used as an l-value.");
}

std::unique_ptr<cg::rvalue> expression::emit_rvalue(
  [[maybe_unused]] cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    throw cg::codegen_error(
      loc,
      "Expression cannot be used as an r-value.");
}

void expression::collect_attributes(sema::env& env) const
{
    visit_nodes(
      [&env](const expression& expr) -> void
      {
          // not a filter, since the filter also removes child nodes.
          if(expr.get_id() == node_identifier::directive_expression)
          {
              const auto* dir_expr = static_cast<const directive_expression*>(&expr);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
              if(!dir_expr->get_target()->symbol_id.has_value())
              {
                  // The target is not a symbol, so we cannot attach an attribute.
                  // For example: Disabling constant evaluation on an expression.
                  return;
              }

              const std::string& name = dir_expr->get_name();

              auto kind = attribs::get_attribute_kind(name);
              if(!kind.has_value())
              {
                  throw attribs::attribute_error(
                    std::format(
                      "{}: Unknown attribute '{}'.",
                      ::slang::to_string(expr.get_location()),
                      name));
              }

              // TODO Formalize attribute specification and allow other argument types.

              env.attach_attribute(
                dir_expr->get_target()->symbol_id.value(),
                sema::attribute_info{
                  .kind = kind.value(),
                  .loc = expr.get_location(),
                  .payload = dir_expr->get_args()
                             | std::views::transform(
                               [](const std::pair<token, token>& p) -> std::pair<std::string, std::string>
                               {
                                   return std::make_pair(p.first.s, p.second.s);
                               })
                             | std::ranges::to<std::vector>()});
          }
      },
      false, /* don't visit this node */
      false  /* pre-order traversal */
    );
}

void expression::collect_macros(
  sema::env& sema_env,
  macro::env& macro_env) const
{
    visit_nodes(
      [&sema_env, &macro_env](const expression& expr) -> void
      {
          // not a filter, since the filter also removes child nodes.
          if(expr.get_id() == node_identifier::macro_expression)
          {
              // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
              static_cast<const macro_expression*>(&expr)->collect_macro(sema_env, macro_env);
          }
      },
      false, /* don't visit this node */
      false  /* pre-order traversal */
    );
}

void expression::declare_functions(ty::context& ctx, sema::env& env)
{
    visit_nodes(
      [&ctx, &env](expression& expr) -> void
      {
          // not a filter, since the filter also removes child nodes.
          if(expr.get_id() == node_identifier::function_expression)
          {
              static_cast<function_expression*>(&expr)->declare_function(ctx, env);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          }
      },
      false, /* don't visit this node */
      false  /* pre-order traversal */
    );
}

void expression::declare_types(ty::context& ctx, sema::env& env)
{
    visit_nodes(
      [&ctx, &env](expression& expr) -> void
      {
          // not a filter, since the filter also removes child nodes.
          if(expr.get_id() == node_identifier::struct_definition_expression)
          {
              static_cast<struct_definition_expression*>(&expr)->declare_type(ctx, env);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          }
      },
      false, /* don't visit this node */
      false  /* pre-order traversal */
    );
}

void expression::define_types(ty::context& ctx) const
{
    visit_nodes(
      [&ctx](const expression& expr) -> void
      {
          // not a filter, since the filter also removes child nodes.
          if(expr.get_id() == node_identifier::struct_definition_expression)
          {
              static_cast<const struct_definition_expression*>(&expr)->define_type(ctx);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          }
      },
      false, /* don't visit this node */
      false  /* pre-order traversal */
    );
}

void expression::bind_constant_declarations(
  [[maybe_unused]] sema::env& sema_env,
  const_::env& const_env) const
{
    visit_nodes(
      [&const_env](const expression& expr) -> void
      {
          if(!expr.is_constant_declaration())
          {
              return;
          }

          std::optional<sema::symbol_id> symbol_id = expr.as_constant_declaration()->get_symbol_id();
          if(!symbol_id.has_value())
          {
              throw std::runtime_error(
                std::format(
                  "{}: Constant has no symbol id.",
                  ::slang::to_string(expr.get_location())));
          }

          const_env.register_constant(symbol_id.value());
      },
      false, /* don't visit this node */
      false  /* pre-order traversal */
    );
}

void expression::evaluate_constant_expressions(
  ty::context& ctx,
  const_::env& env)
{
    visit_nodes(
      [&ctx, &env](const expression& expr) -> void
      {
          // Evaluate constant expression.
          if(!env.is_expression_evaluated(expr))
          {
              env.set_expression_const_eval(expr, false);
              if(expr.is_const_eval(env))
              {
                  std::optional<const_::const_info> result = expr.evaluate(ctx, env);
                  if(result.has_value())
                  {
                      env.set_expression_const_eval(expr, true);
                      env.set_expression_value(expr, std::move(result.value()));
                  }
              }
          }

          // Associate values to declared constants.
          if(expr.is_constant_declaration())
          {
              const auto* const_decl = expr.as_constant_declaration();

              std::optional<sema::symbol_id> symbol_id = const_decl->get_symbol_id();
              if(symbol_id.has_value()
                 && env.constant_registry.contains(symbol_id.value()))
              {
                  if(env.const_info_map.contains(symbol_id.value()))
                  {
                      throw std::runtime_error(
                        std::format(
                          "{}: Symbol id already contained in constant map.",
                          ::slang::to_string(expr.get_location())));
                  }

                  const auto* const_expr = const_decl->get_expr();
                  if(!const_expr)
                  {
                      throw std::runtime_error(
                        std::format(
                          "{}: Constant expression has invalid handle.",
                          ::slang::to_string(expr.get_location())));
                  }

                  env.set_const_info(
                    symbol_id.value(),
                    env.get_expression_value(*const_expr));
              }
          }
      },
      false, /* don't visit this node */
      true,  /* post-order traversal */
      [](const slang::ast::expression& expr) -> bool
      {
          // A macro branch has no type information, so we skip it.
          return !expr.is_macro_branch();
      });
}

void expression::insert_implicit_casts(
  ty::context& ctx,
  sema::env& env)
{
    visit_nodes(
      [&ctx, &env](expression& expr) -> void
      {
          if(expr.get_id() == node_identifier::assignment_expression)
          {
              static_cast<assignment_expression*>(&expr)->insert_implicit_casts(ctx, env);
          }
          else if(expr.get_id() == node_identifier::variable_declaration_expression)
          {
              static_cast<variable_declaration_expression*>(&expr)->insert_implicit_casts(ctx, env);
          }
      },
      false,
      false);
}

std::string expression::to_string() const
{
    throw std::runtime_error("Expression has no string conversion");
}

/**
 * Templated visit helper. Implements DFS to walk the AST.
 * The visitor function is called for each node in the AST.
 * The nodes are visited in pre-order or post-order.
 *
 * @param expr The expression to visit.
 * @param visitor The visitor function.
 * @param visit_self Whether to visit the expression itself.
 * @param post_order Whether to visit the nodes in post-order.
 * @param filter An optional filter that returns `true` if a node
 *               should be traversed. Defaults to traversing all nodes.
 * @tparam T The expression type. Must be a subclass of `expression`.
 */
template<typename T>
    requires(std::is_same_v<std::decay_t<T>, expression>
             || std::is_base_of_v<expression, std::decay_t<T>>)
void visit_nodes(
  T& expr,
  std::function<void(T&)>& visitor,
  bool visit_self,
  bool post_order,
  const std::function<bool(std::add_const_t<T>&)>& filter = nullptr)
{
    // use DFS to topologically sort the AST.
    std::stack<T*> stack;
    stack.push(&expr);

    std::deque<T*> sorted_ast;

    while(!stack.empty())
    {
        T* current = stack.top();
        stack.pop();

        if(current == nullptr)
        {
            throw std::runtime_error("Null expression in AST.");
        }

        if(!filter || filter(*current))
        {
            sorted_ast.emplace_back(current);

            for(auto* child: current->get_children())
            {
                stack.push(child);
            }
        }
    }

    if(!visit_self
       && !sorted_ast.empty()
       && sorted_ast.front() == &expr)    // a filter could remove the starting node, so the check is necessary
    {
        sorted_ast.pop_front();
    }

    if(post_order)
    {
        std::ranges::for_each(
          sorted_ast | std::views::reverse,
          [&](T* ptr)
          { visitor(*ptr); });
    }
    else
    {
        std::ranges::for_each(
          sorted_ast,
          [&](T* ptr)
          { visitor(*ptr); });
    }
}

void expression::visit_nodes(
  std::function<void(expression&)> visitor,
  bool visit_self,
  bool post_order,
  const std::function<bool(const expression&)>& filter)
{
    slang::ast::visit_nodes(*this, visitor, visit_self, post_order, filter);
}

void expression::visit_nodes(
  std::function<void(const expression&)> visitor,
  bool visit_self,
  bool post_order,
  const std::function<bool(const expression&)>& filter) const
{
    slang::ast::visit_nodes(*this, visitor, visit_self, post_order, filter);
}

/*
 * named_expression.
 */

std::unique_ptr<expression> named_expression::clone() const
{
    return std::make_unique<named_expression>(*this);
}

void named_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & name;
}

std::string named_expression::get_qualified_name() const
{
    std::optional<std::string> path = get_namespace_path();
    if(!path.has_value())
    {
        return name.s;
    }
    return name::qualified_name(
      path.value(),
      name.s);
}

/*
 * literal_expression.
 */

std::unique_ptr<expression> literal_expression::clone() const
{
    return std::make_unique<literal_expression>(*this);
}

void literal_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & tok;
}

std::unique_ptr<cg::rvalue> literal_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    if(!tok.value.has_value())
    {
        throw cg::codegen_error(loc, "Empty literal.");
    }

    if(!expr_type.has_value())
    {
        throw cg::codegen_error(loc, "Literal expression has no type.");
    }

    auto lowered_type = ctx.lower(expr_type.value());

    auto [back_end_type, value] =
      [this, &ctx, lowered_type]() -> std::pair<
                                     cg::type_kind,
                                     std::variant<
                                       std::int64_t,
                                       double,
                                       const_::constant_id>>
    {
        if(lowered_type.get_type_kind() == cg::type_kind::i8
           || lowered_type.get_type_kind() == cg::type_kind::i16
           || lowered_type.get_type_kind() == cg::type_kind::i32)
        {
            return std::make_pair(
              cg::type_kind::i32,
              std::get<std::int64_t>(tok.value.value()));
        }

        if(lowered_type.get_type_kind() == cg::type_kind::i64)
        {
            return std::make_pair(
              cg::type_kind::i64,
              std::get<std::int64_t>(tok.value.value()));
        }

        if(lowered_type.get_type_kind() == cg::type_kind::f32)
        {
            return std::make_pair(
              cg::type_kind::f32,
              std::get<double>(tok.value.value()));
        }

        if(lowered_type.get_type_kind() == cg::type_kind::f64)
        {
            return std::make_pair(
              cg::type_kind::f64,
              std::get<double>(tok.value.value()));
        }

        if(lowered_type.get_type_kind() == cg::type_kind::str)
        {
            return std::make_pair(
              cg::type_kind::str,
              ctx.intern(
                std::get<std::string>(tok.value.value())));
        }

        throw cg::codegen_error(
          loc,
          std::format(
            "Unable to generate code for literal of unknown type kind '{}' (type id {}) during code generation.",
            cg::to_string(lowered_type.get_type_kind()),
            lowered_type.get_type_id().value_or(-1)));
    }();

    auto literal_type = cg::type{
      expr_type.value(),
      back_end_type};

    ctx.generate_const(literal_type, value);
    return std::make_unique<cg::rvalue>(literal_type);
}

std::optional<ty::type_id> literal_expression::type_check(
  ty::context& ctx,
  [[maybe_unused]] sema::env& env)
{
    if(!tok.value)
    {
        throw ty::type_error(loc, "Empty literal.");
    }

    // default to i32 for integer literals and f64 for floating-point literals.

    if(tok.type == token_type::int_literal)
    {
        if(tok.suffix.has_value())
        {
            if(tok.suffix.value().ty != suffix_type::integer)
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "Invalid suffix '{}' for integer literal.",
                    slang::to_string(tok.suffix.value().ty)));
            }

            switch(tok.suffix.value().width)
            {
            case 8:    // NOLINT(readability-magic-numbers)
                if(!utils::fits_in<std::int8_t>(
                     std::get<std::int64_t>(tok.value.value())))
                {
                    throw ty::type_error(
                      loc,
                      std::format(
                        "Integer literal '{}' does not fit in type 'i8' with value range {} to {}.",
                        std::get<std::int64_t>(tok.value.value()),
                        std::numeric_limits<std::int8_t>::min(),
                        std::numeric_limits<std::int8_t>::max()));
                }

                expr_type = ctx.get_i8_type();
                break;
            case 16:    // NOLINT(readability-magic-numbers)
                if(!utils::fits_in<std::int16_t>(
                     std::get<std::int64_t>(tok.value.value())))
                {
                    throw ty::type_error(
                      loc,
                      std::format(
                        "Integer literal '{}' does not fit in type 'i16' with value range {} to {}.",
                        std::get<std::int64_t>(tok.value.value()),
                        std::numeric_limits<std::int16_t>::min(),
                        std::numeric_limits<std::int16_t>::max()));
                }

                expr_type = ctx.get_i16_type();
                break;
            case 32:    // NOLINT(readability-magic-numbers)
                if(!utils::fits_in<std::int32_t>(
                     std::get<std::int64_t>(tok.value.value())))
                {
                    throw ty::type_error(
                      loc,
                      std::format(
                        "Integer literal '{}' does not fit in type 'i32' with value range {} to {}.",
                        std::get<std::int64_t>(tok.value.value()),
                        std::numeric_limits<std::int32_t>::min(),
                        std::numeric_limits<std::int32_t>::max()));
                }

                expr_type = ctx.get_i32_type();
                break;
            case 64:    // NOLINT(readability-magic-numbers)
                expr_type = ctx.get_i64_type();
                break;
            default:
                throw ty::type_error(
                  loc,
                  std::format(
                    "Invalid width '{}' in integer literal.",
                    tok.suffix.value().width));
            }
        }
        else
        {
            expr_type = ctx.get_i32_type();
        }
    }
    else if(tok.type == token_type::fp_literal)
    {
        if(tok.suffix.has_value())
        {
            if(tok.suffix.value().ty != suffix_type::floating_point)
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "Invalid suffix '{}' for floating point literal.",
                    slang::to_string(tok.suffix.value().ty)));
            }

            switch(tok.suffix.value().width)
            {
            case 32:    // NOLINT(readability-magic-numbers)
            {
                auto v = std::get<double>(
                  tok.value.value());
                auto narrowed = static_cast<float>(v);

                if(!std::isfinite(narrowed)
                   && std::isfinite(v))
                {
                    throw ty::type_error(
                      loc,
                      std::format(
                        "Floating point literal '{}' cannot be represented as f32 (overflow to infinity). Valid finite range: {} to {}",
                        tok.s,
                        std::numeric_limits<float>::min(),
                        std::numeric_limits<float>::max()));
                }

                if(narrowed == 0.0f
                   && v != 0.0
                   && std::isfinite(v))
                {
                    std::println(
                      "{}: Warning: Floating point literal '{}' underflows to 0.0 in f32.",
                      slang::to_string(loc),
                      tok.s);
                }

                expr_type = ctx.get_f32_type();
                break;
            }
            case 64:    // NOLINT(readability-magic-numbers)
                expr_type = ctx.get_f64_type();
                break;

            default:
                throw ty::type_error(
                  loc,
                  std::format(
                    "Invalid width '{}' in floating point literal.",
                    tok.suffix.value().width));
            }
        }
        else
        {
            expr_type = ctx.get_f64_type();
        }
    }
    else if(tok.type == token_type::str_literal)
    {
        expr_type = ctx.get_str_type();
    }
    else
    {
        throw ty::type_error(
          tok.location,
          std::format(
            "Unknown literal type with id '{}'.",
            std::to_underlying(tok.type)));
    }

    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string literal_expression::to_string() const
{
    if(tok.type == token_type::fp_literal)
    {
        if(tok.value)
        {
            return std::format("FloatLiteral(value={})", std::get<double>(*tok.value));
        }

        return "FloatLiteral(<none>)";
    }

    if(tok.type == token_type::int_literal)
    {
        if(tok.value)
        {
            return std::format("IntLiteral(value={})", std::get<std::int64_t>(*tok.value));
        }

        return "IntLiteral(<none>)";
    }

    if(tok.type == token_type::str_literal)
    {
        if(tok.value)
        {
            return std::format("StrLiteral(value=\"{}\")", std::get<std::string>(*tok.value));
        }

        return "StrLiteral(<none>)";
    }

    return "UnknownLiteral";
}

/*
 * type_expression.
 */

std::unique_ptr<type_expression> type_expression::clone() const
{
    return std::make_unique<type_expression>(*this);
}

void type_expression::serialize(archive& ar)
{
    ar & loc;
    ar & type_name;
    ar & namespace_stack;
    ar & array;
}

/*
 * type_cast_expression.
 */

std::unique_ptr<expression> type_cast_expression::clone() const
{
    return std::make_unique<type_cast_expression>(*this);
}

void type_cast_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
    ar & target_type;
}

std::unique_ptr<cg::rvalue> type_cast_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    auto v = expr->emit_rvalue(ctx, true);

    // only cast if necessary.
    cg::type lowered_type = ctx.lower(target_type->get_type());
    if(always_cast
       || lowered_type.get_type_kind() != v->get_type().get_type_kind())
    {
        if(v->get_type().get_type_kind() == cg::type_kind::i8
           || v->get_type().get_type_kind() == cg::type_kind::i16)
        {
            // i8 is loaded as a i32 onto the stack.

            if(lowered_type.get_type_kind() == cg::type_kind::i8)
            {
                if(always_cast
                   || v->get_type().get_type_kind() != cg::type_kind::i8)
                {
                    ctx.generate_cast(cg::type_cast::i32_to_i8);
                }
                else
                {
                    throw cg::codegen_error(
                      loc,
                      std::format(
                        "Invalid cast from {} to {}.",
                        v->get_type().to_string(),
                        lowered_type.to_string()));
                }
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i16)
            {
                if(always_cast
                   || v->get_type().get_type_kind() != cg::type_kind::i16)
                {
                    ctx.generate_cast(cg::type_cast::i32_to_i16);
                }
                else
                {
                    throw cg::codegen_error(
                      loc,
                      std::format(
                        "Invalid cast from {} to {}.",
                        v->get_type().to_string(),
                        lowered_type.to_string()));
                }
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i32)
            {
                // no-op.
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i64)
            {
                ctx.generate_cast(cg::type_cast::i32_to_i64);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f32)
            {
                ctx.generate_cast(cg::type_cast::i32_to_f32);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f64)
            {
                ctx.generate_cast(cg::type_cast::i32_to_f64);
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Invalid cast from {} to {}.",
                    v->get_type().to_string(),
                    lowered_type.to_string()));
            }
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i32)
        {
            if(lowered_type.get_type_kind() == cg::type_kind::i8)
            {
                ctx.generate_cast(cg::type_cast::i32_to_i8);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i16)
            {
                ctx.generate_cast(cg::type_cast::i32_to_i16);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i64)
            {
                ctx.generate_cast(cg::type_cast::i32_to_i64);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f32)
            {
                ctx.generate_cast(cg::type_cast::i32_to_f32);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f64)
            {
                ctx.generate_cast(cg::type_cast::i32_to_f64);
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Invalid cast from i32 to {}.",
                    lowered_type.to_string()));
            }
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i64)
        {
            if(lowered_type.get_type_kind() == cg::type_kind::i8)
            {
                ctx.generate_cast(cg::type_cast::i64_to_i32);
                ctx.generate_cast(cg::type_cast::i32_to_i8);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i16)
            {
                ctx.generate_cast(cg::type_cast::i64_to_i32);
                ctx.generate_cast(cg::type_cast::i32_to_i16);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i32)
            {
                ctx.generate_cast(cg::type_cast::i64_to_i32);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f32)
            {
                ctx.generate_cast(cg::type_cast::i64_to_f32);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f64)
            {
                ctx.generate_cast(cg::type_cast::i64_to_f64);
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Invalid cast from i32 to {}.",
                    lowered_type.to_string()));
            }
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f32)
        {
            if(lowered_type.get_type_kind() == cg::type_kind::i8)
            {
                ctx.generate_cast(cg::type_cast::f32_to_i32);
                ctx.generate_cast(cg::type_cast::i32_to_i8);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i16)
            {
                ctx.generate_cast(cg::type_cast::f32_to_i32);
                ctx.generate_cast(cg::type_cast::i32_to_i16);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i32)
            {
                ctx.generate_cast(cg::type_cast::f32_to_i32);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i64)
            {
                ctx.generate_cast(cg::type_cast::f32_to_i64);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f64)
            {
                ctx.generate_cast(cg::type_cast::f32_to_f64);
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Invalid cast from f32 to {}.",
                    lowered_type.to_string()));
            }
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f64)
        {
            if(lowered_type.get_type_kind() == cg::type_kind::i8)
            {
                ctx.generate_cast(cg::type_cast::f64_to_i32);
                ctx.generate_cast(cg::type_cast::i32_to_i8);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i16)
            {
                ctx.generate_cast(cg::type_cast::f64_to_i32);
                ctx.generate_cast(cg::type_cast::i32_to_i16);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i32)
            {
                ctx.generate_cast(cg::type_cast::f64_to_i32);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::i64)
            {
                ctx.generate_cast(cg::type_cast::f64_to_i64);
            }
            else if(lowered_type.get_type_kind() == cg::type_kind::f32)
            {
                ctx.generate_cast(cg::type_cast::f64_to_f32);
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Invalid cast from f32 to {}.",
                    lowered_type.to_string()));
            }
        }
        else if(lowered_type.get_type_kind() == cg::type_kind::str
                && v->get_type().get_type_kind() != cg::type_kind::ref)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Cannot cast '{}' to 'str'.",
                v->get_type().to_string()));
        }
        else
        {
            if(lowered_type.get_type_kind() == cg::type_kind::str)
            {
                return std::make_unique<cg::rvalue>(lowered_type);
            }

            // casts between non-builtin types are checked at run-time.
            ctx.generate_checkcast(lowered_type);
            return std::make_unique<cg::rvalue>(lowered_type);
        }

        return std::make_unique<cg::rvalue>(lowered_type);
    }

    // both type kinds are the same here.
    if(v->get_type().get_type_kind() == cg::type_kind::ref)
    {
        if(v->get_type().get_type_id() != target_type->get_type())
        {
            // casts between non-builtin types are checked at run-time.
            ctx.generate_checkcast(lowered_type);
            return std::make_unique<cg::rvalue>(lowered_type);
        }
    }

    return std::make_unique<cg::rvalue>(lowered_type);
}

void type_cast_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    if(expr)
    {
        expr->collect_names(ctx);
    }
}

std::optional<ty::type_id> type_cast_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    target_type->type_check(ctx, env);

    auto type = expr->type_check(ctx, env);
    if(!type.has_value())
    {
        throw ty::type_error(loc, "Type cast expression has no type.");
    }

    // casts for primitive types.
    const std::unordered_map<ty::type_id, std::set<ty::type_id>> primitive_type_casts = {
      {ctx.get_i8_type(),
       {
         ctx.get_i8_type(),  /* no-op. */
         ctx.get_i16_type(), /* sign-extend to i32, then narrow to i16 */
         ctx.get_i32_type(), /* sign-extend to i32. */
         ctx.get_i64_type(), /* sign-extend to i32, then sign-extend to i64. */
         ctx.get_f32_type(), /* sign-extend to i32, convert to f32. */
         ctx.get_f64_type(), /* sign-extend to i32, convert to f64. */
       }},
      {ctx.get_i16_type(),
       {
         ctx.get_i8_type(),  /* narrow to i16. */
         ctx.get_i16_type(), /* no-op. */
         ctx.get_i32_type(), /* sign-extend to i32. */
         ctx.get_i64_type(), /* sign-extend to i32, then sign-extend to i64. */
         ctx.get_f32_type(), /* sign-extend to i32, convert to f32. */
         ctx.get_f64_type(), /* sign-extend to i32, convert to f64. */
       }},
      {ctx.get_i32_type(),
       {
         ctx.get_i8_type(),  /* convert to i8. */
         ctx.get_i16_type(), /* convert to i16. */
         ctx.get_i32_type(), /* no-op. */
         ctx.get_i64_type(), /* convert to i64. */
         ctx.get_f32_type(), /* convert to f32. */
         ctx.get_f64_type(), /* convert to f64. */
       }},
      {ctx.get_i64_type(),
       {
         ctx.get_i8_type(),  /* convert to i32, narrow to i8. */
         ctx.get_i16_type(), /* convert to i32, narrow to i16. */
         ctx.get_i32_type(), /* convert to i32. */
         ctx.get_i64_type(), /* no-op. */
         ctx.get_f32_type(), /* convert to f32. */
         ctx.get_f64_type(), /* convert to f64. */
       }},
      {ctx.get_f32_type(),
       {
         ctx.get_i8_type(),  /* convert to i32, narrow to i8. */
         ctx.get_i16_type(), /* convert to i32, narrow to i16. */
         ctx.get_i32_type(), /* convert to i32. */
         ctx.get_i64_type(), /* convert to i64. */
         ctx.get_f32_type(), /* no-op. */
         ctx.get_f64_type(), /* convert to f64. */
       }},
      {ctx.get_f64_type(),
       {
         ctx.get_i8_type(),  /* convert to i32, narrow to i8. */
         ctx.get_i16_type(), /* convert to i32, narrow to i16. */
         ctx.get_i32_type(), /* convert to i32. */
         ctx.get_i64_type(), /* convert to i64. */
         ctx.get_f32_type(), /* convert to f32. */
         ctx.get_f64_type(), /* no-op. */
       }},
      {ctx.get_str_type(), {}}};

    auto cast_from = primitive_type_casts.find(type.value());
    if(cast_from != primitive_type_casts.end())
    {
        auto cast_to = cast_from->second.find(
          target_type->get_type());
        if(cast_to == cast_from->second.end())
        {
            throw ty::type_error(
              loc,
              std::format(
                "Invalid cast to non-primitive type '{}'.",
                target_type->get_name()));
        }
    }

    // casts for struct types. this is checked at run-time.
    expr_type = ctx.get_type(target_type->get_qualified_name());    // no array casts.
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string type_cast_expression::to_string() const
{
    return std::format(
      "TypeCast(target_type={}, expr={})",
      target_type->to_string(),
      expr ? expr->to_string() : std::string("<none>"));
}

/*
 * namespace_access_expression.
 */

std::unique_ptr<expression> namespace_access_expression::clone() const
{
    return std::make_unique<namespace_access_expression>(*this);
}

void namespace_access_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & name;
    ar& expression_serializer{expr};
}

void namespace_access_expression::generate_code(
  cg::context& ctx) const
{
    // NOTE update_namespace is (intentionally) not const, so we cannot use it here.
    auto expr_namespace_stack = namespace_stack;
    expr_namespace_stack.push_back(name.s);
    expr->set_namespace(std::move(expr_namespace_stack));
    expr->generate_code(ctx);
}

std::unique_ptr<cg::lvalue> namespace_access_expression::emit_lvalue(
  cg::context& ctx) const
{
    // NOTE update_namespace is (intentionally) not const, so we cannot use it here.
    auto expr_namespace_stack = namespace_stack;
    expr_namespace_stack.push_back(name.s);
    expr->set_namespace(std::move(expr_namespace_stack));
    return expr->emit_lvalue(ctx);
}

std::unique_ptr<cg::rvalue> namespace_access_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    // NOTE update_namespace is (intentionally) not const, so we cannot use it here.
    auto expr_namespace_stack = namespace_stack;
    expr_namespace_stack.push_back(name.s);
    expr->set_namespace(std::move(expr_namespace_stack));
    return expr->emit_rvalue(ctx, result_used);
}

void namespace_access_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    update_namespace();
    expr->collect_names(ctx);
}

std::optional<ty::type_id> namespace_access_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    expr_type = expr->type_check(ctx, env);
    if(!expr_type.has_value())
    {
        throw ty::type_error(
          loc,
          "Type check: Expression has no type in namespace access.");
    }
    ctx.set_expression_type(*this, expr_type);
    return expr_type;
}

std::string namespace_access_expression::to_string() const
{
    return std::format(
      "Scope(name={}, expr={})",
      name.s,
      expr ? expr->to_string() : std::string("<none>"));
}

access_expression::access_expression(std::unique_ptr<ast::expression> lhs, std::unique_ptr<ast::expression> rhs)
: expression{lhs->get_location()}
, lhs{std::move(lhs)}
, rhs{std::move(rhs)}
{
}

std::unique_ptr<expression> access_expression::clone() const
{
    return std::make_unique<access_expression>(*this);
}

void access_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{lhs};
    ar& expression_serializer{rhs};
    ar & struct_type;
}

std::unique_ptr<cg::lvalue> access_expression::emit_lvalue(
  cg::context& ctx) const
{
    // validate expression.
    if(!expr_type.has_value())
    {
        throw cg::codegen_error(loc, "Access expression has no type.");
    }
    if(!rhs)
    {
        throw cg::codegen_error(loc, "Access expression has no r.h.s.");
    }
    if(!struct_type.has_value())
    {
        throw cg::codegen_error(loc, "Access expression has no struct type.");
    }

    // load l.h.s. for access.
    auto lhs_value = lhs->emit_rvalue(ctx, true);

    /*
     * arrays.
     */
    if(lhs_is_array)
    {
        // array properties are read-only.
        // only report errors here.

        if(!rhs->is_named_expression())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Could not find name for element access in array access expression."));
        }
        auto* identifier_expr = rhs->as_named_expression();

        if(identifier_expr->get_name() == "length")
        {
            throw cg::codegen_error(rhs->get_location(), "Array length is read only.");
        }

        throw cg::codegen_error(
          rhs->get_location(),
          std::format(
            "Unknown array property '{}'.",
            identifier_expr->get_name()));
    }

    /*
     * structs.
     */

    // generate access instructions for rhs.
    if(!rhs->is_named_expression())
    {
        return rhs->emit_lvalue(ctx);
    }

    return std::make_unique<cg::lvalue>(
      ctx.lower(expr_type.value()),
      cg::field_location_info{
        .struct_type = cg::type{struct_type.value(), cg::type_kind::ref},
        .field_index = field_index});
}

std::unique_ptr<cg::rvalue> access_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    // validate expression.
    if(!expr_type.has_value())
    {
        throw cg::codegen_error(loc, "Access expression has no type.");
    }
    if(!rhs)
    {
        throw cg::codegen_error(loc, "Access expression has no r.h.s.");
    }
    if(!struct_type.has_value())
    {
        throw cg::codegen_error(loc, "Access expression has no struct type.");
    }

    auto lhs_value = lhs->emit_rvalue(ctx, true);

    /*
     * arrays.
     */
    if(lhs_is_array)
    {
        if(!rhs->is_named_expression())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Could not find name for element access in array access expression."));
        }
        auto* identifier_expr = rhs->as_named_expression();

        if(identifier_expr->get_name() == "length")
        {
            ctx.generate_arraylength();
            return std::make_unique<cg::rvalue>(
              cg::type{cg::type_kind::i32});
        }

        throw cg::codegen_error(
          rhs->get_location(),
          std::format(
            "Unknown array property '{}'.",
            identifier_expr->get_name()));
    }

    /*
     * structs.
     */

    // generate access instructions for rhs.
    if(!rhs->is_named_expression())
    {
        return rhs->emit_rvalue(ctx);
    }

    auto type = cg::type{struct_type.value(), cg::type_kind::ref};

    ctx.generate_get_field(
      std::make_unique<cg::field_access_argument>(
        type,
        field_index));

    return std::make_unique<cg::rvalue>(
      ctx.lower(expr_type.value()));
}

void access_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    lhs->collect_names(ctx);
}

std::optional<ty::type_id> access_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    auto type = lhs->type_check(ctx, env);
    if(!type.has_value())
    {
        throw ty::type_error(
          loc,
          "L.h.s. has no value in element access expression.");
    }

    lhs_is_array = ctx.is_array(type.value());
    if(!lhs_is_array
       && !ctx.is_struct(type.value()))
    {
        throw ty::type_error(
          loc,
          std::format(
            "Base type '{}' is not a struct or array.",
            ctx.to_string(type.value())));
    }

    if(ctx.is_struct(type.value()))
    {
        struct_info = ctx.get_struct_info(type.value());
    }
    struct_type = type;    // includes arrays.

    // get field.
    if(rhs->get_id() != node_identifier::variable_reference_expression)
    {
        throw std::runtime_error(
          std::format(
            "{}: Expected <identifier> as accessor (got node id {}).",
            ::slang::to_string(loc),
            std::to_underlying(rhs->get_id())));
    }

    const auto* identifier_node = rhs->as_variable_reference();
    field_index = ctx.get_field_index(type.value(), identifier_node->get_name());
    expr_type = ctx.get_field_type(type.value(), field_index);
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string access_expression::to_string() const
{
    return std::format("Access(lhs={}, rhs={})", lhs->to_string(), rhs->to_string());
}

ty::type_id access_expression::get_struct_type() const
{
    if(!struct_type.has_value())
    {
        throw ty::type_error(
          loc,
          "No struct type set for element access.");
    }

    return struct_type.value();
}

/*
 * expression_statement.
 */

std::unique_ptr<expression> expression_statement::clone() const
{
    return std::make_unique<expression_statement>(*this);
}

void expression_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
}

void expression_statement::generate_code(
  cg::context& ctx) const
{
    auto v = expr->emit_rvalue(ctx, false);
    if(v != nullptr
       && v->get_type().get_type_kind() != cg::type_kind::void_)
    {
        // clean up stack.
        ctx.generate_pop(v->get_type());
    }
}

std::unique_ptr<cg::rvalue> expression_statement::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    return expr->emit_rvalue(ctx, result_used);
}

void expression_statement::collect_names(co::context& ctx)
{
    expr->collect_names(ctx);
}

std::optional<ty::type_id> expression_statement::type_check(
  ty::context& ctx,
  sema::env& env)
{
    return expr->type_check(ctx, env);
}

std::string expression_statement::to_string() const
{
    return std::format(
      "ExpressionStatement(expr={})",
      expr->to_string());
}

/*
 * import_statement.
 */

std::unique_ptr<expression> import_statement::clone() const
{
    return std::make_unique<import_statement>(*this);
}

void import_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar & path;
}

void import_statement::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    symbol_id = ctx.declare(
      path.begin()->s,
      utils::join(
        path
          | std::views::transform([](const auto& c)
                                  { return c.s; })
          | std::ranges::to<std::vector>(),
        "::"),
      sema::symbol_type::module_,
      path.begin()->location,
      sema::symbol_id::invalid,
      false);
}

std::string import_statement::to_string() const
{
    auto transform = [](const token& p) -> std::string
    { return p.s; };

    return std::format(
      "Import(path={})",
      slang::utils::join(path, {transform}, "."));
}

/*
 * directive_expression.
 */

std::unique_ptr<expression> directive_expression::clone() const
{
    return std::make_unique<directive_expression>(*this);
}

void directive_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & args;
    ar& expression_serializer{expr};
}

static void update_flags(
  cg::codegen_flag_type& flags,
  const std::string& name,
  bool enable)
{
    cg::codegen_flags flag = cg::codegen_flags::none;

    if(name == "const_eval")
    {
        flag = cg::codegen_flags::enable_const_eval;
    }

    if(enable)
    {
        flags |= std::to_underlying(flag);
    }
    else
    {
        flags &= ~std::to_underlying(flag);
    }
};

static void update_flags(
  const std::string& name,
  cg::codegen_flag_type& flags,
  const std::vector<std::pair<slang::token, slang::token>>& args)
{
    if(name == "enable")
    {
        for(const auto& [key, _]: args)
        {
            update_flags(flags, key.s, true);
        }
    }
    else if(name == "disable")
    {
        for(const auto& [key, _]: args)
        {
            update_flags(flags, key.s, false);
        }
    }
}

void directive_expression::generate_code(
  cg::context& ctx) const
{
    // enable/disable codegen flags.
    auto saved_flags = ctx.get_flags();
    auto _ = gsl::finally(
      [&ctx, saved_flags]()
      {
          ctx.set_flags(saved_flags);
      });

    auto new_flags = saved_flags;
    update_flags(name.s, new_flags, args);
    ctx.set_flags(new_flags);

    expr->generate_code(ctx);
}

std::unique_ptr<cg::lvalue> directive_expression::emit_lvalue(
  cg::context& ctx) const
{
    // enable/disable codegen flags.
    auto saved_flags = ctx.get_flags();
    auto _ = gsl::finally(
      [&ctx, saved_flags]()
      {
          ctx.set_flags(saved_flags);
      });

    auto new_flags = saved_flags;
    update_flags(name.s, new_flags, args);
    ctx.set_flags(new_flags);

    return expr->emit_lvalue(ctx);
}

std::unique_ptr<cg::rvalue> directive_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    // enable/disable codegen flags.
    auto saved_flags = ctx.get_flags();
    auto _ = gsl::finally(
      [&ctx, saved_flags]()
      {
          ctx.set_flags(saved_flags);
      });

    auto new_flags = saved_flags;
    update_flags(name.s, new_flags, args);
    ctx.set_flags(new_flags);

    return expr->emit_rvalue(ctx, result_used);
}

void directive_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    expr->collect_names(ctx);
}

std::optional<ty::type_id> directive_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    expr_type = expr->type_check(ctx, env);
    if(expr_type.has_value())
    {
        ctx.set_expression_type(*this, expr_type);
    }
    return expr_type;
}

std::string directive_expression::to_string() const
{
    auto transform = [](const std::pair<token, token>& p) -> std::string
    { return std::format("{}, {}", p.first.s, p.second.s); };

    return std::format(
      "Directive(name={}, args=({}), expr={})",
      name.s,
      slang::utils::join(args, {transform}, ","),
      expr->to_string());
}

expression* directive_expression::get_target()
{
    auto* it = expr.get();
    for(; it != nullptr
          && it->get_id() == node_identifier::directive_expression;
        it = static_cast<directive_expression*>(it)->expr.get())    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    {
        // empty.
    }

    if(it == nullptr)
    {
        throw std::runtime_error(
          std::format(
            "{}: Directive without target",
            ::slang::to_string(get_location())));
    }

    return it;
}

const expression* directive_expression::get_target() const
{
    const auto* it = expr.get();
    for(; it != nullptr
          && it->get_id() == node_identifier::directive_expression;
        it = static_cast<const directive_expression*>(it)->expr.get())    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    {
        // empty.
    }

    if(it == nullptr)
    {
        throw std::runtime_error(
          std::format(
            "{}: Directive without target",
            ::slang::to_string(get_location())));
    }

    return it;
}

/*
 * variable_reference_expression.
 */

std::unique_ptr<expression> variable_reference_expression::clone() const
{
    return std::make_unique<variable_reference_expression>(*this);
}

void variable_reference_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expansion};
}

std::unique_ptr<cg::lvalue> variable_reference_expression::emit_lvalue(
  cg::context& ctx) const
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Reference '{}' has no symbol id.",
            name.s));
    }

    if(!expr_type.has_value())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Reference '{}' has no type.",
            name.s));
    }

    // check for macro expansions first.
    if(expansion)
    {
        return expansion->emit_lvalue(ctx);
    }

    const auto& sema_env = ctx.get_sema_env();
    auto it = sema_env.symbol_table.find(symbol_id.value());
    if(it == sema_env.symbol_table.cend())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "{}: Reference not found in symbol table (symbol id: {}).",
            get_name(),
            symbol_id.value().value));
    }

    const auto& info = it->second;

    switch(info.type)
    {
    case sema::symbol_type::constant:
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Cannot assign to constant '{}'.",
            info.name));
    }

    case sema::symbol_type::variable:
    {
        auto type = ctx.lower(expr_type.value());
        return std::make_unique<cg::lvalue>(
          cg::lvalue{
            type,
            cg::variable_location_info{
              .index = ctx.get_current_function()->get_index(
                symbol_id.value())},
            symbol_id}    // NOLINT(bugprone-unchecked-optional-access)
        );
    }
    default:;
        // fall-through.
    }

    throw cg::codegen_error(
      loc,
      std::format(
        "Identifier '{}' is not a value.",
        info.name));
}

std::unique_ptr<cg::rvalue> variable_reference_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Reference '{}' has no symbol id.",
            name.s));
    }

    if(!expr_type.has_value())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Reference '{}' has no type.",
            name.s));
    }

    // check for macro expansions first.
    if(expansion)
    {
        return expansion->emit_rvalue(ctx, result_used);
    }

    const auto& sema_env = ctx.get_sema_env();
    auto it = sema_env.symbol_table.find(symbol_id.value());
    if(it == sema_env.symbol_table.cend())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "{}: Reference not found in symbol table (symbol id: {}).",
            get_name(),
            symbol_id.value().value));
    }

    const auto& info = it->second;

    switch(info.type)
    {
    case sema::symbol_type::constant:
    {
        auto const_info = ctx.get_const_env().get_const_info(symbol_id.value());
        if(!const_info.has_value())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Could not find constant info for '{}'.",
                info.name));
        }

        if(const_info->type == const_::constant_type::i32)
        {
            ctx.generate_const(
              {cg::type_kind::i32},
              std::get<std::int64_t>(const_info->value));
            return std::make_unique<cg::rvalue>(
              cg::type{cg::type_kind::i32},
              symbol_id);
        }

        if(const_info->type == const_::constant_type::i64)
        {
            ctx.generate_const(
              {cg::type_kind::i64},
              std::get<std::int64_t>(const_info->value));
            return std::make_unique<cg::rvalue>(
              cg::type{cg::type_kind::i64},
              symbol_id);
        }

        if(const_info->type == const_::constant_type::f32)
        {
            ctx.generate_const(
              {cg::type_kind::f32},
              std::get<double>(const_info->value));
            return std::make_unique<cg::rvalue>(
              cg::type{cg::type_kind::f32},
              symbol_id);
        }

        if(const_info->type == const_::constant_type::f64)
        {
            ctx.generate_const(
              {cg::type_kind::f64},
              std::get<double>(const_info->value));
            return std::make_unique<cg::rvalue>(
              cg::type{cg::type_kind::f64},
              symbol_id);
        }

        if(const_info->type == const_::constant_type::str)
        {
            ctx.generate_const(
              {cg::type_kind::str},
              ctx.intern(
                std::get<std::string>(const_info->value)));

            return std::make_unique<cg::rvalue>(
              cg::type{cg::type_kind::str},
              symbol_id);
        }

        // unsupported type.
        throw cg::codegen_error(
          std::format(
            "Unsupported constant type '{}'.",
            std::to_underlying(const_info->type)));
    }

    case sema::symbol_type::variable:
    {
        auto type = ctx.lower(expr_type.value());
        ctx.generate_load(
          cg::lvalue{
            type,
            cg::variable_location_info{
              .index = ctx.get_current_function()->get_index(
                symbol_id.value())},
            symbol_id});

        return std::make_unique<cg::rvalue>(type, symbol_id);
    }
    default:;
        // fall-through.
    }

    throw cg::codegen_error(
      loc,
      std::format(
        "Identifier '{}' is not a value.",
        info.name));
}

void variable_reference_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    if(expansion)
    {
        expansion->collect_names(ctx);
    }
}

std::optional<ty::type_id> variable_reference_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    ty::type_id type = [this, &ctx, &env]() -> ty::type_id
    {
        if(expansion)
        {
            std::optional<ty::type_id> t = expansion->type_check(ctx, env);
            if(!t.has_value())
            {
                throw ty::type_error(
                  expansion->get_location(),
                  "Expression has no type.");
            }

            return t.value();
        }

        if(!symbol_id.has_value())
        {
            throw ty::type_error(
              loc,
              std::format(
                "Identifier '{}' has no symbol id.",
                get_qualified_name()));
        }

        if(!env.type_map.contains(symbol_id.value()))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Identifier '{}' not in type map.",
                get_qualified_name()));
        }

        return env.type_map[symbol_id.value()];
    }();

    expr_type = type;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string variable_reference_expression::to_string() const
{
    std::string ret = std::format("VariableReference(name={}", name.s);

    if(expansion)
    {
        ret += std::format(
          ", expansion={}",
          expansion->to_string());
    }
    ret += ")";

    return ret;
}

/*
 * array_subscript_expression.
 */

std::unique_ptr<expression> array_subscript_expression::clone() const
{
    return std::make_unique<array_subscript_expression>(*this);
}

void array_subscript_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{lhs};
    ar& expression_serializer{subscript_expr};
}

std::unique_ptr<cg::lvalue> array_subscript_expression::emit_lvalue(
  cg::context& ctx) const
{
    // load array reference and index
    auto lhs_value = lhs->emit_rvalue(ctx, true);
    subscript_expr->emit_rvalue(ctx, true);

    return std::make_unique<cg::lvalue>(
      cg::lvalue{
        ctx.lower(expr_type.value()),
        cg::array_location_info{},
        symbol_id}    // NOLINT(bugprone-unchecked-optional-access)
    );
}

std::unique_ptr<cg::rvalue> array_subscript_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    // load array reference and index
    auto lhs_value = lhs->emit_rvalue(ctx, true);
    subscript_expr->emit_rvalue(ctx, true);

    auto type = ctx.lower(expr_type.value());
    ctx.generate_load(
      cg::lvalue{
        type,
        cg::array_location_info{},
        symbol_id});

    return std::make_unique<cg::rvalue>(type, symbol_id);
}

void array_subscript_expression::collect_names(
  co::context& ctx)
{
    super::collect_names(ctx);
    lhs->collect_names(ctx);
    subscript_expr->collect_names(ctx);
}

std::optional<ty::type_id> array_subscript_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    auto lhs_type = lhs->type_check(ctx, env);
    if(!lhs_type.has_value())
    {
        throw ty::type_error(
          lhs->get_location(),
          "Eexpression has no type.");
    }

    if(!ctx.is_array(lhs_type.value()))
    {
        throw ty::type_error(
          loc,
          std::format(
            "Cannot use subscript on non-array type '{}'.",
            ctx.to_string(lhs_type.value())));
    }

    auto subscript_type = subscript_expr->type_check(ctx, env);
    if(!subscript_type.has_value())
    {
        throw ty::type_error(
          subscript_expr->get_location(),
          "Subscript expression has no type.");
    }
    if(!ctx.are_types_compatible(
         ctx.get_i32_type(),
         subscript_type.value()))
    {
        throw ty::type_error(
          subscript_expr->get_location(),
          std::format(
            "Expected <integer> for array element access, got '{}'.",
            ctx.to_string(subscript_type.value())));
    }

    expr_type = ctx.array_element_type(lhs_type.value());
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string array_subscript_expression::to_string() const
{
    return std::format(
      "ArraySubscript(lhs={}, subscript_expr={})",
      lhs->to_string(),
      subscript_expr->to_string());
}

/*
 * type_expression.
 */

void type_expression::type_check(
  ty::context& ctx,
  [[maybe_unused]] sema::env& env)
{
    type_id = ctx.get_type(get_qualified_name());
    if(array)
    {
        type_id = ctx.get_array(type_id, 1);
    }
}

std::string type_expression::get_qualified_name() const
{
    std::optional<std::string> namespace_path = get_namespace_path();
    if(namespace_path.has_value())
    {
        return name::qualified_name(
          namespace_path.value(),
          type_name.s);
    }
    return type_name.s;
}

std::optional<std::string> type_expression::get_namespace_path() const
{
    if(namespace_stack.empty())
    {
        return std::nullopt;
    }

    auto transform = [](const token& t) -> std::string
    {
        return t.s;
    };

    return slang::utils::join(namespace_stack, {transform}, "::");
}

std::string type_expression::to_string() const
{
    std::string namespace_string;
    if(!namespace_stack.empty())
    {
        for(std::size_t i = 0; i < namespace_stack.size() - 1; ++i)
        {
            namespace_string += std::format("{}, ", namespace_stack[i].s);
        }
        namespace_string += namespace_stack.back().s;
    }

    return std::format(
      "TypeExpression(name={}, namespaces=({}), array={})",
      get_name(),
      namespace_string,
      array);
}

/*
 * variable_declaration_expression.
 */

std::unique_ptr<expression> variable_declaration_expression::clone() const
{
    return std::make_unique<variable_declaration_expression>(*this);
}

void variable_declaration_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & type;
    ar& expression_serializer{expr};
}

void variable_declaration_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    const std::string canonical_name = name::qualified_name(
      ctx.get_canonical_scope_name(ctx.get_current_scope()),
      name.s);

    symbol_id = ctx.declare(
      name.s,
      canonical_name,
      sema::symbol_type::variable,
      name.location,
      sema::symbol_id::invalid,
      false);

    if(expr)
    {
        expr->collect_names(ctx);
    }
}

void variable_declaration_expression::generate_code(
  cg::context& ctx) const
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          name.location,
          std::format(
            "Variable '{}' has no symbol id.",
            name.s));
    }

    auto lowered_type = ctx.lower(type->get_type());
    ctx.push_declaration_type(lowered_type);

    auto* fn = ctx.get_current_function();
    if(fn != nullptr)
    {
        fn->add_local(
          symbol_id.value(),
          lowered_type);
    }

    if(expr)
    {
        expr->emit_rvalue(ctx, true);
        ctx.generate_store(
          cg::lvalue{
            lowered_type,
            cg::variable_location_info{
              .index = fn->get_index(symbol_id.value())},
            symbol_id});
    }

    ctx.pop_declaration_type();
}

std::optional<ty::type_id> variable_declaration_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(ctx.has_expression_type(*this))
    {
        return ctx.get_expression_type(*this);
    }

    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          name.location,
          std::format(
            "Variable '{}' has no symbol id.",
            name.s));
    }

    type->type_check(ctx, env);

    auto annotated_type_id = ctx.get_type(type->get_qualified_name());
    if(type->is_array())
    {
        annotated_type_id = ctx.get_array(annotated_type_id, 1);
    }
    env.type_map.insert({symbol_id.value(), annotated_type_id});

    if(expr)
    {
        auto rhs = expr->type_check(ctx, env);
        if(!rhs.has_value())
        {
            throw ty::type_error(
              loc,
              "Expression has no type.");
        }

        ctx.set_expression_type(*expr.get(), rhs);

        if(!ctx.are_types_compatible(annotated_type_id, rhs.value()))
        {
            throw ty::type_error(
              name.location,
              std::format(
                "R.h.s. has type '{}' (type id {}), which does not match the variable's type '{}' (type id {}).",
                ctx.to_string(rhs.value()),
                rhs.value(),
                ctx.to_string(annotated_type_id),
                annotated_type_id));
        }
    }

    expr_type = annotated_type_id;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string variable_declaration_expression::to_string() const
{
    return std::format(
      "VariableDeclaration(name={}, type={}, expr={})",
      name.s,
      type->to_string(),
      expr ? expr->to_string() : std::string("<none>"));
}

void variable_declaration_expression::insert_implicit_casts(
  ty::context& ctx,
  sema::env& env)
{
    if(expr == nullptr)
    {
        return;
    }

    if(!ctx.has_expression_type(*expr.get()))
    {
        // only insert casts if the type is known.
        // the type is unknown inside non-expanded macros.
        return;
    }

    auto expr_type = ctx.get_expression_type(*expr.get());
    if(!expr_type.has_value())
    {
        return;
    }

    if(expr_type.value() == ctx.get_i8_type()
       || expr_type.value() == ctx.get_i16_type())
    {
        expr = std::make_unique<type_cast_expression>(
          expr->get_location(),
          std::move(expr),
          std::make_unique<type_expression>(
            expr->get_location(),
            token{ctx.to_string(expr_type.value()), expr->get_location()},
            std::vector<token>{},
            ctx.is_array(expr_type.value())),
          true /* always cast */);

        expr->type_check(ctx, env);    // FIXME should not need to be re-checked.
    }
}

/*
 * constant_declaration_expression.
 */

std::unique_ptr<expression> constant_declaration_expression::clone() const
{
    return std::make_unique<constant_declaration_expression>(*this);
}

void constant_declaration_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & type;
    ar& expression_serializer{expr};
}

void constant_declaration_expression::generate_code(
  cg::context& ctx) const
{
    // just ensure the value exists in the constant environment.
    const auto& env = ctx.get_const_env();

    if(env.const_eval_expr_values.find(expr.get())
       == env.const_eval_expr_values.cend())
    {
        throw cg::codegen_error(
          expr->get_location(),
          "Expression in constant declaration is not compile-time computable.");
    }
}

void constant_declaration_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    const std::string canonical_name = name::qualified_name(
      ctx.get_canonical_scope_name(ctx.get_current_scope()),
      name.s);

    symbol_id = ctx.declare(
      name.s,
      canonical_name,
      sema::symbol_type::constant,
      name.location,
      sema::symbol_id::invalid,
      false);

    if(expr)
    {
        expr->collect_names(ctx);
    }
}

std::optional<ty::type_id> constant_declaration_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Constant '{}' has no symbol id.",
            name.s));
    }

    // Prevent double declaration of constant.
    if(ctx.has_expression_type(*this))
    {
        return ctx.get_expression_type(*this);
    }

    auto annotated_type_id = ctx.get_type(type->get_qualified_name());
    if(type->is_array())
    {
        annotated_type_id = ctx.get_array(annotated_type_id, 1);
    }
    env.type_map.insert({symbol_id.value(), annotated_type_id});

    auto rhs = expr->type_check(ctx, env);
    if(!rhs.has_value())
    {
        throw ty::type_error(
          name.location,
          "Expression has no type.");
    }

    // Either the types match, or the type is a reference type which is set to 'null'.
    if(!ctx.are_types_compatible(annotated_type_id, rhs.value()))
    {
        throw ty::type_error(
          name.location,
          std::format(
            "R.h.s. has type '{}' (type id {}), which does not match the constant's type '{}' (type id {}).",
            ctx.to_string(rhs.value()),
            rhs.value(),
            ctx.to_string(annotated_type_id),
            annotated_type_id));
    }

    return std::nullopt;
}

std::string constant_declaration_expression::to_string() const
{
    return std::format(
      "Constant(name={}, type={}, expr={})",
      name.s,
      type->to_string(),
      expr->to_string());
}

/*
 * array_initializer_expression.
 */

std::unique_ptr<expression> array_initializer_expression::clone() const
{
    return std::make_unique<array_initializer_expression>(*this);
}

void array_initializer_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{exprs};
}

std::unique_ptr<cg::rvalue> array_initializer_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    std::unique_ptr<cg::rvalue> v;
    auto array_type = ctx.get_declaration_type();
    auto element_type = ctx.deref(array_type);

    if(exprs.size() >= std::numeric_limits<std::int32_t>::max())
    {
        throw cg::codegen_error(
          std::format(
            "Cannot generate code for array initializer list: list size exceeds numeric limits ({} >= {}).",
            exprs.size(),
            std::numeric_limits<std::int32_t>::max()));
    }

    ctx.generate_const(
      {cg::type_kind::i32},
      static_cast<int>(exprs.size()));
    ctx.generate_newarray(
      element_type);

    for(std::size_t i = 0; i < exprs.size(); ++i)
    {
        const auto& expr = exprs[i];

        // the top of the stack contains the array address.
        ctx.generate_dup(cg::rvalue{array_type});
        ctx.generate_const(
          {cg::type_kind::i32},
          static_cast<int>(i));

        auto expr_value = expr->emit_rvalue(ctx, true);
        if(i >= std::numeric_limits<std::int32_t>::max())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Array index exceeds max i32 size ({}).",
                std::numeric_limits<std::int32_t>::max()));
        }

        ctx.generate_store(
          cg::lvalue{
            element_type,
            cg::array_location_info{}});

        if(!v)
        {
            v = std::move(expr_value);
        }
        else
        {
            if(v->get_type() != expr_value->get_type())
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Inconsistent types in array initialization: '{}' and '{}'.",
                    v->get_type().to_string(),
                    expr_value->get_type().to_string()));
            }
        }
    }

    return v;
}

void array_initializer_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    for(auto& it: exprs)
    {
        it->collect_names(ctx);
    }
}

std::optional<ty::type_id> array_initializer_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    std::optional<ty::type_id> element_type = std::nullopt;
    for(auto& it: exprs)
    {
        auto type = it->type_check(ctx, env);

        if(!type.has_value())
        {
            throw ty::type_error(loc, "Initializer expression has no type.");
        }

        if(element_type.has_value())
        {
            if(element_type != type)
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "Initializer types do not match. Found '{}' and '{}'.",
                    ctx.to_string(element_type.value()),
                    ctx.to_string(type.value())));
            }
        }
        else
        {
            element_type = type;
        }
    }

    if(!element_type.has_value())
    {
        throw ty::type_error(loc, "Initializer expression has no type.");
    }

    expr_type = ctx.get_array(element_type.value(), 1);
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string array_initializer_expression::to_string() const
{
    std::string ret = "ArrayInitializer(exprs=(";

    for(std::size_t i = 0; i < exprs.size() - 1; ++i)
    {
        ret += std::format("{}, ", exprs[i]->to_string());
    }
    ret += std::format("{}))", exprs.back()->to_string());

    return ret;
}

/*
 * struct_definition_expression.
 */

std::unique_ptr<expression> struct_definition_expression::clone() const
{
    return std::make_unique<struct_definition_expression>(*this);
}

void struct_definition_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{members};
}

void struct_definition_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    const std::string canonical_name = name::qualified_name(
      ctx.get_canonical_scope_name(ctx.get_current_scope()),
      name.s);

    symbol_id = ctx.declare(
      name.s,
      canonical_name,
      sema::symbol_type::type,
      name.location,
      sema::symbol_id::invalid,
      false,
      this);

    ctx.push_scope(std::format("{}@struct", canonical_name), name.location);
    for(auto& m: members)
    {
        m->collect_names(ctx);
    }
    ctx.pop_scope();
}

void struct_definition_expression::declare_type(ty::context& ctx, sema::env& env)
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Struct definition for '{}' has no symbol id.",
            name.s));
    }

    struct_type_id = ctx.declare_struct(get_name(), std::nullopt);
    auto& struct_info = ctx.get_struct_info(struct_type_id);

    if(env.has_attribute(
         symbol_id.value(),
         attribs::attribute_kind::allow_cast))
    {
        struct_info.allow_cast = true;
    }

    if(env.has_attribute(
         symbol_id.value(),
         attribs::attribute_kind::native))
    {
        struct_info.native = true;
    }
}

void struct_definition_expression::define_type(ty::context& ctx) const
{
    for(const auto& m: members)
    {
        auto field_type = ctx.get_type(
          m->get_type()->get_qualified_name());
        if(m->is_array())
        {
            field_type = ctx.get_array(field_type, 1);
        }

        ctx.add_field(
          struct_type_id,
          m->get_name(),
          field_type);
    }
    ctx.seal_struct(struct_type_id);
}

std::optional<ty::type_id> struct_definition_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    for(const auto& m: members)
    {
        m->type_check(ctx, env);
    }

    return std::nullopt;
}

std::string struct_definition_expression::to_string() const
{
    std::string ret = std::format("Struct(name={}, members=(", name.s);
    if(!members.empty())
    {
        for(std::size_t i = 0; i < members.size() - 1; ++i)
        {
            ret += std::format("{}, ", members[i]->to_string());
        }
        ret += std::format("{}", members.back()->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * struct_anonymous_initializer_expression.
 */

std::unique_ptr<expression> struct_anonymous_initializer_expression::clone() const
{
    return std::make_unique<struct_anonymous_initializer_expression>(*this);
}

void struct_anonymous_initializer_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{initializers};
}

std::unique_ptr<cg::rvalue> struct_anonymous_initializer_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    if(!expr_type.has_value())
    {
        throw cg::codegen_error(
          loc,
          "Anonymous struct initializer has no type.");
    }

    if(initializers.size() != fields.size())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Inconsistent struct initialization: {} initializers, {} fields.",
            initializers.size(),
            fields.size()));
    }

    cg::type struct_type = ctx.lower(expr_type.value());
    ctx.generate_new(struct_type);

    for(std::size_t i = 0; i < initializers.size(); ++i)
    {
        ctx.generate_dup(cg::rvalue{struct_type});

        const auto& field_info = fields[i];
        const auto& initializer = initializers[i];

        auto member_type = ctx.lower(field_info.field_type_id);
        auto initializer_value = initializer->emit_rvalue(ctx, true);

        if(!initializer_value)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned no type.",
                name.s,
                field_info.field_index));    // FIXME Resolve field name.
        }
        if(initializer_value->get_type() != member_type
           && initializer_value->get_type().get_type_kind() == cg::type_kind::null
           && member_type.get_type_kind() != cg::type_kind::str
           && member_type.get_type_kind() != cg::type_kind::ref)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned '{}' (expected '{}').",
                name.s,
                field_info.field_index,    // FIXME Resolve field name.
                ::cg::to_string(initializer_value->get_type().get_type_kind()),
                ::cg::to_string(member_type.get_type_kind())));
        }

        ctx.generate_set_field(
          std::make_unique<cg::field_access_argument>(
            struct_type,
            field_info.field_index));
    }

    return std::make_unique<cg::rvalue>(struct_type);
}

void struct_anonymous_initializer_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    for(auto& it: initializers)
    {
        it->collect_names(ctx);
    }
}

std::optional<ty::type_id> struct_anonymous_initializer_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    // don't check twice, otherwise the fields will be initialized multiple times.
    if(ctx.has_expression_type(*this))
    {
        return ctx.get_expression_type(*this);
    }

    auto struct_type_id = ctx.get_type(get_qualified_name());    // FIXME should use symbol id?
    const auto& struct_info = ctx.get_struct_info(struct_type_id);

    if(initializers.size() != struct_info.fields.size())
    {
        throw ty::type_error(
          name.location,
          std::format(
            "Struct '{}' has {} member(s), but {} are initialized.",
            name.s,
            struct_info.fields.size(),
            initializers.size()));
    }

    for(std::size_t i = 0; i < initializers.size(); ++i)
    {
        const auto& initializer = initializers[i];
        const auto& field = struct_info.fields[i];

        auto initializer_type = initializer->type_check(ctx, env);
        if(!initializer_type.has_value())
        {
            throw ty::type_error(initializer->get_location(), "Initializer has no type.");
        }

        // Either the types match, or the type is a reference types which is set to 'null'.
        if(!ctx.are_types_compatible(field.type, initializer_type.value()))
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct member '{}.{}' has type '{}', but initializer has type '{}'.",
                name.s,
                field.name,
                ctx.to_string(field.type),
                ctx.to_string(initializer_type.value())));
        }

        fields.emplace_back(
          field_info{
            .field_index = i,
            .field_type_id = field.type,
            .struct_type_id = struct_type_id});
    }

    expr_type = struct_type_id;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string struct_anonymous_initializer_expression::to_string() const
{
    std::string ret = std::format("StructAnonymousInitializer(name={}, initializers=(", name.s);
    if(!initializers.empty())
    {
        for(std::size_t i = 0; i < initializers.size() - 1; ++i)
        {
            ret += std::format("{}, ", initializers[i]->to_string());
        }
        ret += std::format("{}", initializers.back()->to_string());
    }
    ret += "))";
    return ret;
}

/*
 * named_initializer.
 */

std::unique_ptr<expression> named_initializer::clone() const
{
    return std::make_unique<named_initializer>(*this);
}

void named_initializer::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
}

std::unique_ptr<cg::rvalue> named_initializer::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    return expr->emit_rvalue(ctx, result_used);
}

void named_initializer::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    expr->collect_names(ctx);
}

std::optional<ty::type_id> named_initializer::type_check(
  ty::context& ctx,
  sema::env& env)
{
    return expr->type_check(ctx, env);
}

std::string named_initializer::to_string() const
{
    return std::format(
      "NamedInitializer(name={}, expr={})",
      get_name(),
      expr->to_string());
}

/*
 * struct_named_initializer_expression.
 */

std::unique_ptr<expression> struct_named_initializer_expression::clone() const
{
    return std::make_unique<struct_named_initializer_expression>(*this);
}

void struct_named_initializer_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{initializers};
}

std::unique_ptr<cg::rvalue> struct_named_initializer_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    if(!expr_type.has_value())
    {
        throw cg::codegen_error(
          loc,
          "Named struct initializer has no type.");
    }

    if(initializers.size() != fields.size())
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Inconsistent struct initialization: {} initializers, {} fields.",
            initializers.size(),
            fields.size()));
    }

    cg::type struct_type = ctx.lower(expr_type.value());
    ctx.generate_new(struct_type);

    for(std::size_t i = 0; i < initializers.size(); ++i)
    {
        ctx.generate_dup(cg::rvalue{struct_type});

        const auto& field_info = fields[i];
        const auto& initializer = initializers[i];

        auto member_type = ctx.lower(field_info.field_type_id);

        ctx.push_declaration_type(member_type);
        auto initializer_value = initializer->emit_rvalue(ctx, true);
        ctx.pop_declaration_type();

        if(!initializer_value)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned no type.",
                name.s,
                field_info.field_index));    // FIXME Resolve field name.
        }
        if(initializer_value->get_type() != member_type
           && initializer_value->get_type().get_type_kind() == cg::type_kind::null
           && member_type.get_type_kind() != cg::type_kind::str
           && member_type.get_type_kind() != cg::type_kind::ref)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Code generation for '{}.{}' initialization returned '{}' (expected '{}').",
                name.s,
                field_info.field_index,    // FIXME Resolve field name.
                ::cg::to_string(initializer_value->get_type().get_type_kind()),
                ::cg::to_string(member_type.get_type_kind())));
        }

        ctx.generate_set_field(
          std::make_unique<cg::field_access_argument>(
            struct_type,
            field_info.field_index));
    }

    return std::make_unique<cg::rvalue>(struct_type);
}

void struct_named_initializer_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    for(auto& it: initializers)
    {
        it->collect_names(ctx);
    }
}

std::optional<ty::type_id> struct_named_initializer_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    // don't check twice, otherwise the fields will be initialized multiple times.
    if(ctx.has_expression_type(*this))
    {
        return ctx.get_expression_type(*this);
    }

    auto struct_type_id = ctx.get_type(get_qualified_name());    // FIXME should use symbol id?
    const ty::struct_info& info = ctx.get_struct_info(struct_type_id);

    if(info.fields.size() != initializers.size())
    {
        throw ty::type_error(
          name.location,
          std::format(
            "Struct '{}' has {} member(s), but {} are initialized.",
            name.s,
            info.fields.size(),
            initializers.size()));
    }

    std::vector<std::string> initialized_member_names;    // used in check for duplicates
    for(const auto& initializer: initializers)
    {
        const auto& member_name = initializer->get_name();

        if(std::ranges::find_if(
             initialized_member_names,
             [&member_name](auto& name) -> bool
             { return name == member_name; })
           != initialized_member_names.end())
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Multiple initializations of struct member '{}::{}'.",
                name.s,
                member_name));
        }
        initialized_member_names.push_back(member_name);

        auto field_it = info.fields_by_name.find(member_name);
        if(field_it == info.fields_by_name.end())
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct '{}' has no member '{}'.",
                name.s,
                member_name));
        }
        if(field_it->second >= info.fields.size())
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Field index for '{}.{}' out of range.",
                name.s,
                member_name));
        }

        auto initializer_type = initializer->type_check(ctx, env);
        if(!initializer_type.has_value())
        {
            throw ty::type_error(
              initializer->get_location(),
              "Initializer has no type.");
        }

        if(!ctx.are_types_compatible(
             info.fields[field_it->second].type,
             initializer_type.value()))
        {
            throw ty::type_error(
              name.location,
              std::format(
                "Struct member '{}.{}' has type '{}', but initializer has type '{}'.",
                name.s,
                member_name,
                ctx.to_string(info.fields[field_it->second].type),
                ctx.to_string(initializer_type.value())));
        }

        fields.emplace_back(
          field_info{
            .field_index = field_it->second,
            .field_type_id = info.fields[field_it->second].type,
            .struct_type_id = struct_type_id});
    }

    expr_type = struct_type_id;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string struct_named_initializer_expression::to_string() const
{
    std::string ret = std::format("StructNamedInitializer(name={}, initializers=(", name.s);

    if(!initializers.empty())
    {
        for(std::size_t i = 0; i < initializers.size() - 1; ++i)
        {
            ret += std::format(
              "name={}, expr={}, ",
              initializers[i]->get_name(),
              initializers[i]->get_expression()->to_string());
        }
        ret += std::format(
          "name={}, expr={}",
          initializers.back()->get_name(),
          initializers.back()->get_expression()->to_string());
    }
    ret += ")";

    return ret;
}

/*
 * assignment_expression.
 */

std::unique_ptr<expression> assignment_expression::clone() const
{
    return std::make_unique<assignment_expression>(*this);
}

void assignment_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & op;
    ar& expression_serializer{lhs};
    ar& expression_serializer{rhs};
}

static const std::unordered_map<std::string, cg::binary_op> binary_op_map = {
  {"*", cg::binary_op::op_mul},
  {"/", cg::binary_op::op_div},
  {"%", cg::binary_op::op_mod},
  {"+", cg::binary_op::op_add},
  {"-", cg::binary_op::op_sub},
  {"<<", cg::binary_op::op_shl},
  {">>", cg::binary_op::op_shr},
  {"<", cg::binary_op::op_less},
  {"<=", cg::binary_op::op_less_equal},
  {">", cg::binary_op::op_greater},
  {">=", cg::binary_op::op_greater_equal},
  {"==", cg::binary_op::op_equal},
  {"!=", cg::binary_op::op_not_equal},
  {"&", cg::binary_op::op_and},
  {"^", cg::binary_op::op_xor},
  {"|", cg::binary_op::op_or},
  {"&&", cg::binary_op::op_logical_and},
  {"||", cg::binary_op::op_logical_or}};

static std::pair<bool, std::string> classify_assignment(const std::string& s)
{
    bool is_compound = (s != "=");

    std::string reduced_op = s;
    if(is_compound)
    {
        reduced_op.pop_back();
    }

    return std::make_pair(
      is_compound,
      std::move(reduced_op));
}

std::unique_ptr<cg::rvalue> assignment_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    // Evaluate constant subexpressions.
    if(std::unique_ptr<cg::rvalue> v;
       (v = try_emit_const_eval_result(ctx)) != nullptr)
    {
        return v;
    }

    std::unique_ptr<cg::rvalue> rhs_value;
    std::unique_ptr<cg::lvalue> lhs_value;

    auto [is_compound, reduced_op] = classify_assignment(op.s);

    lhs_value = lhs->emit_lvalue(ctx);

    if(is_compound)
    {
        if(lhs->is_array_element_access())
        {
            // duplicate array address and index.
            ctx.generate_dup(*lhs_value);
        }

        ctx.generate_load(*lhs_value);

        rhs_value = rhs->emit_rvalue(ctx, true);

        auto it = binary_op_map.find(reduced_op);
        if(it == binary_op_map.end())
        {
            throw std::runtime_error(
              std::format(
                "{}: Code generation for binary operator '{}' not implemented.",
                slang::to_string(loc), op.s));
        }

        ctx.generate_binary_op(it->second, lhs_value->get_type());

        // FIXME Should this go into a desugar-phase for compound assignments?
        if(rhs_value->get_type().get_type_kind() == cg::type_kind::i8)
        {
            ctx.generate_cast(cg::type_cast::i32_to_i8);
        }
        else if(rhs_value->get_type().get_type_kind() == cg::type_kind::i16)
        {
            ctx.generate_cast(cg::type_cast::i32_to_i16);
        }
    }
    else
    {
        rhs_value = rhs->emit_rvalue(ctx, true);
    }

    if(lhs->is_struct_member_access())
    {
        // duplicate the value for chained assignments.
        if(result_used)
        {
            ctx.generate_dup_x1(
              rhs_value->get_type(),
              {cg::type_kind::ref});
        }

        ctx.generate_store(*lhs_value);

        if(result_used)
        {
            return rhs_value;
        }

        return nullptr;
    }

    if(lhs->is_array_element_access())
    {
        // duplicate the value for chained assignments.
        if(result_used)
        {
            ctx.generate_dup_x2(
              rhs_value->get_type(),
              {cg::type_kind::i32},
              {cg::type_kind::ref});
        }

        ctx.generate_store(*lhs_value);

        if(result_used)
        {
            return rhs_value;
        }

        return nullptr;
    }

    /* Case 1. (cont.), 4. (cont.) */
    // we might need to duplicate the value for chained assignments.
    if(result_used)
    {
        ctx.generate_dup(*rhs_value);
    }

    ctx.generate_store(*lhs_value);

    if(result_used)
    {
        return rhs_value;
    }

    return nullptr;
}

void assignment_expression::insert_implicit_casts(
  ty::context& ctx,
  sema::env& env)
{
    if(!ctx.has_expression_type(*rhs.get()))
    {
        // only insert casts if the type is known.
        // the type is unknown inside non-expanded macros.
        return;
    }

    auto rhs_type = ctx.get_expression_type(*rhs.get());
    if(!rhs_type.has_value())
    {
        return;
    }

    if(rhs_type.value() == ctx.get_i8_type()
       || rhs_type.value() == ctx.get_i16_type())
    {
        auto loc = rhs->get_location();    // save location before moving r.h.s.

        rhs = std::make_unique<type_cast_expression>(
          loc,
          std::move(rhs),
          std::make_unique<type_expression>(
            loc,
            token{ctx.to_string(rhs_type.value()), loc},
            std::vector<token>{},
            ctx.is_array(rhs_type.value())),
          true /* always cast */);

        rhs->type_check(ctx, env);    // FIXME should not need to be re-checked.
    }
}

void assignment_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    lhs->collect_names(ctx);
    rhs->collect_names(ctx);
}

std::optional<ty::type_id> assignment_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(!ctx.has_expression_type(*lhs) || !ctx.has_expression_type(*rhs))
    {
        // visit the nodes to get the types. note that if we are here,
        // no type has been set yet, so we can traverse all nodes without
        // doing evaluation twice.
        visit_nodes(
          [&ctx, &env](ast::expression& node)
          {
              node.type_check(ctx, env);
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    auto [is_compound, reduced_op] = classify_assignment(op.s);

    auto lhs_type = ctx.get_expression_type(*lhs);
    auto rhs_type = ctx.get_expression_type(*rhs);

    if(!lhs_type.has_value())
    {
        throw ty::type_error(
          loc,
          "L.h.s. in binary expression does not have a type.");
    }
    if(!rhs_type.has_value())
    {
        throw ty::type_error(
          loc,
          "R.h.s. in binary expression does not have a type.");
    }

    // some operations restrict the type.
    if(reduced_op == "%"
       || reduced_op == "&"
       || reduced_op == "^"
       || reduced_op == "|")
    {
        if((lhs_type != rhs_type
            || (lhs_type != ctx.get_i32_type() && lhs_type != ctx.get_i64_type())))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got binary expression of type '{}' {} '{}', expected 'i32' {} 'i32' or 'i64' {} 'i64'.",
                ctx.to_string(lhs_type.value()),
                reduced_op,
                ctx.to_string(rhs_type.value()),
                reduced_op,
                reduced_op));
        }

        // set the restricted type.
        expr_type = lhs_type;
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    if(reduced_op == "<<" || reduced_op == ">>")
    {
        if((lhs_type != ctx.get_i32_type() && lhs_type != ctx.get_i64_type())
           || rhs_type != ctx.get_i32_type())
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got shift expression of type '{}' {} '{}', expected 'i32' {} 'i32' or 'i64' {} 'i32'.",
                ctx.to_string(lhs_type.value()),
                reduced_op,
                ctx.to_string(rhs_type.value()),
                reduced_op,
                reduced_op));
        }

        // disallow negative literals.
        if(rhs->is_literal())
        {
            const std::optional<const_value>& v = rhs->as_literal()->get_token().value;
            if(!v.has_value())
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "R.h.s. does not have a value, but is typed as 'i32' literal."));
            }

            if(std::get<std::int64_t>(v.value()) < 0)
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "Negative shift counts are not allowed."));
            }
        }

        // set the restricted type.
        expr_type = lhs_type;
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    if(reduced_op == "&&" || reduced_op == "||")
    {
        if((lhs_type != rhs_type
            || (lhs_type != ctx.get_i32_type() && lhs_type != ctx.get_i64_type())))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got logical expression of type '{}' {} '{}', expected 'i32' {} 'i32'.",
                ctx.to_string(lhs_type.value()),
                reduced_op,
                ctx.to_string(rhs_type.value()),
                reduced_op));
        }

        // set the restricted type.
        expr_type = ctx.get_i32_type();
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    // assignments and comparisons.
    if(op.s == "="
       || op.s == "=="
       || op.s == "!=")
    {
        // Either the types match, or the type is a reference types which is set to 'null'.
        if(!ctx.are_types_compatible(lhs_type.value(), rhs_type.value()))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Types don't match in binary expression. Got expression of type '{}' {} '{}'.",
                ctx.to_string(lhs_type.value()),
                reduced_op,
                ctx.to_string(rhs_type.value())));
        }

        if(op.s == "=")
        {
            // assignments return the type of the l.h.s.
            expr_type = lhs_type;
            ctx.set_expression_type(*this, expr_type);
            return expr_type;
        }

        // comparisons return i32.
        expr_type = ctx.get_i32_type();
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    // check lhs and rhs have supported types (i32, i64, f32 and f64).
    if(lhs_type.value() != ctx.get_i8_type()
       && lhs_type.value() != ctx.get_i16_type()
       && lhs_type.value() != ctx.get_i32_type()
       && lhs_type.value() != ctx.get_i64_type()
       && lhs_type.value() != ctx.get_f32_type()
       && lhs_type.value() != ctx.get_f64_type())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected 'i32', 'i64', 'f32' or 'f64' for l.h.s. of binary operation of type '{}', got '{}'.",
            reduced_op,
            ctx.to_string(lhs_type.value())));
    }

    if(rhs_type.value() != ctx.get_i8_type()
       && rhs_type.value() != ctx.get_i16_type()
       && rhs_type.value() != ctx.get_i32_type()
       && rhs_type.value() != ctx.get_i64_type()
       && rhs_type.value() != ctx.get_f32_type()
       && rhs_type.value() != ctx.get_f64_type())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected 'i32', 'i64', 'f32' or 'f64' for r.h.s. of binary operation of type '{}', got '{}'.",
            reduced_op,
            ctx.to_string(rhs_type.value())));
    }

    if(lhs_type != rhs_type)
    {
        throw ty::type_error(
          loc,
          std::format(
            "Types don't match in binary expression. Got expression of type '{}' {} '{}'.",
            ctx.to_string(lhs_type.value()),
            reduced_op,
            ctx.to_string(rhs_type.value())));
    }

    expr_type = lhs_type;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string assignment_expression::to_string() const
{
    return std::format(
      "Assign(op=\"{}\", lhs={}, rhs={})", op.s,
      lhs ? lhs->to_string() : std::string("<none>"),
      rhs ? rhs->to_string() : std::string("<none>"));
}

/*
 * binary_expression.
 */

std::unique_ptr<expression> binary_expression::clone() const
{
    return std::make_unique<binary_expression>(*this);
}

void binary_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & op;
    ar& expression_serializer{lhs};
    ar& expression_serializer{rhs};
}

/**
 * Check whether the binary operator is a comparison.
 *
 * @param s The binary operator.
 * @returns Returns a tuple `(is_comparison, reduced_op)`.
 */
static bool is_comparison(const std::string& s)
{
    return (s == "==" || s == "!=" || s == ">" || s == ">=" || s == "<" || s == "<=");
}

bool binary_expression::is_pure(cg::context& ctx) const
{
    return lhs->is_pure(ctx)
           && rhs->is_pure(ctx);
}

/**
 * Generate the control logic / short-circuit evaluation for _logical and_ operations (&&).
 *
 * @param ctx The code generation context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @returns A value with `i32` type.
 */
static std::unique_ptr<cg::rvalue> generate_logical_and(
  cg::context& ctx,
  const std::unique_ptr<expression>& lhs,
  const std::unique_ptr<expression>& rhs)
{
    std::unique_ptr<cg::rvalue> lhs_value;
    std::unique_ptr<cg::rvalue> rhs_value;

    lhs_value = lhs->emit_rvalue(ctx, true);
    if(!lhs_value)
    {
        throw cg::codegen_error(
          lhs->get_location(),
          "Expression didn't produce a value.");
    }
    if(lhs_value->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          lhs->get_location(),
          std::format(
            "Wrong expression type '{}' for logical and operator. Expected 'i32'.",
            lhs_value->get_type().to_string()));
    }

    ctx.generate_const(cg::type{cg::type_kind::i32}, 0);
    ctx.generate_binary_op(cg::binary_op::op_not_equal, lhs_value->get_type());    // stack: (lhs != 0)

    // store where to insert the branch.
    auto* function_insertion_point = ctx.get_insertion_point(true);

    // set up basic blocks.
    auto* lhs_true_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* lhs_false_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    /*
     * code generation for l.h.s. being true
     */
    ctx.get_current_function(true)->append_basic_block(lhs_true_basic_block);
    ctx.set_insertion_point(lhs_true_basic_block);

    rhs_value = rhs->emit_rvalue(ctx, true);
    if(!rhs_value)
    {
        throw cg::codegen_error(
          rhs->get_location(),
          "Expression didn't produce a value.");
    }
    if(rhs_value->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          rhs->get_location(),
          std::format(
            "Wrong expression type '{}' for logical and operator. Expected 'i32'.",
            lhs_value->get_type().to_string()));
    }

    ctx.generate_const(cg::type{cg::type_kind::i32}, 0);
    ctx.generate_binary_op(cg::binary_op::op_not_equal, lhs_value->get_type());    // stack: ... && (rhs != 0).
    ctx.generate_branch(merge_basic_block);

    /*
     * code generation for l.h.s. being false
     */
    ctx.get_current_function(true)->append_basic_block(lhs_false_basic_block);
    ctx.set_insertion_point(lhs_false_basic_block);
    ctx.generate_const(cg::type{cg::type_kind::i32}, 0);
    ctx.generate_branch(merge_basic_block);

    /*
     * control flow logic.
     */

    // insert blocks into function.
    ctx.set_insertion_point(function_insertion_point);
    ctx.generate_cond_branch(lhs_true_basic_block, lhs_false_basic_block);

    // emit merge block.
    ctx.get_current_function(true)->append_basic_block(merge_basic_block);
    ctx.set_insertion_point(merge_basic_block);

    return std::make_unique<cg::rvalue>(cg::type{cg::type_kind::i32});
}

/**
 * Generate the control logic / short-circuit evaluation for _logical or_ operations (||).
 *
 * @param ctx The code generation context.
 * @param lhs The left-hand side.
 * @param rhs The right-hand side.
 * @returns A value with `i32` type.
 */
static std::unique_ptr<cg::rvalue> generate_logical_or(
  cg::context& ctx,
  const std::unique_ptr<expression>& lhs,
  const std::unique_ptr<expression>& rhs)
{
    std::unique_ptr<cg::rvalue> lhs_value;
    std::unique_ptr<cg::rvalue> rhs_value;

    lhs_value = lhs->emit_rvalue(ctx, true);
    if(!lhs_value)
    {
        throw cg::codegen_error(
          lhs->get_location(),
          "Expression didn't produce a value.");
    }
    if(lhs_value->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          lhs->get_location(),
          std::format(
            "Wrong expression type '{}' for logical and operator. Expected 'i32'.",
            lhs_value->get_type().to_string()));
    }

    ctx.generate_const(cg::type{cg::type_kind::i32}, 0);
    ctx.generate_binary_op(cg::binary_op::op_equal, lhs_value->get_type());    // stack: (lhs != 0)

    // store where to insert the branch.
    auto* function_insertion_point = ctx.get_insertion_point(true);

    // set up basic blocks.
    auto* lhs_false_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* lhs_true_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    /*
     * code generation for l.h.s. being false
     */
    ctx.get_current_function(true)->append_basic_block(lhs_false_basic_block);
    ctx.set_insertion_point(lhs_false_basic_block);

    rhs_value = rhs->emit_rvalue(ctx, true);
    if(!rhs_value)
    {
        throw cg::codegen_error(
          rhs->get_location(),
          "Expression didn't produce a value.");
    }
    if(rhs_value->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          rhs->get_location(),
          std::format(
            "Wrong expression type '{}' for logical and operator. Expected 'i32'.",
            lhs_value->get_type().to_string()));
    }

    ctx.generate_const(cg::type{cg::type_kind::i32}, 0);
    ctx.generate_binary_op(cg::binary_op::op_not_equal, lhs_value->get_type());    // stack: ... || (rhs != 0).
    ctx.generate_branch(merge_basic_block);

    /*
     * code generation for l.h.s. being true
     */
    ctx.get_current_function(true)->append_basic_block(lhs_true_basic_block);
    ctx.set_insertion_point(lhs_true_basic_block);
    ctx.generate_const(cg::type{cg::type_kind::i32}, 1);
    ctx.generate_branch(merge_basic_block);

    /*
     * control flow logic.
     */

    // insert blocks into function.
    ctx.set_insertion_point(function_insertion_point);
    ctx.generate_cond_branch(lhs_false_basic_block, lhs_true_basic_block);

    // emit merge block.
    ctx.get_current_function(true)->append_basic_block(merge_basic_block);
    ctx.set_insertion_point(merge_basic_block);

    return std::make_unique<cg::rvalue>(cg::type{cg::type_kind::i32});
}

std::unique_ptr<cg::rvalue> binary_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    /*
     * Code generation for binary expressions
     * --------------------------------------
     *
     * 0. Special cases for logical and/logical or.
     *
     * 1. Non-assigning binary operation
     *
     *    <l.h.s. load>
     *    <r.h.s. load>
     *    <binary-op>
     */

    std::unique_ptr<cg::rvalue> lhs_value, lhs_store_value, rhs_value;    // NOLINT(readability-isolate-declaration)

    /* Case 0 (logical and/logical or). */
    if(op.s == "&&")
    {
        // TODO Evaluate constant subexpressions

        // Short-circuit evaluation of "lhs && rhs".
        return generate_logical_and(ctx, lhs, rhs);
    }

    if(op.s == "||")
    {
        // TODO Evaluate constant subexpressions

        // Short-circuit evaluation of "lhs || rhs".
        return generate_logical_or(ctx, lhs, rhs);
    }

    // Evaluate constant subexpressions.
    if(std::unique_ptr<cg::rvalue> v;
       (v = try_emit_const_eval_result(ctx)) != nullptr)
    {
        return v;
    }

    lhs_value = lhs->emit_rvalue(ctx, true);
    rhs_value = rhs->emit_rvalue(ctx, true);

    auto it = binary_op_map.find(op.s);
    if(it == binary_op_map.end())
    {
        throw std::runtime_error(
          std::format(
            "{}: Code generation for binary operator '{}' not implemented.",
            slang::to_string(loc), op.s));
    }

    ctx.generate_binary_op(it->second, lhs_value->get_type());

    if(is_comparison(op.s))
    {
        return std::make_unique<cg::rvalue>(cg::type{cg::type_kind::i32});
    }

    // non-assignment operation.
    return lhs_value;
}

void binary_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    lhs->collect_names(ctx);
    rhs->collect_names(ctx);
}

std::optional<ty::type_id> binary_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(!ctx.has_expression_type(*lhs) || !ctx.has_expression_type(*rhs))
    {
        // visit the nodes to get the types. note that if we are here,
        // no type has been set yet, so we can traverse all nodes without
        // doing evaluation twice.
        visit_nodes(
          [&ctx, &env](ast::expression& node)
          {
              node.type_check(ctx, env);
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    if(op.s == ".")    // struct access
    {
        // TODO change or improve error message.
        throw std::runtime_error("Struct access is not handled by binary_expression");
    }

    auto lhs_type = ctx.get_expression_type(*lhs);
    auto rhs_type = ctx.get_expression_type(*rhs);

    if(!lhs_type.has_value())
    {
        throw ty::type_error(
          loc,
          "L.h.s. in binary expression does not have a type.");
    }
    if(!rhs_type.has_value())
    {
        throw ty::type_error(
          loc,
          "R.h.s. in binary expression does not have a type.");
    }

    // some operations restrict the type.
    if(op.s == "%"
       || op.s == "&"
       || op.s == "^"
       || op.s == "|")
    {
        if((lhs_type != rhs_type
            || (lhs_type != ctx.get_i32_type() && lhs_type != ctx.get_i64_type())))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got binary expression of type '{}' {} '{}', expected 'i32' {} 'i32' or 'i64' {} 'i64'.",
                ctx.to_string(lhs_type.value()),
                op.s,
                ctx.to_string(rhs_type.value()),
                op.s,
                op.s));
        }

        // set the restricted type.
        expr_type = lhs_type;
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    if(op.s == "<<" || op.s == ">>")
    {
        if((lhs_type != ctx.get_i32_type() && lhs_type != ctx.get_i64_type())
           || rhs_type != ctx.get_i32_type())
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got shift expression of type '{}' {} '{}', expected 'i32' {} 'i32' or 'i64' {} 'i32'.",
                ctx.to_string(lhs_type.value()),
                op.s,
                ctx.to_string(rhs_type.value()),
                op.s,
                op.s));
        }

        // disallow negative literals.
        if(rhs->is_literal())
        {
            const std::optional<const_value>& v = rhs->as_literal()->get_token().value;
            if(!v.has_value())
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "R.h.s. does not have a value, but is typed as 'i32' literal."));
            }

            if(std::get<std::int64_t>(v.value()) < 0)
            {
                throw ty::type_error(
                  loc,
                  std::format(
                    "Negative shift counts are not allowed."));
            }
        }

        // set the restricted type.
        expr_type = lhs_type;
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    if(op.s == "&&" || op.s == "||")
    {
        if((lhs_type != rhs_type
            || (lhs_type != ctx.get_i32_type() && lhs_type != ctx.get_i64_type())))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Got logical expression of type '{}' {} '{}', expected 'i32' {} 'i32'.",
                ctx.to_string(lhs_type.value()),
                op.s,
                ctx.to_string(rhs_type.value()),
                op.s));
        }

        // set the restricted type.
        expr_type = ctx.get_i32_type();
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    // assignments and comparisons.
    if(op.s == "=="
       || op.s == "!=")
    {
        // Either the types match, or the type is a reference types which is set to 'null'.
        if(!ctx.are_types_compatible(lhs_type.value(), rhs_type.value()))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Types don't match in binary expression. Got expression of type '{}' {} '{}'.",
                ctx.to_string(lhs_type.value()),
                op.s,
                ctx.to_string(rhs_type.value())));
        }

        // comparisons return i32.
        expr_type = ctx.get_i32_type();
        ctx.set_expression_type(*this, expr_type);
        return expr_type;
    }

    // check lhs and rhs have supported types (i32, i64, f32 and f64).
    if(lhs_type.value() != ctx.get_i8_type()
       && lhs_type.value() != ctx.get_i16_type()
       && lhs_type.value() != ctx.get_i32_type()
       && lhs_type.value() != ctx.get_i64_type()
       && lhs_type.value() != ctx.get_f32_type()
       && lhs_type.value() != ctx.get_f64_type())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected 'i32', 'i64', 'f32' or 'f64' for l.h.s. of binary operation of type '{}', got '{}'.",
            op.s,
            ctx.to_string(lhs_type.value())));
    }

    if(rhs_type.value() != ctx.get_i8_type()
       && rhs_type.value() != ctx.get_i16_type()
       && rhs_type.value() != ctx.get_i32_type()
       && rhs_type.value() != ctx.get_i64_type()
       && rhs_type.value() != ctx.get_f32_type()
       && rhs_type.value() != ctx.get_f64_type())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected 'i32', 'i64', 'f32' or 'f64' for r.h.s. of binary operation of type '{}', got '{}'.",
            op.s,
            ctx.to_string(rhs_type.value())));
    }

    if(lhs_type != rhs_type)
    {
        throw ty::type_error(
          loc,
          std::format(
            "Types don't match in binary expression. Got expression of type '{}' {} '{}'.",
            ctx.to_string(lhs_type.value()),
            op.s,
            ctx.to_string(rhs_type.value())));
    }

    if(is_comparison(op.s))
    {
        // comparisons return i32.
        expr_type = ctx.get_i32_type();
    }
    else
    {
        // set the type of the binary expression,
        expr_type = lhs_type;
    }

    ctx.set_expression_type(*this, expr_type);
    return expr_type;
}

std::string binary_expression::to_string() const
{
    return std::format(
      "Binary(op=\"{}\", lhs={}, rhs={})", op.s,
      lhs ? lhs->to_string() : std::string("<none>"),
      rhs ? rhs->to_string() : std::string("<none>"));
}

/*
 * unary_expression.
 */

std::unique_ptr<expression> unary_expression::clone() const
{
    return std::make_unique<unary_expression>(*this);
}

void unary_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & op;
    ar& expression_serializer{operand};
}

bool unary_expression::is_pure(cg::context& ctx) const
{
    return op.s != "++"
           && op.s != "--"
           && operand->is_pure(ctx);
}

/**
 * Add or subtract one to a value on the stack of a given type.
 *
 * @param ctx The codegen context.
 * @param loc Source location of the add.
 * @param ty Type to emit an add for.
 * @param add Whether to add.
 * @throws Throws a `cg::codegen_error` if the type is not one of `i8`, `i16`, `i32`, `i64`, `f32` or `f64`.
 */
static void emit_addsub_one(
  cg::context& ctx,
  source_location loc,
  bool add,
  cg::type ty)
{
    auto kind = ty.get_type_kind();

    if(kind == cg::type_kind::i8
       || kind == cg::type_kind::i16
       || kind == cg::type_kind::i32)
    {
        ctx.generate_const(
          {cg::type_kind::i32},
          add
            ? static_cast<std::int64_t>(1)
            : static_cast<std::int64_t>(-1));
    }
    else if(kind == cg::type_kind::i64)
    {
        ctx.generate_const(
          {cg::type_kind::i64},
          add
            ? static_cast<std::int64_t>(1)
            : static_cast<std::int64_t>(-1));
    }
    else if(kind == cg::type_kind::f32
            || kind == cg::type_kind::f64)
    {
        ctx.generate_const(ty, add ? 1.0 : -1.0);
    }
    else
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Wrong expression type '{}' for prefix operator '++'. Expected 'i32', 'i64', 'f32' or 'f64'.",
            ty.to_string()));
    }

    ctx.generate_binary_op(cg::binary_op::op_add, ty);
}

std::unique_ptr<cg::rvalue> unary_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    if(op.s == "++" || op.s == "--")
    {
        bool increment = op.s == "++";

        auto v = operand->emit_lvalue(ctx);

        std::visit(
          [&v, &ctx, result_used, increment, this](auto const& l) -> void
          {
              using T = std::decay_t<decltype(l)>;

              if constexpr(std::is_same_v<T, cg::variable_location_info>)
              {
                  ctx.generate_load(*v);
                  emit_addsub_one(ctx, loc, increment, v->get_type());
                  if(result_used)
                  {
                      ctx.generate_dup(*v);
                  }
              }
              else if constexpr(std::is_same_v<T, cg::array_location_info>)
              {
                  // stack: [..., array_ref, array_index]
                  ctx.generate_dup2_x0({cg::type_kind::ref}, {cg::type_kind::i32});    // stack: [..., array_ref, array_index, array_ref, array_index]
                  ctx.generate_load(*v);                                               // stack: [..., array_ref, array_index, old_value]
                  emit_addsub_one(ctx, loc, increment, v->get_type());                 // stack: [..., array_ref, array_index, new_value]

                  if(result_used)
                  {
                      ctx.generate_dup_x2(
                        v->get_type(),
                        {cg::type_kind::ref},
                        {cg::type_kind::i32});
                      // stack: [..., new_value, array_ref, array_index, new_value]
                  }
              }
              else if constexpr(std::is_same_v<T, cg::field_location_info>)
              {
                  // stack: [..., struct_ref]
                  ctx.generate_dup({cg::type_kind::ref});    // stack: [..., struct_ref, struct_ref]
                  ctx.generate_load(*v);                     // stack: [..., struct_ref, old_value]

                  emit_addsub_one(ctx, loc, increment, v->get_type());    // stack: [..., struct_ref, new_value]

                  if(result_used)
                  {
                      ctx.generate_dup_x1(
                        v->get_type(),
                        {cg::type_kind::ref});
                      // stack: [..., new_value, struct_ref, new_value]
                  }
              }
              else
              {
                  static_assert(utils::false_type<T>::value, "Unsupported lvalue location type.");
              }
          },
          v->get_location());

        ctx.generate_store(*v);

        if(result_used)
        {
            return std::make_unique<cg::rvalue>(
              v->get_base());
        }

        return nullptr;
    }

    // Evaluate constant subexpressions.
    if(std::unique_ptr<cg::rvalue> v;
       (v = try_emit_const_eval_result(ctx)) != nullptr)
    {
        return v;
    }

    if(op.s == "+")
    {
        return operand->emit_rvalue(ctx, result_used);
    }

    if(op.s == "-")
    {
        auto& instrs = ctx.get_insertion_point()->get_instructions();
        std::size_t pos = instrs.size();
        auto v = operand->emit_rvalue(ctx, true);

        std::vector<std::unique_ptr<cg::argument>> args;
        if(v->get_type().get_type_kind() == cg::type_kind::i8
           || v->get_type().get_type_kind() == cg::type_kind::i16
           || v->get_type().get_type_kind() == cg::type_kind::i32)
        {
            args.emplace_back(
              std::make_unique<cg::const_argument>(
                cg::type_kind::i32,
                static_cast<std::int64_t>(0),
                std::nullopt));
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::i64)
        {
            args.emplace_back(
              std::make_unique<cg::const_argument>(
                cg::type_kind::i64,
                static_cast<std::int64_t>(0),
                std::nullopt));
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f32)
        {
            args.emplace_back(
              std::make_unique<cg::const_argument>(
                cg::type_kind::f32,
                0.0,
                std::nullopt));
        }
        else if(v->get_type().get_type_kind() == cg::type_kind::f64)
        {
            args.emplace_back(
              std::make_unique<cg::const_argument>(
                cg::type_kind::f64,
                0.0,
                std::nullopt));
        }
        else
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Type error for unary operator '-': Expected 'i8', 'i16', 'i32', 'i64', 'f32' or 'f64', got '{}'.",
                v->get_type().to_string()));
        }
        instrs.insert(
          instrs.begin() + pos,    // NOLINT(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
          std::make_unique<cg::instruction>("const", std::move(args)));

        ctx.generate_binary_op(cg::binary_op::op_sub, v->get_type());
        return v;
    }

    if(op.s == "!")
    {
        auto v = operand->emit_rvalue(ctx, true);
        if(v->get_type().get_type_kind() != cg::type_kind::i32
           && v->get_type().get_type_kind() != cg::type_kind::i64)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Type error for unary operator '!': Expected 'i32' or 'i64', got '{}'.",
                v->get_type().to_string()));
        }

        ctx.generate_const({v->get_type().get_type_kind()}, 0);
        ctx.generate_binary_op(cg::binary_op::op_equal, v->get_type());

        return std::make_unique<cg::rvalue>(v->get_type());
    }

    if(op.s == "~")
    {
        auto& instrs = ctx.get_insertion_point()->get_instructions();
        std::size_t pos = instrs.size();

        auto v = operand->emit_rvalue(ctx, true);
        auto constant_type = [this, &v]() -> cg::type_kind
        {
            if(v->get_type().get_type_kind() == cg::type_kind::i8
               || v->get_type().get_type_kind() == cg::type_kind::i16
               || v->get_type().get_type_kind() == cg::type_kind::i32)
            {
                return cg::type_kind::i32;
            }

            if(v->get_type().get_type_kind() == cg::type_kind::i64)
            {
                return cg::type_kind::i64;
            }

            throw cg::codegen_error(
              loc,
              std::format(
                "Type error for unary operator '~': Expected 'i8', 'i16', 'i32' or 'i64', got '{}'.",
                v->get_type().to_string()));
        }();

        std::vector<std::unique_ptr<cg::argument>> args;
        args.emplace_back(
          std::make_unique<cg::const_argument>(
            constant_type,
            static_cast<std::int64_t>(~0),
            std::nullopt));

        instrs.insert(
          instrs.begin() + pos,    // NOLINT(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
          std::make_unique<cg::instruction>(
            "const",
            std::move(args)));

        ctx.generate_binary_op(cg::binary_op::op_xor, constant_type);
        return std::make_unique<cg::rvalue>(constant_type);
    }

    throw std::runtime_error(
      std::format(
        "{}: Code generation for unary operator '{}' not implemented.",
        slang::to_string(loc),
        op.s));
}

void unary_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    operand->collect_names(ctx);
}

std::optional<ty::type_id> unary_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(!ctx.has_expression_type(*operand))
    {
        // visit the nodes to get the types. note that if we are here,
        // no type has been set yet, so we can traverse all nodes without
        // doing evaluation twice.
        visit_nodes(
          [&ctx, &env](ast::expression& node)
          {
              node.type_check(ctx, env);
          },
          false, /* don't visit this node */
          true   /* post-order traversal */
        );
    }

    const std::unordered_map<std::string, std::set<ty::type_id>> valid_operand_types = {
      {"++", {ctx.get_i8_type(), ctx.get_i16_type(), ctx.get_i32_type(), ctx.get_i64_type(), ctx.get_f32_type(), ctx.get_f64_type()}},
      {"--", {ctx.get_i8_type(), ctx.get_i16_type(), ctx.get_i32_type(), ctx.get_i64_type(), ctx.get_f32_type(), ctx.get_f64_type()}},
      {"+", {ctx.get_i8_type(), ctx.get_i16_type(), ctx.get_i32_type(), ctx.get_i64_type(), ctx.get_f32_type(), ctx.get_f64_type()}},
      {"-", {ctx.get_i8_type(), ctx.get_i16_type(), ctx.get_i32_type(), ctx.get_i64_type(), ctx.get_f32_type(), ctx.get_f64_type()}},
      {"!", {ctx.get_i8_type(), ctx.get_i16_type(), ctx.get_i32_type(), ctx.get_i64_type()}},
      {"~", {ctx.get_i8_type(), ctx.get_i16_type(), ctx.get_i32_type(), ctx.get_i64_type()}}};

    auto op_it = valid_operand_types.find(op.s);
    if(op_it == valid_operand_types.end())
    {
        throw ty::type_error(
          op.location,
          std::format(
            "Unknown unary operator '{}'.",
            op.s));
    }

    auto operand_type = ctx.get_expression_type(*operand);
    if(!operand_type.has_value())
    {
        throw ty::type_error(
          operand->get_location(),
          "Expression has no type.");
    }

    auto type_it = std::ranges::find(op_it->second, operand_type.value());
    if(type_it == op_it->second.end())
    {
        throw ty::type_error(
          operand->get_location(),
          std::format(
            "Invalid operand type '{}' for unary operator '{}'.",
            ctx.to_string(operand_type.value()),
            op.s));
    }

    expr_type = operand_type;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string unary_expression::to_string() const
{
    return std::format(
      "Unary(op=\"{}\", operand={})",
      op.s,
      operand ? operand->to_string() : std::string("<none>"));
}

/*
 * new_expression.
 */

std::unique_ptr<expression> new_expression::clone() const
{
    return std::make_unique<new_expression>(*this);
}

void new_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & type_expr;
    ar& expression_serializer{array_length_expr};
}

bool new_expression::is_pure(cg::context& ctx) const
{
    return array_length_expr->is_pure(ctx);
}

std::unique_ptr<cg::rvalue> new_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    cg::type element_type = ctx.lower(type_expr->get_type());
    if(element_type.get_type_kind() == cg::type_kind::void_)
    {
        throw cg::codegen_error(
          loc,
          "Cannot create array with elements of type 'void'.");
    }

    // generate array size.
    std::unique_ptr<cg::rvalue> v = array_length_expr->emit_rvalue(ctx, true);
    if(v->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Expected <integer> as array size, got '{}'.",
            v->get_type().to_string()));
    }

    if(element_type.get_type_kind() == cg::type_kind::i8
       || element_type.get_type_kind() == cg::type_kind::i16
       || element_type.get_type_kind() == cg::type_kind::i32
       || element_type.get_type_kind() == cg::type_kind::i64
       || element_type.get_type_kind() == cg::type_kind::f32
       || element_type.get_type_kind() == cg::type_kind::f64
       || element_type.get_type_kind() == cg::type_kind::str)
    {
        ctx.generate_newarray(element_type);
        return std::make_unique<cg::rvalue>(cg::type{cg::type_kind::ref});
    }

    // custom type.
    ctx.generate_anewarray(element_type);
    return std::make_unique<cg::rvalue>(cg::type_kind::ref);
}

void new_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    array_length_expr->collect_names(ctx);
}

std::optional<ty::type_id> new_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    type_expr->type_check(ctx, env);

    type_expr_id = ctx.get_type(type_expr->get_qualified_name());
    if(!type_expr_id.has_value())
    {
        throw ty::type_error(
          type_expr->get_location(),
          "Unable to resolve type.");
    }
    if(type_expr_id == ctx.get_void_type())
    {
        throw ty::type_error(
          type_expr->get_location(),
          "Cannot use operator new with type 'void'.");
    }

    auto array_length_type = array_length_expr->type_check(ctx, env);
    if(!array_length_type.has_value())
    {
        throw ty::type_error(
          array_length_expr->get_location(),
          "Array size expression has no type.");
    }

    if(array_length_type != ctx.get_i32_type())
    {
        throw ty::type_error(
          array_length_expr->get_location(),
          std::format(
            "Expected array size of type 'i32', got '{}'.",
            ctx.to_string(array_length_type.value())));
    }

    expr_type = ctx.get_array(type_expr_id.value(), 1);
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string new_expression::to_string() const
{
    return std::format(
      "NewExpression(type={}, expr={})",
      type_expr->to_string(), array_length_expr->to_string());
}

/*
 * null_expression.
 */

std::unique_ptr<expression> null_expression::clone() const
{
    return std::make_unique<null_expression>(*this);
}

std::unique_ptr<cg::rvalue> null_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    ctx.generate_const_null();

    return std::make_unique<cg::rvalue>(cg::type{cg::type_kind::null});
}

std::optional<ty::type_id> null_expression::type_check(
  ty::context& ctx,
  [[maybe_unused]] sema::env& env)
{
    expr_type = ctx.get_null_type();
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string null_expression::to_string() const
{
    return "NullExpression()";
}

/*
 * postfix_expression.
 */

std::unique_ptr<expression> postfix_expression::clone() const
{
    return std::make_unique<postfix_expression>(*this);
}

void postfix_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
    ar & op;
}

std::unique_ptr<cg::rvalue> postfix_expression::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    auto v = expr->emit_lvalue(ctx);
    if(result_used)
    {
        std::visit(
          [&v, &ctx](auto const& l) -> void
          {
              using T = std::decay_t<decltype(l)>;

              if constexpr(std::is_same_v<T, cg::variable_location_info>)
              {
                  ctx.generate_load(*v);
                  ctx.generate_dup(*v);
              }
              else if constexpr(std::is_same_v<T, cg::array_location_info>)
              {
                  // stack: [..., array_ref, array_index]
                  ctx.generate_dup2_x0(
                    {cg::type_kind::ref},
                    {cg::type_kind::i32});    // stack: [..., array_ref, array_index, array_ref, array_index]
                  ctx.generate_load(*v);      // stack: [..., array_ref, array_index, value]
                  ctx.generate_dup_x2(
                    v->get_type(),
                    {cg::type_kind::ref},
                    {cg::type_kind::i32});    // stack: [..., value, array_ref, array_index, value]
              }
              else if constexpr(std::is_same_v<T, cg::field_location_info>)
              {
                  // stack: [..., struct_ref]
                  ctx.generate_dup(
                    {cg::type_kind::ref});    // stack: [..., struct_ref, struct_ref]
                  ctx.generate_load(*v);      // stack: [..., struct_ref, value]
                  ctx.generate_dup_x1(
                    v->get_type(),
                    {cg::type_kind::ref});    // stack: [..., value, struct_ref, value]
              }
              else
              {
                  static_assert(utils::false_type<T>::value, "Unsupported lvalue location type.");
              }
          },
          v->get_location());
    }
    else
    {
        ctx.generate_load(*v);
    }

    if(op.s == "++"
       || op.s == "--")
    {
        auto type_kind = v->get_type().get_type_kind();
        if(type_kind == cg::type_kind::i8
           || type_kind == cg::type_kind::i16
           || type_kind == cg::type_kind::i32)
        {
            ctx.generate_const(
              {cg::type_kind::i32},
              static_cast<std::int64_t>(1));
        }
        else if(type_kind == cg::type_kind::i64)
        {
            ctx.generate_const(
              {cg::type_kind::i64},
              static_cast<std::int64_t>(1));
        }
        else if(type_kind == cg::type_kind::f32
                || type_kind == cg::type_kind::f64)
        {
            ctx.generate_const(v->get_type(), 1.0);
        }
        else
        {
            throw cg::codegen_error(
              op.location,
              std::format(
                "Unknown variable type for postfix operator '{}'.",
                op.s));
        }

        if(op.s == "++")
        {
            ctx.generate_binary_op(cg::binary_op::op_add, v->get_type());
        }
        else
        {
            ctx.generate_binary_op(cg::binary_op::op_sub, v->get_type());
        }

        ctx.generate_store(*v);
    }
    else
    {
        throw cg::codegen_error(
          op.location,
          std::format(
            "Unknown postfix operator '{}'.",
            op.s));
    }

    if(result_used)
    {
        return std::make_unique<cg::rvalue>(
          v->get_base());
    }

    return nullptr;
}

void postfix_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    expr->collect_names(ctx);
}

std::optional<ty::type_id> postfix_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    auto identifier_type = expr->type_check(ctx, env);
    if(!identifier_type.has_value())
    {
        throw ty::type_error(
          expr->get_location(),
          "Identifier has no type.");
    }

    if(identifier_type.value() != ctx.get_i8_type()
       && identifier_type.value() != ctx.get_i16_type()
       && identifier_type.value() != ctx.get_i32_type()
       && identifier_type.value() != ctx.get_i64_type()
       && identifier_type.value() != ctx.get_f32_type()
       && identifier_type.value() != ctx.get_f64_type())
    {
        throw ty::type_error(
          expr->get_location(),
          std::format(
            "Postfix operator '{}' can only operate on 'i8', 'i16', 'i32', 'i64', 'f32' or 'f64' (found '{}').",
            op.s,
            ctx.to_string(identifier_type.value())));
    }

    expr_type = identifier_type;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string postfix_expression::to_string() const
{
    return std::format("Postfix(expr={}, op=\"{}\")", expr->to_string(), op.s);
}

/*
 * prototype.
 */

std::unique_ptr<prototype_ast> prototype_ast::clone() const
{
    return std::make_unique<prototype_ast>(*this);
}

void prototype_ast::serialize(archive& ar)
{
    ar & loc;
    ar & name;
    ar & args;
    ar & return_type;
    ar & arg_type_ids;
    ar & return_type_id;
}

void prototype_ast::collect_names(co::context& ctx)
{
    for(auto& arg: args)
    {
        const std::string canonical_name = name::qualified_name(
          ctx.get_canonical_scope_name(ctx.get_current_scope()),
          std::get<0>(arg).s);

        std::get<1>(arg) = ctx.declare(
          std::get<0>(arg).s,
          canonical_name,
          sema::symbol_type::variable,
          std::get<0>(arg).location,
          sema::symbol_id::invalid,
          false);
    }
}

std::string prototype_ast::to_string() const
{
    std::string ret_type_str = return_type->to_string();
    std::string ret = std::format("Prototype(name={}, return_type={}, args=(", name.s, ret_type_str);
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            std::string arg_type_str = std::get<2>(args[i])->to_string();
            ret += std::format("(name={}, type={}), ", std::get<0>(args[i]).s, arg_type_str);
        }
        std::string arg_type_str = std::get<2>(args.back())->to_string();
        ret += std::format("(name={}, type={})", std::get<0>(args.back()).s, arg_type_str);
    }
    ret += "))";
    return ret;
}

void prototype_ast::declare(ty::context& ctx, sema::env& env)
{
    arg_type_ids.clear();
    arg_type_ids.reserve(args.size());

    for(const auto& arg: args)
    {
        auto type = ctx.get_type(
          std::get<2>(arg)->get_qualified_name());
        if(std::get<2>(arg)->is_array())
        {
            type = ctx.get_array(type, 1);
        }

        arg_type_ids.emplace_back(type);
        env.type_map.insert({std::get<1>(arg), type});
    }

    return_type_id = ctx.get_type(return_type->get_qualified_name());
    if(return_type->is_array())
    {
        return_type_id = ctx.get_array(return_type_id, 1);
    }
}

std::vector<std::pair<sema::symbol_id, ty::type_id>> prototype_ast::get_arg_infos() const
{
    // FIXME Likely not the nicest way to do this.
    return std::views::zip(
             args
               | std::views::transform(
                 [](const auto& arg) -> sema::symbol_id
                 {
                     return std::get<1>(arg);
                 })
               | std::ranges::to<std::vector>(),
             get_arg_type_ids())
           | std::views::transform(
             [](const auto& t) -> std::pair<sema::symbol_id, ty::type_id>
             {
                 return std::make_pair(std::get<0>(t), std::get<1>(t));
             })
           | std::ranges::to<std::vector>();
}

/*
 * block.
 */

std::unique_ptr<expression> block::clone() const
{
    return std::make_unique<block>(*this);
}

void block::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_vector_serializer{exprs};
}

bool block::is_pure(cg::context& ctx) const
{
    return std::ranges::all_of(
      exprs,
      [&ctx](const auto& e) -> bool
      {
          return e->is_pure(ctx);
      });
}

void block::generate_code(
  cg::context& ctx) const
{
    bool was_terminated = false;

    for(const auto& expr: exprs)
    {
        if(was_terminated)
        {
            auto* fn = ctx.get_current_function();
            if(fn != nullptr)
            {
                cg::basic_block* bb = cg::basic_block::create(ctx, ctx.generate_label());
                fn->append_basic_block(bb);
                ctx.set_insertion_point(bb);
            }
        }

        if(expr->is_pure(ctx))
        {
            std::println("{}: Expression has no effect.", ::slang::to_string(expr->get_location()));

            // don't generate code.
            continue;
        }

        expr->generate_code(ctx);

        auto* bb = ctx.get_insertion_point();
        if(ctx.get_current_function() != nullptr
           && bb != nullptr)
        {
            was_terminated = bb->is_terminated();
        }
        else
        {
            was_terminated = false;
        }
    }
}

std::unique_ptr<cg::rvalue> block::emit_rvalue(
  cg::context& ctx,
  bool result_used) const
{
    if(exprs.size() == 0)
    {
        return nullptr;
    }

    bool was_terminated = false;
    for(std::size_t i = 0; i < exprs.size() - 1; ++i)
    {
        const auto& expr = exprs[i];
        if(was_terminated)
        {
            auto* fn = ctx.get_current_function();
            if(fn != nullptr)
            {
                cg::basic_block* bb = cg::basic_block::create(ctx, ctx.generate_label());
                fn->append_basic_block(bb);
                ctx.set_insertion_point(bb);
            }
        }

        if(expr->is_pure(ctx))
        {
            std::println("{}: Expression has no effect.", ::slang::to_string(expr->get_location()));

            // don't generate code.
            continue;
        }

        expr->generate_code(ctx);

        auto* bb = ctx.get_insertion_point();
        if(ctx.get_current_function() != nullptr
           && bb != nullptr)
        {
            was_terminated = bb->is_terminated();
        }
        else
        {
            was_terminated = false;
        }
    }

    // the last expression is loaded if it is an expression.

    if(was_terminated)
    {
        auto* fn = ctx.get_current_function();
        if(fn != nullptr)
        {
            cg::basic_block* bb = cg::basic_block::create(ctx, ctx.generate_label());
            fn->append_basic_block(bb);
            ctx.set_insertion_point(bb);
        }
    }

    const auto& last_expr = exprs.back();
    if(result_used)
    {
        return last_expr->emit_rvalue(ctx, true);
    }

    last_expr->generate_code(ctx);
    return nullptr;
}

void block::collect_names(co::context& ctx)
{
    collect_names(ctx, true);
}

void block::collect_names(co::context& ctx, bool push_anonymous_scope)
{
    super::collect_names(ctx);

    if(push_anonymous_scope)
    {
        ctx.push_scope(std::nullopt, loc);
    }

    for(const auto& expr: exprs)
    {
        expr->collect_names(ctx);
    }

    if(push_anonymous_scope)
    {
        ctx.pop_scope();
    }
}

std::optional<ty::type_id> block::type_check(
  ty::context& ctx,
  sema::env& env)
{
    for(auto& expr: exprs | std::views::take(exprs.size() - 1))
    {
        expr->type_check(ctx, env);
    }

    expr_type = exprs.empty() ? std::nullopt : exprs.back()->type_check(ctx, env);
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string block::to_string() const
{
    std::string ret = "Block(exprs=(";
    if(!exprs.empty())
    {
        for(std::size_t i = 0; i < exprs.size() - 1; ++i)
        {
            ret += std::format("{}, ", exprs[i] ? exprs[i]->to_string() : std::string("<none>"));
        }
        ret += std::format("{}", exprs.back() ? exprs.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

/*
 * function_expression.
 */

std::unique_ptr<expression> function_expression::clone() const
{
    return std::make_unique<function_expression>(*this);
}

void function_expression::serialize(archive& ar)
{
    super::serialize(ar);
    bool has_prototype = prototype != nullptr;
    ar & has_prototype;
    if(has_prototype)
    {
        if(prototype == nullptr)
        {
            prototype = std::make_unique<prototype_ast>();
        }
        prototype->serialize(ar);
    }
    else
    {
        prototype = {};
    }
    ar& expression_serializer{body};
}

void function_expression::generate_code(
  cg::context& ctx) const
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Function '{}' has no symbol id.",
            prototype->get_name()));
    }

    if(!ctx.get_sema_env().has_attribute(
         symbol_id.value(),
         attribs::attribute_kind::native))
    {
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args =
            prototype->get_arg_infos()
            | std::views::transform(
              [&ctx](const std::pair<sema::symbol_id, ty::type_id>& p)
                -> std::pair<sema::symbol_id, cg::type>
              {
                  return std::make_pair(
                    std::get<0>(p),
                    ctx.lower(std::get<1>(p)));
              })
            | std::ranges::to<std::vector>();

        auto ret_type = ctx.lower(prototype->get_return_type_id());

        cg::function* fn = ctx.create_function(
          prototype->get_name(),
          ret_type,
          std::move(args));

        cg::function_guard fg{ctx, fn};

        cg::basic_block* bb = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(bb);

        ctx.set_insertion_point(bb);

        if(!body)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "No function body defined for '{}'.",
                prototype->get_name()));
        }

        auto v = body->emit_rvalue(ctx);

        // verify that the break-continue-stack is empty.
        if(ctx.get_break_continue_stack_size() != 0)
        {
            throw cg::codegen_error(
              loc,
              "Internal error: Break-continue stack is not empty.");
        }

        auto* ip = ctx.get_insertion_point(true);
        if(!ip->ends_with_return())
        {
            // for `void` return types, we insert a return instruction. otherwise, the
            // return statement is missing and we throw an error.
            auto ret_type = std::get<0>(fn->get_signature());
            if(ret_type.get_type_kind() == cg::type_kind::void_)
            {
                // pop the stack if needed.
                if(v != nullptr
                   && v->get_type().get_type_kind() != cg::type_kind::void_)
                {
                    ctx.generate_pop(v->get_type());
                }
            }
            else
            {
                throw cg::codegen_error(
                  loc,
                  std::format(
                    "Missing return statement in function '{}'.",
                    fn->get_name()));
            }

            ctx.generate_ret();
        }
    }
    else
    {
        auto native_payload = ctx.get_sema_env().get_attribute_payload(    // checked above // NOLINT(bugprone-unchecked-optional-access)
                                                  symbol_id.value(),
                                                  attribs::attribute_kind::native)
                                .value();

        auto key_value_pairs = std::get<std::vector<std::pair<std::string, std::string>>>(native_payload);

        if(std::ranges::count_if(
             key_value_pairs,
             [](const std::pair<std::string, std::string&>& p) -> bool
             {
                 return p.first == "lib";
             })
           != 1)
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Native function '{}': Expected single argument for 'lib' attribute.",
                prototype->get_name()));
        }

        std::string lib_name =
          std::ranges::find_if(
            key_value_pairs,
            [](const std::pair<std::string, std::string>& p) -> bool
            {
                return p.first == "lib";
            })
            ->second;

        // Library name might be in quotation marks.
        if(lib_name[0] == '\"'
           && lib_name.back() == '\"')
        {
            lib_name = lib_name.substr(1, lib_name.size() - 2);
        }
        if(lib_name.empty())
        {
            throw cg::codegen_error(
              loc,
              std::format(
                "Native function '{}': Invalid libary name in 'lib' attribute.",
                prototype->get_name()));
        }

        /*
         * Generate binding.
         */

        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args =
            prototype->get_arg_infos()
            | std::views::transform(
              [&ctx](const std::pair<sema::symbol_id, ty::type_id>& p)
                -> std::pair<sema::symbol_id, cg::type>
              {
                  return std::make_pair(
                    std::get<0>(p),
                    ctx.lower(p.second));
              })
            | std::ranges::to<std::vector>();

        auto ret_type = ctx.lower(prototype->get_return_type_id());

        ctx.create_native_function(
          lib_name,
          prototype->get_name(),
          ret_type,
          std::move(args));
    }
}

void function_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    const auto& name = prototype->get_name_token();
    const std::string canonical_name = name::qualified_name(
      ctx.get_canonical_scope_name(ctx.get_current_scope()),
      name.s);

    symbol_id = ctx.declare(
      name.s,
      canonical_name,
      sema::symbol_type::function,
      name.location,
      sema::symbol_id::invalid,
      false,
      this);

    ctx.push_scope(std::format("{}@function", name.s), name.location);
    prototype->collect_names(ctx);

    if(body != nullptr)
    {
        body->collect_names(ctx, false);
    }

    ctx.pop_scope();
}

std::optional<ty::type_id> function_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(body)
    {
        env.current_function_return_type = prototype->get_return_type_id();
        env.current_function_name = prototype->get_name();

        body->type_check(ctx, env);

        env.current_function_name = std::nullopt;
        env.current_function_return_type = std::nullopt;
    }

    return std::nullopt;
}

std::string function_expression::to_string() const
{
    return std::format(
      "Function(prototype={}, body={})",
      prototype ? prototype->to_string() : std::string("<none>"),
      body ? body->to_string() : std::string("<none>"));
}

void function_expression::declare_function(ty::context& ctx, sema::env& env)
{
    prototype->declare(ctx, env);
}

/*
 * call_expression.
 */

std::unique_ptr<expression> call_expression::clone() const
{
    return std::make_unique<call_expression>(*this);
}

void call_expression::serialize(archive& ar)
{
    super::serialize(ar);
    ar & callee;
    ar& expression_vector_serializer{args};
    ar& expression_serializer{index_expr};
    ar & return_type;
}

bool call_expression::is_pure([[maybe_unused]] cg::context& ctx) const
{
    // TODO Check in context. Functions from the current module can be checked,
    //      imported functions and native functions should be seen as impure.
    return false;
}

std::unique_ptr<cg::rvalue> call_expression::emit_rvalue(
  cg::context& ctx,
  [[maybe_unused]] bool result_used) const
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Callee '{}' has no symbol id.",
            callee.s));
    }

    for(const auto& arg: args)
    {
        arg->emit_rvalue(ctx, true);
    }
    ctx.generate_invoke(
      cg::function_argument{
        symbol_id.value()});

    auto lowered_return_type = ctx.lower(return_type);

    if(index_expr)
    {
        auto type = ctx.deref(lowered_return_type);

        // evaluate the index expression.
        index_expr->emit_rvalue(ctx, true);
        ctx.generate_load(
          cg::lvalue{
            type,
            cg::array_location_info{}});

        return std::make_unique<cg::rvalue>(
          type);
    }

    return std::make_unique<cg::rvalue>(lowered_return_type);
}

void call_expression::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    for(auto& arg: args)
    {
        arg->collect_names(ctx);
    }

    if(index_expr)
    {
        index_expr->collect_names(ctx);
    }
}

std::optional<ty::type_id> call_expression::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(!symbol_id.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Function '{}' has no symbol id.",
            callee.s));
    }

    const auto& callee_symbol_info = env.symbol_table[symbol_id.value()];
    if(callee_symbol_info.type != sema::symbol_type::function)
    {
        throw ty::type_error(
          loc,
          "Expected function call.");
    }

    if(!callee_symbol_info.reference.has_value())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Function '{}' has no reference.",
            callee.s));
    }

    if(callee_symbol_info.declaring_module == sema::symbol_info::current_module_id)
    {
        const auto* ast_node = std::get<const ast::expression*>(callee_symbol_info.reference.value());
        if(ast_node->get_id() != ast::node_identifier::function_expression)
        {
            throw ty::type_error(
              loc,
              std::format(
                "AST node for '{}' is not a function node.",
                callee.s));
        }

        const auto* function_node = static_cast<const ast::function_expression*>(ast_node);    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        return_type = function_node->get_return_type_id();
        auto arg_type_ids = function_node->get_arg_type_ids();

        if(arg_type_ids.size() != args.size())
        {
            throw ty::type_error(
              callee.location,
              std::format(
                "Wrong number of arguments in function call. Expected {}, got {}.",
                arg_type_ids.size(),
                args.size()));
        }

        for(std::size_t i = 0; i < args.size(); ++i)
        {
            auto arg_type = args[i]->type_check(ctx, env);
            if(!arg_type.has_value())
            {
                throw ty::type_error(
                  args[i]->get_location(),
                  "Argument does not have a type.");
            }
            if(!ctx.are_types_compatible(arg_type_ids[i], arg_type.value()))
            {
                throw ty::type_error(
                  args[i]->get_location(),
                  std::format(
                    "Type of argument {} does not match signature: Expected '{}', got '{}'.",
                    i + 1,
                    ctx.to_string(arg_type_ids[i]),
                    ctx.to_string(arg_type.value())));
            }
        }
    }
    else
    {
        // FIXME The types here should already be resolved.

        const auto* exp_sym = std::get<const module_::exported_symbol*>(callee_symbol_info.reference.value());
        if(exp_sym == nullptr)
        {
            throw ty::type_error(
              loc,
              std::format(
                "Missing export entry for function '{}'.",
                callee.s));
        }
        if(exp_sym->type != module_::symbol_type::function)
        {
            throw ty::type_error(
              loc,
              std::format(
                "Exported symbol is not a function (got type '{}').",
                module_::to_string(exp_sym->type)));
        }

        auto desc = std::get<module_::function_descriptor>(exp_sym->desc);

        std::string type_str = desc.signature.return_type.base_type();
        if(type_str.find("::") == std::string::npos)
        {
            // Could be a built-in type or a local type.
            if(!ctx.has_type(type_str)
               || !ctx.is_builtin(ctx.get_type(type_str)))
            {
                auto module_symbol_it = env.symbol_table.find(callee_symbol_info.declaring_module);
                if(module_symbol_it == env.symbol_table.end())
                {
                    throw ty::type_error(
                      loc,
                      std::format(
                        "Declaring module for external symbol '{}' not found.",
                        callee_symbol_info.qualified_name));
                }

                type_str = name::qualified_name(
                  env.symbol_table[callee_symbol_info.declaring_module].qualified_name,
                  type_str);
            }
        }

        return_type = ctx.get_type(type_str);
        if(desc.signature.return_type.is_array())
        {
            return_type = ctx.get_array(return_type, 1);
        }

        if(desc.signature.arg_types.size() != args.size())
        {
            throw ty::type_error(
              callee.location,
              std::format(
                "Wrong number of arguments in function call. Expected {}, got {}.",
                desc.signature.arg_types.size(),
                args.size()));
        }

        for(std::size_t i = 0; i < args.size(); ++i)
        {
            std::string type_str = desc.signature.arg_types[i].base_type();
            if(type_str.find("::") == std::string::npos)
            {
                // Could be a built-in type or a local type.
                if(!ctx.has_type(type_str)
                   || !ctx.is_builtin(ctx.get_type(type_str)))
                {
                    type_str = name::qualified_name(
                      env.symbol_table[callee_symbol_info.declaring_module].qualified_name,
                      type_str);
                }
            }

            auto expected_arg_type = ctx.get_type(type_str);
            if(desc.signature.arg_types[i].is_array())
            {
                expected_arg_type = ctx.get_array(expected_arg_type, 1);
            }

            auto arg_type = args[i]->type_check(ctx, env);
            if(!arg_type.has_value())
            {
                throw ty::type_error(
                  args[i]->get_location(),
                  "Argument does not have a type.");
            }
            if(!ctx.are_types_compatible(expected_arg_type, arg_type.value()))
            {
                throw ty::type_error(
                  args[i]->get_location(),
                  std::format(
                    "Type of argument {} does not match signature: Expected '{}', got '{}'.",
                    i + 1,
                    ctx.to_string(expected_arg_type),
                    ctx.to_string(arg_type.value())));
            }
        }
    }

    if(index_expr)
    {
        auto v = index_expr->type_check(ctx, env);
        if(!v.has_value())
        {
            throw ty::type_error(
              index_expr->get_location(),
              "Index expression has no type.");
        }
        if(!ctx.are_types_compatible(ctx.get_i32_type(), v.value()))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Expected <integer> for array element access, got '{}'.",
                ctx.to_string(v.value())));
        }

        if(!ctx.is_array(return_type))
        {
            throw ty::type_error(loc, "Cannot use subscript on non-array type.");
        }

        expr_type = ctx.array_element_type(return_type);
    }
    else
    {
        expr_type = return_type;
    }

    is_void_return_type = expr_type.value() == ctx.get_void_type();
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string call_expression::to_string() const
{
    std::string ret = std::format("Call(callee={}, args=(", callee.s);
    if(!args.empty())
    {
        for(std::size_t i = 0; i < args.size() - 1; ++i)
        {
            ret += std::format("{}, ", args[i] ? args[i]->to_string() : std::string("<none>"));
        }
        ret += std::format("{}", args.back() ? args.back()->to_string() : std::string("<none>"));
    }
    ret += "))";
    return ret;
}

std::string call_expression::get_qualified_callee_name() const
{
    std::optional<std::string> path = get_namespace_path();
    if(!path.has_value())
    {
        return callee.s;
    }
    return name::qualified_name(
      path.value(),
      callee.s);
}

/*
 * return_statement.
 */

std::unique_ptr<expression> return_statement::clone() const
{
    return std::make_unique<return_statement>(*this);
}

void return_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{expr};
}

void return_statement::generate_code(
  cg::context& ctx) const
{
    if(expr)
    {
        // Evaluate constant subexpressions.
        if(std::unique_ptr<cg::rvalue> v;
           (v = expr->try_emit_const_eval_result(ctx)) != nullptr)
        {
            ctx.generate_ret(v->get_type());
            return;
        }

        auto v = expr->emit_rvalue(ctx, true);
        ctx.generate_ret(v->get_type());
    }
    else
    {
        ctx.generate_ret();
    }
}

void return_statement::collect_names(co::context& ctx)
{
    super::collect_names(ctx);

    if(expr)
    {
        expr->collect_names(ctx);
    }
}

std::optional<ty::type_id> return_statement::type_check(
  ty::context& ctx,
  sema::env& env)
{
    if(!env.current_function_name.has_value())
    {
        throw ty::type_error(
          loc,
          "Current function has no name.");
    }
    if(!env.current_function_return_type.has_value())
    {
        throw ty::type_error(
          loc,
          "Cannot have a return statement outside a function");
    }

    if(env.current_function_return_type.value() == ctx.get_void_type())
    {
        if(expr)
        {
            throw ty::type_error(
              loc,
              std::format(
                "Function '{}' declared as having 'void' return type cannot have a return expression.",
                env.current_function_name.value()));
        }
    }
    else
    {
        auto ret_type = expr->type_check(ctx, env);
        if(!ret_type.has_value())
        {
            throw ty::type_error(loc, "Return expression has no type.");
        }

        if(!ctx.are_types_compatible(
             env.current_function_return_type.value(),
             ret_type.value()))
        {
            throw ty::type_error(
              loc,
              std::format(
                "Function '{}': Return expression has type '{}', expected '{}'.",
                env.current_function_name.value(),
                ctx.to_string(ret_type.value()),
                ctx.to_string(env.current_function_return_type.value())));
        }
    }

    expr_type = std::nullopt;
    ctx.set_expression_type(*this, expr_type);

    return expr_type;
}

std::string return_statement::to_string() const
{
    if(expr)
    {
        return std::format("Return(expr={})", expr->to_string());
    }

    return "Return()";
}

/*
 * if_statement.
 */

std::unique_ptr<expression> if_statement::clone() const
{
    return std::make_unique<if_statement>(*this);
}

void if_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{condition};
    ar& expression_serializer{if_block};
    ar& expression_serializer{else_block};
}

void if_statement::generate_code(
  cg::context& ctx) const
{
    auto v = condition->emit_rvalue(ctx, true);
    if(v->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Expected if condition to be of type 'i32', got '{}",
            v->get_type().to_string()));
    }

    // store where to insert the branch.
    auto* function_insertion_point = ctx.get_insertion_point(true);

    // set up basic blocks.
    auto* if_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    cg::basic_block* else_basic_block = nullptr;
    auto* merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    bool if_ends_with_return = false;
    bool else_ends_with_return = false;

    // code generation for if block.
    ctx.get_current_function(true)->append_basic_block(if_basic_block);
    ctx.set_insertion_point(if_basic_block);
    if_block->generate_code(ctx);
    if_ends_with_return = if_basic_block->ends_with_return();
    if(!if_ends_with_return)
    {
        ctx.generate_branch(merge_basic_block);
    }

    // code generation for optional else block.
    if(!else_block)
    {
        ctx.set_insertion_point(function_insertion_point);
        ctx.generate_cond_branch(if_basic_block, merge_basic_block);
    }
    else
    {
        else_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
        ctx.get_current_function(true)->append_basic_block(else_basic_block);
        ctx.set_insertion_point(else_basic_block);
        else_block->generate_code(ctx);
        else_ends_with_return = else_basic_block->ends_with_return();
        if(!else_ends_with_return)
        {
            ctx.generate_branch(merge_basic_block);
        }

        ctx.set_insertion_point(function_insertion_point);
        ctx.generate_cond_branch(if_basic_block, else_basic_block);
    }

    // emit merge block.
    if(!if_ends_with_return || !else_ends_with_return)
    {
        ctx.get_current_function(true)->append_basic_block(merge_basic_block);
        ctx.set_insertion_point(merge_basic_block);
    }
    else
    {
        // pick the last of the if/else blocks.
        if(else_block)
        {
            ctx.set_insertion_point(else_basic_block);
        }
        else
        {
            ctx.set_insertion_point(if_basic_block);
        }
    }
}

void if_statement::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    condition->collect_names(ctx);
    if_block->collect_names(ctx);
    if(else_block)
    {
        else_block->collect_names(ctx);
    }
}

std::optional<ty::type_id> if_statement::type_check(
  ty::context& ctx,
  sema::env& env)
{
    auto condition_type = condition->type_check(ctx, env);
    if(!condition_type.has_value())
    {
        throw ty::type_error(condition->get_location(), "Condition has no type.");
    }

    if(condition_type.value() != ctx.get_i32_type())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected if condition to be of type 'i32', got '{}",
            ctx.to_string(condition_type.value())));
    }

    if_block->type_check(ctx, env);

    if(else_block)
    {
        else_block->type_check(ctx, env);
    }

    return std::nullopt;
}

std::string if_statement::to_string() const
{
    return std::format(
      "If(condition={}, if_block={}, else_block={})",
      condition ? condition->to_string() : std::string("<none>"),
      if_block ? if_block->to_string() : std::string("<none>"),
      else_block ? else_block->to_string() : std::string("<none>"));
}

/*
 * while_statement.
 */

std::unique_ptr<expression> while_statement::clone() const
{
    return std::make_unique<while_statement>(*this);
}

void while_statement::serialize(archive& ar)
{
    super::serialize(ar);
    ar& expression_serializer{condition};
    ar& expression_serializer{while_block};
}

void while_statement::generate_code(
  cg::context& ctx) const
{
    // set up basic blocks.
    auto* while_loop_header_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* while_loop_basic_block = cg::basic_block::create(ctx, ctx.generate_label());
    auto* merge_basic_block = cg::basic_block::create(ctx, ctx.generate_label());

    // while loop header.
    ctx.get_current_function(true)->append_basic_block(while_loop_header_basic_block);
    ctx.set_insertion_point(while_loop_header_basic_block);

    ctx.push_break_continue({merge_basic_block, while_loop_header_basic_block});

    auto v = condition->emit_rvalue(ctx, true);
    if(v->get_type().get_type_kind() != cg::type_kind::i32)
    {
        throw cg::codegen_error(
          loc,
          std::format(
            "Expected while condition to be of type 'i32', got '{}'.",
            v->get_type().to_string()));
    }

    ctx.generate_cond_branch(while_loop_basic_block, merge_basic_block);

    // while loop body.
    ctx.get_current_function(true)->append_basic_block(while_loop_basic_block);
    ctx.set_insertion_point(while_loop_basic_block);
    while_block->generate_code(ctx);

    ctx.set_insertion_point(ctx.get_current_function(true)->get_basic_blocks().back());
    ctx.generate_branch(while_loop_header_basic_block);

    ctx.pop_break_continue(loc);

    // emit merge block.
    ctx.get_current_function(true)->append_basic_block(merge_basic_block);
    ctx.set_insertion_point(merge_basic_block);
}

void while_statement::collect_names(co::context& ctx)
{
    super::collect_names(ctx);
    condition->collect_names(ctx);
    while_block->collect_names(ctx);
}

std::optional<ty::type_id> while_statement::type_check(
  ty::context& ctx,
  sema::env& env)
{
    auto condition_type = condition->type_check(ctx, env);
    if(!condition_type.has_value())
    {
        throw ty::type_error(condition->get_location(), "Condition has no type.");
    }

    if(condition_type.value() != ctx.get_i32_type())
    {
        throw ty::type_error(
          loc,
          std::format(
            "Expected while condition to be of type 'i32', got '{}",
            ctx.to_string(condition_type.value())));
    }

    while_block->type_check(ctx, env);
    return std::nullopt;
}

std::string while_statement::to_string() const
{
    return std::format(
      "While(condition={}, while_block={})",
      condition ? condition->to_string() : std::string("<none>"),
      while_block ? while_block->to_string() : std::string("<none>"));
}

/*
 * break_statement.
 */

std::unique_ptr<expression> break_statement::clone() const
{
    return std::make_unique<break_statement>(*this);
}

void break_statement::generate_code(
  cg::context& ctx) const
{
    auto [break_block, continue_block] = ctx.top_break_continue(loc);
    ctx.generate_branch(break_block);
}

/*
 * continue_statement.
 */

std::unique_ptr<expression> continue_statement::clone() const
{
    return std::make_unique<continue_statement>(*this);
}

void continue_statement::generate_code(
  cg::context& ctx) const
{
    auto [break_block, continue_block] = ctx.top_break_continue(loc);
    ctx.generate_branch(continue_block);
}

}    // namespace slang::ast
