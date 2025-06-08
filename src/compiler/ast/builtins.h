/**
 * slang - a simple scripting language.
 *
 * abstract syntax tree (built-ins).
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

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

/** Format string placeholder. */
struct format_string_placeholder
{
    /** Starting offset into the string. */
    std::size_t start{0};

    /** One past the ending offset into the string. */
    std::size_t end{0};

    /** Type: `{`, `}`, `d` (i32), `f` (f32) or `s` (str). */
    std::optional<std::uint8_t> type{std::nullopt};
};

/** Expansion helper for format macro. */
class format_macro_expander
{
    /** Location of the macro invokation. */
    token_location loc;

    /** Expressions the macro operates on. */
    const std::vector<std::unique_ptr<ast::expression>>& exprs;

    /** Format string. */
    const token& format_string;

    /** Format specifiers/placeholders. */
    std::vector<format_string_placeholder> placeholders;

    /**
     * Check and return the token holding the format string.
     *
     * @throws Throws a `codegen_error` if validation failed.
     * @returns Returns the token holding the format string.
     */
    const token& get_format_token();

    /**
     * Create the format string placeholders.
     *
     * @throws Throws a `codegen_error` when an invalid or unsupported
     *         format specifier was found.
     */
    void create_format_string_placeholders();

public:
    /** Deleted constructors. */
    format_macro_expander() = delete;
    format_macro_expander(const format_macro_expander&) = delete;
    format_macro_expander(format_macro_expander&&) = delete;

    /** Default destructor. */
    ~format_macro_expander() = default;

    /** Deleted assignments. */
    format_macro_expander& operator=(const format_macro_expander&) = delete;
    format_macro_expander& operator=(format_macro_expander&&) = delete;

    /**
     * Constructor.
     *
     * @param desc Descriptor for the `format!` macro.
     * @param loc Location of the macro invokation.
     * @param exprs Expressions the macro operates on.
     */
    format_macro_expander(
      token_location loc,
      const std::vector<std::unique_ptr<ast::expression>>& exprs)
    : loc{loc}
    , exprs{exprs}
    , format_string{get_format_token()}
    {
        create_format_string_placeholders();
    }

    /** Return the token containing the format string. */
    [[nodiscard]]
    const token& get_format_string() const
    {
        return format_string;
    }

    /** Return the format specifiers/placeholders. */
    [[nodiscard]]
    const std::vector<format_string_placeholder>& get_placeholders() const
    {
        return placeholders;
    }
};

/** Format macro AST for code generation. */
class format_macro_expression : public expression
{
    /** The argument expressions. */
    std::vector<std::unique_ptr<expression>> exprs;

    /** Format specifiers/placeholders. Set during type checking. */
    std::vector<format_string_placeholder> placeholders;

public:
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
    ~format_macro_expression() = default;

    /**
     * Construct a format! macro expression.
     *
     * @param loc The location.
     * @param exprs The argument expressions.
     */
    format_macro_expression(
      token_location loc,
      const std::vector<std::unique_ptr<expression>>& exprs)
    : expression{loc}
    {
        this->exprs.reserve(exprs.size());
        for(const auto& e: exprs)
        {
            this->exprs.emplace_back(e->clone());
        }
    }

    [[nodiscard]]
    node_identifier get_id() const override
    {
        return node_identifier::format_macro_expression;
    }

    std::unique_ptr<cg::value> generate_code(cg::context& ctx, memory_context mc = memory_context::none) const override;
    [[nodiscard]] std::optional<ty::type_info> type_check([[maybe_unused]] ty::context& ctx) override;
    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]]
    std::vector<expression*> get_children() override
    {
        std::vector<expression*> children;
        children.reserve(exprs.size());
        for(auto& e: exprs)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
    [[nodiscard]]
    std::vector<const expression*> get_children() const override
    {
        std::vector<const expression*> children;
        children.reserve(exprs.size());
        for(const auto& e: exprs)
        {
            children.emplace_back(e.get());
        }
        return children;
    }
};

}    // namespace slang::ast
