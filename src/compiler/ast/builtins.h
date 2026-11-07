/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (built-ins).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "compiler/typing.h"
#include "ast.h"

/*
 * Forward declarations.
 */

namespace slang::module_
{
struct macro_descriptor;
}    // namespace slang::module_

namespace slang::ast
{

/** Format macro AST placeholder for late expansion. */
class format_macro_expression : public expression
{
    /** The argument expressions. */
    std::vector<std::unique_ptr<expression>> exprs;

    /** Raw token streams for the macro arguments (fed to the token-stream builder). */
    std::vector<token_stream> arg_tokens;

public:
    /** Expansion. */
    std::unique_ptr<expression> expansion;
    /** Set the super class. */
    using super = expression;

    /** Defaulted and deleted constructors. */
    format_macro_expression() = default;
    format_macro_expression(const format_macro_expression&) = delete;
    format_macro_expression(format_macro_expression&&) = default;

    /** Defaulted and deleted assignment operators. */
    format_macro_expression& operator=(const format_macro_expression&) = delete;
    format_macro_expression& operator=(format_macro_expression&&) = delete;

    /** Default destructor. */
    ~format_macro_expression() override = default;

    /**
     * Construct a format! macro expression.
     *
     * @param loc The location.
     * @param exprs The argument expressions.
     * @param arg_tokens Raw token streams for the macro arguments.
     */
    format_macro_expression(
      source_location loc,
      const std::vector<std::unique_ptr<expression>>& exprs,
      std::vector<token_stream> arg_tokens = {})
    : expression{loc}
    , arg_tokens{std::move(arg_tokens)}
    {
        this->exprs =
          exprs
          | std::views::transform(
            [](const auto& e)
            {
                return e->clone();
            })
          | std::ranges::to<std::vector>();
    }

    [[nodiscard]]
    const std::vector<token_stream>& get_argument_tokens() const
    {
        return arg_tokens;
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::format_macro_expression;
    }

    std::unique_ptr<cg::rvalue> emit_rvalue(
      cg::context& ctx,
      bool result_used = false) const override;
    void collect_names(co::context& ctx) override;
    void resolve_names(rs::context& ctx) override;
    [[nodiscard]] std::optional<ty::type_id> type_check(
      [[maybe_unused]] ty::context& ctx,
      [[maybe_unused]] sema::env& env) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        return exprs
               | std::views::transform(
                 [](auto& e)
                 {
                     return e.get();
                 })
               | std::ranges::to<std::vector>();
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        return exprs
               | std::views::transform(
                 [](const auto& e) -> const expression*
                 {
                     return e.get();
                 })
               | std::ranges::to<std::vector>();
    }

    /**
     * Macro expansion (after type checking).
     *
     * @param ctx The type context.
     * @param env The semantic environment.
     */
    void expand_late_macros(
      ty::context& ctx,
      sema::env& env);
};

}    // namespace slang::ast
